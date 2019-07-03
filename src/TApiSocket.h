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
	DECLARE_BIND_TYPENAME(UdpBroadcastServer);
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


class TUdpBroadcastServerWrapper : public Bind::TObjectWrapper<ApiSocket::BindType::UdpBroadcastServer,TUdpBroadcastServer>, public TSocketWrapper
{
public:
	TUdpBroadcastServerWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void		Construct(Bind::TCallback& Params) override;

	//	queue up a callback for This handle's OnMessage callback
	void									OnMessage(const Array<uint8_t>& Message,SoyRef Peer);
	
	virtual std::shared_ptr<SoySocket>		GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

public:
	std::shared_ptr<TUdpBroadcastServer>	mSocket = mObject;
};

