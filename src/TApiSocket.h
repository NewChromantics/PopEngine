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
	
	std::string					GetAddress() const;

protected:
	virtual bool				Iteration() override;
	
public:
	std::shared_ptr<SoySocket>		mSocket;
	
private:
	std::function<void(const Array<uint8_t>&,SoyRef)>	mOnBinaryMessage;
};



class TSocketWrapper
{
public:
	static v8::Local<v8::Value>			GetAddress(const v8::CallbackInfo& Arguments);

	virtual std::shared_ptr<SoySocket>	GetSocket()=0;
};

class TUdpBroadcastServerWrapper : public TSocketWrapper
{
public:
	TUdpBroadcastServerWrapper(uint16_t ListenPort);
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);

	//	queue up a callback for This handle's OnMessage callback
	void									OnMessage(const Array<uint8_t>& Message,SoyRef Peer);
	
	virtual std::shared_ptr<SoySocket>		GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

public:
	v8::Persistent<v8::Object>	mHandle;
	TV8Container*				mContainer;
	
	std::shared_ptr<TUdpBroadcastServer>	mSocket;
};

