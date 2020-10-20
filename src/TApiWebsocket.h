#pragma once
#include "TBind.h"
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


class TWebsocketServerPeer : public TSocketReadThread, TSocketWriteThread
{
public:
	TWebsocketServerPeer(std::shared_ptr<SoySocket>& Socket, SoyRef ConnectionRef, std::function<void(SoyRef, const std::string&)> OnTextMessage, std::function<void(SoyRef, const Array<uint8_t>&)> OnBinaryMessage);
	~TWebsocketServerPeer();

	void				ClientConnect(const std::string& Host);
	void				Stop(bool WaitToFinish);

	virtual void		OnDataRecieved(std::shared_ptr<Soy::TReadProtocol>& Data);
	
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

class TWebsocketClientPeer : public TWebsocketServerPeer
{
public:
	TWebsocketClientPeer(std::shared_ptr<SoySocket>& Socket, SoyRef ConnectionRef, std::function<void(SoyRef, const std::string&)> OnTextMessage, std::function<void(SoyRef, const Array<uint8_t>&)> OnBinaryMessage,std::function<void()> OnConnected);

	virtual void		OnDataRecieved(std::shared_ptr<Soy::TReadProtocol>& Data) override;

	virtual std::shared_ptr<Soy::TReadProtocol>	AllocProtocol() override;

public:
	std::function<void()>	mOnConnected;	//	handshake completed
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
	Array<std::shared_ptr<TWebsocketServerPeer>>	mDeadClients;

	std::function<void(SoyRef,const std::string&)>		mOnTextMessage;
	std::function<void(SoyRef,const Array<uint8_t>&)>	mOnBinaryMessage;
};



class TWebsocketClient : public SoyWorkerThread
{
public:
	TWebsocketClient(const std::string& Hostname,uint16_t Port, std::function<void(SoyRef, const std::string&)> OnTextMessage, std::function<void(SoyRef, const Array<uint8_t>&)> OnBinaryMessage);

	void						Send(SoyRef ClientRef, const std::string& Message);
	void						Send(SoyRef ClientRef, const ArrayBridge<uint8_t>& Message);

	SoySocket&					GetSocket() { return *mSocket; }

	//	get clients who have finished handshaking
	void						GetConnectedPeers(ArrayBridge<SoyRef>&& Clients)	{	GetConnectedPeers(Clients);	}
	void						GetConnectedPeers(ArrayBridge<SoyRef>& Clients);

protected:
	virtual bool				Iteration() override;

	void						AddPeer(SoyRef ClientRef);
	void						RemovePeer(SoyRef ClientRef);
	std::shared_ptr<TWebsocketServerPeer>	GetPeer(SoyRef ClientRef);

public:
	std::shared_ptr<SoySocket>		mSocket;

	//	this could be "on peer connected" to be generic
	std::function<void()>			mOnConnected;
	std::function<void()>			mOnDisconnected;

protected:
	std::shared_ptr<TWebsocketClientPeer>	mServerPeer;
	std::string								mServerHost;

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




class TWebsocketClientWrapper : public Bind::TObjectWrapper<ApiWebsocket::BindType::WebsocketClient, TWebsocketClient>, public ApiSocket::TSocketClientWrapper
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

public:
	std::shared_ptr<TWebsocketClient>	mSocket = mObject;
};






