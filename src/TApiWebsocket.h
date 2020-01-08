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
	DECLARE_BIND_TYPENAME(WebsocketClient);
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

	//	get clients who have finished handshaking
	void						GetConnectedPeers(ArrayBridge<SoyRef>& Clients);
	
protected:
	virtual bool				Iteration() override;
	
	void						AddPeer(SoyRef ClientRef);
	void						RemovePeer(SoyRef ClientRef);
	std::shared_ptr<TWebsocketServerPeer>	GetPeer(SoyRef ClientRef);
	
public:
	std::shared_ptr<SoySocket>		mSocket;
	
protected:
	std::recursive_mutex			mClientsLock;
	Array<std::shared_ptr<TWebsocketServerPeer>>	mClients;
	
	std::function<void(SoyRef,const std::string&)>		mOnTextMessage;
	std::function<void(SoyRef,const Array<uint8_t>&)>	mOnBinaryMessage;
};



class TWebsocketClient : public SoyWorkerThread
{
public:
	TWebsocketClient(const std::string& Address, std::function<void(SoyRef, const std::string&)> OnTextMessage, std::function<void(SoyRef, const Array<uint8_t>&)> OnBinaryMessage);

	void						Send(SoyRef ClientRef, const std::string& Message);
	void						Send(SoyRef ClientRef, const ArrayBridge<uint8_t>& Message);

	SoySocket&					GetSocket() { return *mSocket; }

	//	get clients who have finished handshaking
	void						GetConnectedPeers(ArrayBridge<SoyRef>& Clients);

protected:
	virtual bool				Iteration() override;

	void						AddPeer(SoyRef ClientRef);
	void						RemovePeer(SoyRef ClientRef);
	std::shared_ptr<TWebsocketServerPeer>	GetPeer(SoyRef ClientRef);

public:
	std::shared_ptr<SoySocket>		mSocket;

protected:
	std::shared_ptr<TWebsocketServerPeer>	mServerPeer;

	std::function<void(SoyRef, const std::string&)>		mOnTextMessage;
	std::function<void(SoyRef, const Array<uint8_t>&)>	mOnBinaryMessage;
};



	
class TWebsocketServerWrapper : public Bind::TObjectWrapper<ApiWebsocket::BindType::WebsocketServer,TWebsocketServer>, public ApiSocket::TSocketWrapper
{
public:
	TWebsocketServerWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void				CreateTemplate(Bind::TTemplate& Template);

	virtual void			Construct(Bind::TCallback& Params) override;
	virtual void			Send(Bind::TCallback& Params) override;
	
	virtual std::shared_ptr<SoySocket>		GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}
	virtual void			GetConnectedPeers(ArrayBridge<SoyRef>&& Peers) override;

public:
	std::shared_ptr<TWebsocketServer>	mSocket = mObject;
};




class TWebsocketClientWrapper : public Bind::TObjectWrapper<ApiWebsocket::BindType::WebsocketClient, TWebsocketClient>, public ApiSocket::TSocketWrapper
{
public:
	TWebsocketClientWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void				CreateTemplate(Bind::TTemplate& Template);

	virtual void			Construct(Bind::TCallback& Params) override;
	virtual void			Send(Bind::TCallback& Params) override;

	virtual std::shared_ptr<SoySocket>		GetSocket() override { return mSocket ? mSocket->mSocket : nullptr; }
	virtual void			GetConnectedPeers(ArrayBridge<SoyRef>&& Peers) override;

	void					WaitForConnect(Bind::TCallback& Params);

protected:
	void					OnConnected();
	void					FlushPendingConnects();

public:
	std::shared_ptr<TWebsocketClient>	mSocket = mObject;
	Bind::TPromiseQueue		mOnConnectPromises;
};






