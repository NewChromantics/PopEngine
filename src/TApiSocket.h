#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"

//#include "TPopServerThread.h"
#include <SoyRef.h>
#include <SoyStream.h>
#include "SoySocketStream.h"



class SoySocket;

namespace ApiSocket
{
	void	Bind(TV8Container& Container);
}



class TUdpBroadcastServer : public SoyWorkerThread
{
public:
	TUdpBroadcastServer(uint16_t ListenPort,std::function<void(const Array<uint8_t>&,SoyRef)> OnBinaryMessage);
	
protected:
	virtual bool				Iteration() override;
	
private:
	std::shared_ptr<SoySocket>		mSocket;
	
	std::function<void(const Array<uint8_t>&,SoyRef)>	mOnBinaryMessage;
};




class TUdpBroadcastServerWrapper
{
public:
	TUdpBroadcastServerWrapper(uint16_t ListenPort);
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	
	//	queue up a callback for This handle's OnMessage callback
	void						OnMessage(const Array<uint8_t>& Message,SoyRef Peer);
	
public:
	v8::Persistent<v8::Object>	mHandle;
	TV8Container*				mContainer;
	
	std::shared_ptr<TUdpBroadcastServer>	mSocket;
};

