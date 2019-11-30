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

	class TPacket;
	class TBinaryPacket;
	class TStringPacket;
	class TSocketWrapper;
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


class ApiSocket::TPacket
{
public:
	virtual bool			IsBinary() = 0;
	virtual Array<uint8_t>&	GetBinary() { throw Soy::AssertException("Not a binary packet"); }
	virtual std::string&	GetString() { throw Soy::AssertException("Not a string packet"); }

	SoyRef			mPeer;
};

class ApiSocket::TBinaryPacket : public TPacket
{
public:
	virtual bool			IsBinary() override {	return true;	}
	virtual Array<uint8_t>&	GetBinary() override { return mData; }

	Array<uint8_t>	mData;
};

class ApiSocket::TStringPacket : public TPacket
{
public:
	virtual bool			IsBinary() override { return true; }
	virtual std::string&	GetString() override { return mData; }

	std::string		mData;
};

class ApiSocket::TSocketWrapper
{
protected:
	void		GetAddress(Bind::TCallback& Arguments);
	void		Send(Bind::TCallback& Arguments);
	void		GetPeers(Bind::TCallback& Arguments);

	virtual std::shared_ptr<SoySocket>	GetSocket()=0;
	

	//	get a promise for next message
	void		WaitForMessage(Bind::TCallback& Params);

	//	queue up a callback for This handle's OnMessage callback
	void		OnMessage(const Array<uint8_t>& Message, SoyRef Peer);
	void		OnMessage(const std::string& Message, SoyRef Peer);
	void		FlushPendingMessages();

private:
	Bind::TPromiseQueue					mOnMessagePromises;
	//	pending packets
	std::mutex							mMessagesLock;
	//	these need ordering!
	Array<std::shared_ptr<ApiSocket::TPacket>>		mMessages;
};



class TUdpBroadcastServerWrapper : public Bind::TObjectWrapper<ApiSocket::BindType::UdpBroadcastServer,TUdpBroadcastServer>, public ApiSocket::TSocketWrapper
{
public:
	TUdpBroadcastServerWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void		Construct(Bind::TCallback& Params) override;

	virtual std::shared_ptr<SoySocket>		GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

public:
	std::shared_ptr<TUdpBroadcastServer>	mSocket = mObject;

};

