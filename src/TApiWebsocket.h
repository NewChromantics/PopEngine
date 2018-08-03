#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"

//#include "TPopServerThread.h"
#include <SoyRef.h>
#include <SoyStream.h>
#include "SoyWebsocket.h"
#include "SoyHttp.h"
#include "SoySocketStream.h"



class SoySocket;

namespace ApiWebsocket
{
	void	Bind(TV8Container& Container);
}



class TClient : public TSocketReadThread_Impl<WebSocket::TRequestProtocol>, TSocketWriteThread
{
public:
	TClient(std::shared_ptr<SoySocket>& Socket,SoyRef Ref,std::function<void(const std::string&)> OnTextMessage,std::function<void(const Array<uint8_t>&)> OnBinaryMessage) :
		TSocketReadThread_Impl	( Socket, Ref ),
		TSocketWriteThread		( Socket, Ref ),
		mOnTextMessage			( OnTextMessage ),
		mOnBinaryMessage		( OnBinaryMessage )
	{
		TSocketReadThread_Impl::Start();
		TSocketWriteThread::Start();
	}

	virtual void		OnDataRecieved(std::shared_ptr<WebSocket::TRequestProtocol>& Data) override;
	
	virtual std::shared_ptr<Soy::TReadProtocol>	AllocProtocol() override;
	
public:
	std::function<void(const std::string&)>		mOnTextMessage;
	std::function<void(const Array<uint8_t>&)>	mOnBinaryMessage;
	WebSocket::THandshakeMeta					mHandshake;
	std::shared_ptr<WebSocket::TMessage>		mCurrentMessage;
};


class TWebsocketServer : public SoyWorkerThread
{
public:
	TWebsocketServer(uint16_t ListenPort);

protected:
	virtual bool				Iteration() override;
	
	void						AddClient(SoyRef ClientRef);
	void						RemoveClient(SoyRef ClientRef);
	std::shared_ptr<TClient>	GetClient(SoyRef ClientRef);
	
private:
	std::shared_ptr<SoySocket>		mSocket;
	
	std::recursive_mutex			mClientsLock;
	Array<std::shared_ptr<TClient>>	mClients;
};



class TWebsocketServerWrapper
{
public:
	TWebsocketServerWrapper(uint16_t ListenPort);
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	
public:
	v8::Persistent<v8::Object>	mHandle;
	TV8Container*				mContainer;

	std::shared_ptr<TWebsocketServer>	mSocket;
};

