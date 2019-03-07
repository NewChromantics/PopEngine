#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"

//#include "TPopServerThread.h"
#include "SoyRef.h"
#include "SoyStream.h"
#include "SoySocketStream.h"



class SoySocket;

namespace ApiSocket
{
	void	Bind(Bind::TContext& Context);
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
	static void		GetAddress(Bind::TCallback& Arguments);
	static void		Send(Bind::TCallback& Arguments);
	static void		GetPeers(Bind::TCallback& Arguments);

	virtual std::shared_ptr<SoySocket>	GetSocket()=0;
};

extern const char UdpBroadcastServer_TypeName[];
class TUdpBroadcastServerWrapper : public Bind::TObjectWrapper<UdpBroadcastServer_TypeName,TUdpBroadcastServer>, public TSocketWrapper
{
public:
	TUdpBroadcastServerWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper	( Context, This )
	{
	}
	//TUdpBroadcastServerWrapper(uint16_t ListenPort);
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void		Construct(Bind::TCallback& Params) override;

	//	queue up a callback for This handle's OnMessage callback
	void									OnMessage(const Array<uint8_t>& Message,SoyRef Peer);
	
	virtual std::shared_ptr<SoySocket>		GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

public:
	std::shared_ptr<TUdpBroadcastServer>	mSocket = mObject;
};

