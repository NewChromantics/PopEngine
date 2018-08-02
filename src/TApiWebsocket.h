#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"

//#include "TPopServerThread.h"
#include <SoyRef.h>

class SoySocket;

namespace ApiWebsocket
{
	void	Bind(TV8Container& Container);
}



class TClient
{
public:
	TClient(SoyRef Ref) :
		mRef	( Ref )
	{
	}
	
	void		OnRecvData(ArrayBridge<char>&& Data);
	
	bool		operator==(const SoyRef& ThatRef) const	{	return ThatRef == this->mRef;	}

public:
	SoyRef		mRef;
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

