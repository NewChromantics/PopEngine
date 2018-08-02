#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"

//#include "TPopServerThread.h"


class SoySocket;

namespace ApiWebsocket
{
	void	Bind(TV8Container& Container);
}




class TWebsocketServer : public SoyWorkerThread
{
public:
	TWebsocketServer(uint16_t ListenPort);

protected:
	virtual bool		Iteration() override;
	
private:
	std::shared_ptr<SoySocket>	mSocket;
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

