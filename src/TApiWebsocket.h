#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"

//#include "TPopServerThread.h"
#include <SoyRef.h>
#include <SoyStream.h>
#include "SoyWebsocket.h"
#include "SoyHttp.h"
#include "SoySocketStream.h"
#include "TApiSocket.h"



class SoySocket;

namespace ApiWebsocket
{
	void	Bind(TV8Container& Container);
}


//	client connected to us
class TWebsocketServerPeer : public TSocketReadThread_Impl<WebSocket::TRequestProtocol>, TSocketWriteThread
{
public:
	TWebsocketServerPeer(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef,std::function<void(const std::string&)> OnTextMessage,std::function<void(const Array<uint8_t>&)> OnBinaryMessage) :
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
	std::function<void(const std::string&)>		mOnTextMessage;
	std::function<void(const Array<uint8_t>&)>	mOnBinaryMessage;
	WebSocket::THandshakeMeta					mHandshake;				//	handshake probably doesn't need a lock as its only modified by packets
	//	current message gets reset & used & allocated on different threads though
	//	I think it may need an id so it doesnt get passed along to a packet WHILST we finish decoding the last one? (recv is serial though...)
	std::recursive_mutex						mCurrentMessageLock;
	std::shared_ptr<WebSocket::TMessageBuffer>	mCurrentMessage;
};


class TWebsocketServer : public SoyWorkerThread
{
public:
	TWebsocketServer(uint16_t ListenPort,std::function<void(const std::string&)> OnTextMessage,std::function<void(const Array<uint8_t>&)> OnBinaryMessage);

	std::string					GetAddress() const;

	void						Send(SoyRef ClientRef,const std::string& Message);
	void						Send(SoyRef ClientRef,const ArrayBridge<uint8_t>& Message);

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
	
	std::function<void(const std::string&)>		mOnTextMessage;
	std::function<void(const Array<uint8_t>&)>	mOnBinaryMessage;
};



class TWebsocketServerWrapper : public TSocketWrapper
{
public:
	TWebsocketServerWrapper(uint16_t ListenPort);
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static v8::Local<v8::Value>				Send(const v8::CallbackInfo& Arguments);

	//	queue up a callback for This handle's OnMessage callback
	void									OnMessage(const std::string& Message);
	void									OnMessage(const Array<uint8_t>& Message);
	
	virtual std::shared_ptr<SoySocket>		GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

public:
	v8::Persistent<v8::Object>	mHandle;
	TV8Container*				mContainer;

	std::shared_ptr<TWebsocketServer>	mSocket;
};






