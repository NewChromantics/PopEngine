#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"

//#include "TPopServerThread.h"
#include "SoyRef.h"
#include "SoyStream.h"
#include "SoyWebSocket.h"
#include "SoyHttp.h"
#include "SoySocketStream.h"
#include "TApiSocket.h"



class SoySocket;

namespace ApiWebsocket
{
	void	Bind(Bind::TContext& Context);
	DECLARE_BIND_TYPENAME(WebsocketServer);
}


//	client connected to us
class TWebsocketServerPeer : public TSocketReadThread_Impl<WebSocket::TRequestProtocol>, TSocketWriteThread
{
public:
	TWebsocketServerPeer(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef,std::function<void(SoyRef,const std::string&)> OnTextMessage,std::function<void(SoyRef,const Array<uint8_t>&)> OnBinaryMessage) :
		TSocketReadThread_Impl	( Socket, ConnectionRef ),
		TSocketWriteThread		( Socket, ConnectionRef ),
		mOnTextMessage			( OnTextMessage ),
		mOnBinaryMessage		( OnBinaryMessage ),
		mConnectionRef			( ConnectionRef )
	{
		TSocketReadThread_Impl::Start();
		TSocketWriteThread::Start();
	}

	virtual void		OnDataRecieved(std::shared_ptr<WebSocket::TRequestProtocol>& Data) override;
	
	virtual std::shared_ptr<Soy::TReadProtocol>	AllocProtocol() override;

	void				Send(const std::string& Message);
	void				Send(const ArrayBridge<uint8_t>& Message);
	
public:
	SoyRef										mConnectionRef;
	std::function<void(SoyRef,const std::string&)>		mOnTextMessage;
	std::function<void(SoyRef,const Array<uint8_t>&)>	mOnBinaryMessage;
	WebSocket::THandshakeMeta					mHandshake;				//	handshake probably doesn't need a lock as its only modified by packets
	//	current message gets reset & used & allocated on different threads though
	//	I think it may need an id so it doesnt get passed along to a packet WHILST we finish decoding the last one? (recv is serial though...)
	std::recursive_mutex						mCurrentMessageLock;
	std::shared_ptr<WebSocket::TMessageBuffer>	mCurrentMessage;
};


class TWebsocketServer : public SoyWorkerThread
{
public:
	TWebsocketServer(uint16_t ListenPort,std::function<void(SoyRef,const std::string&)> OnTextMessage,std::function<void(SoyRef,const Array<uint8_t>&)> OnBinaryMessage);

	void						Send(SoyRef ClientRef,const std::string& Message);
	void						Send(SoyRef ClientRef,const ArrayBridge<uint8_t>& Message);

	SoySocket&					GetSocket()		{	return *mSocket;	}

protected:
	virtual bool				Iteration() override;
	
	void						AddClient(SoyRef ClientRef);
	void						RemoveClient(SoyRef ClientRef);
	std::shared_ptr<TWebsocketServerPeer>	GetClient(SoyRef ClientRef);
	
public:
	std::shared_ptr<SoySocket>		mSocket;
	
protected:
	std::recursive_mutex			mClientsLock;
	Array<std::shared_ptr<TWebsocketServerPeer>>	mClients;
	
	std::function<void(SoyRef,const std::string&)>		mOnTextMessage;
	std::function<void(SoyRef,const Array<uint8_t>&)>	mOnBinaryMessage;
};



	
class TWebsocketServerWrapper : public Bind::TObjectWrapper<ApiWebsocket::WebsocketServer_TypeName,TWebsocketServer>, public TSocketWrapper
{
public:
	TWebsocketServerWrapper(Bind::TLocalContext& Context,Bind::TObject& This) :
		TObjectWrapper	( Context, This )
	{
	}
	
	static void				CreateTemplate(Bind::TTemplate& Template);

	virtual void			Construct(Bind::TCallback& Params) override;
	static void				Send(Bind::TCallback& Params);

	//	queue up a callback for This handle's OnMessage callback
	void					OnMessage(const std::string& Message);
	void					OnMessage(const Array<uint8_t>& Message);
	
	virtual std::shared_ptr<SoySocket>		GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

public:
	std::shared_ptr<TWebsocketServer>	mSocket = mObject;
};






