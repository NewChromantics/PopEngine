#pragma once
#include "TBind.h"
#include "SoyRef.h"
#include "SoyStream.h"
#include "SoySocketStream.h"
#include "SoyRingArray.h"



class SoySocket;

namespace ApiSocket
{
	void	Bind(Bind::TContext& Context);
	DECLARE_BIND_TYPENAME(UdpServer);
	DECLARE_BIND_TYPENAME(UdpClient);
	DECLARE_BIND_TYPENAME(TcpClient);
	DECLARE_BIND_TYPENAME(TcpServer);

	class TSocketWrapper;
	class TSocketClientWrapper;
	class TUdpServerWrapper;
	class TUdpClientWrapper;
	class TTcpClientWrapper;
	class TTcpServerWrapper;

	//	these could be generic things outside API
	class TSocketClient;		//	TCP & UDP share code
	class TUdpServer;
	class TTcpServer;
	class TTcpServerPeer;

	namespace TProtocol
	{
		enum TYPE
		{
			Tcp,
			Udp,
			UdpBroadcast,
		};
	};
}


class ApiSocket::TUdpServer : public SoyWorkerThread
{
public:
	TUdpServer(uint16_t ListenPort,bool IsBroadcast,std::function<void(SoyRef,const ArrayBridge<uint8_t>&&)> OnBinaryMessage);
	
	std::string					GetAddress() const;

protected:
	virtual bool				Iteration() override;
	
public:
	std::shared_ptr<SoySocket>		mSocket;
	
private:
	std::function<void(SoyRef,const ArrayBridge<uint8_t>&&)>	mOnBinaryMessage;
};


class ApiSocket::TSocketClient : public SoyWorkerThread
{
public:
	TSocketClient(TProtocol::TYPE Protocol,const std::string& Hostname,uint16_t Port, std::function<void(SoyRef,const ArrayBridge<uint8_t>&&)> OnBinaryMessage,std::function<void()> OnConnected, std::function<void(const std::string&)> OnDisconnected);

	std::string					GetAddress() const;

protected:
	virtual bool				Iteration() override;

public:
	std::shared_ptr<SoySocket>		mSocket;

private:
	std::function<void(SoyRef,const ArrayBridge<uint8_t>&&)>	mOnBinaryMessage;
	std::function<void(const std::string&)>				mOnDisconnected;
	std::function<void()>								mOnConnected;
};


class TPacketMeta
{
public:
	SoyRef		mPeer;
	size_t		mSize = 0;
	bool		mIsString = false;
};


//	giant FIFO packet storage
//	it stores one giant (ring)buffer of data and packet meta (which remember how much data each packet holds)
//	this is kinda delicate, removing any data without removing the packet (or vice versa) will break the correlation
class TPacketStorage
{
public:
	void				Push(SoyRef Peer,const std::string& Data);
	void				Push(SoyRef Peer,const ArrayBridge<uint8_t>& Data);

	//	peek to see if there is a packet, and if its a string
	bool				Peek(TPacketMeta& Meta);
	void				Pop(SoyRef& Peer,std::string& Data);
	void				Pop(SoyRef& Peer,ArrayBridge<uint8_t>&& Data);

private:
	std::mutex			mPacketLock;
	Array<TPacketMeta>	mPackets;			//	maybe we can store this in the data buffer too to avoid lots of resizes
	Array<uint8_t>		mDataBuffer;
};



//	gr: this needs a OnDisconnected so a client socket can reject() WaitForMessage when its disconnected from server
//		this should also implement WaitForConnect() (see websocketclientwrapper)
class ApiSocket::TSocketWrapper
{
	//	can't be protected when getting member pointers with clang
public:
	void			GetAddress(Bind::TCallback& Arguments);
	virtual void	Send(Bind::TCallback& Arguments);
	void			GetPeers(Bind::TCallback& Arguments);
	virtual void	GetConnectedPeers(ArrayBridge<SoyRef>&& Peers);
	virtual void	Disconnect(Bind::TCallback& Arguments);		//	todo: overload so explicit peers can be kicked

	//	get a promise for next message
	void			WaitForMessage(Bind::TCallback& Params);
	
protected:
	virtual std::shared_ptr<SoySocket>	GetSocket()=0;
	
	//	queue up a callback for This handle's OnMessage callback
	void		OnMessage(const ArrayBridge<uint8_t>&& Message, SoyRef Peer)	{	OnMessage(Message,Peer);	};
	void		OnMessage(const ArrayBridge<uint8_t>& Message, SoyRef Peer);
	void		OnMessage(const std::string& Message, SoyRef Peer);

	void		FlushPendingMessages();

	virtual std::string	GetSocketError()  { return std::string(); }	//	if set, then pending messages will error with this

private:
	Bind::TPromiseQueue		mOnMessagePromises;
	
	//	pending packets
	TPacketStorage			mMessages;
};

class ApiSocket::TSocketClientWrapper : public TSocketWrapper
{
public:
	//	get a promise for when connected
	void					WaitForConnect(Bind::TCallback& Params);
	virtual std::string		GetConnectionError() { return mClosedReason; }

protected:
	//	gr: these should only be called on client wrappers? as they have no peer refs
	void					OnConnected();
	void					OnSocketClosed(const std::string& Reason);

	void					FlushPendingConnects();
	virtual std::string		GetSocketError() override { return mClosedReason; }	//	if set, then pending messages will error with this

private:
	std::string				mClosedReason;		//	if this is set, messages/connection promises fail as socket is closed. Even UDP sockets can die!
	Bind::TPromiseQueue		mOnConnectPromises;
};

class ApiSocket::TUdpServerWrapper : public Bind::TObjectWrapper<ApiSocket::BindType::UdpServer,TUdpServer>, public ApiSocket::TSocketWrapper
{
public:
	TUdpServerWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void		Construct(Bind::TCallback& Params) override;

	virtual std::shared_ptr<SoySocket>		GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

public:
	std::shared_ptr<TUdpServer>	mSocket = mObject;
};



class ApiSocket::TUdpClientWrapper : public Bind::TObjectWrapper<BindType::UdpClient, TSocketClient>, public TSocketClientWrapper
{
public:
	TUdpClientWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void		Construct(Bind::TCallback& Params) override;

	virtual std::shared_ptr<SoySocket>		GetSocket() override { return mSocket ? mSocket->mSocket : nullptr; }

public:
	std::shared_ptr<TSocketClient>	mSocket = mObject;
};

class ApiSocket::TTcpClientWrapper : public Bind::TObjectWrapper<ApiSocket::BindType::TcpClient, TSocketClient>, public ApiSocket::TSocketClientWrapper
{
public:
	TTcpClientWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void		Construct(Bind::TCallback& Params) override;

	virtual std::shared_ptr<SoySocket>		GetSocket() override { return mSocket ? mSocket->mSocket : nullptr; }

public:
	std::shared_ptr<TSocketClient>	mSocket = mObject;
};


class TAnythingProtocol : public Soy::TReadProtocol, public Soy::TWriteProtocol
{
protected:
	virtual void					Encode(TStreamBuffer& Buffer) override
	{
		Buffer.Push(GetArrayBridge(mData));
	}
	virtual TProtocolState::Type	Decode(TStreamBuffer& Buffer) override
	{
		//	needs to wait for >0 data?
		auto Length = Buffer.GetBufferedSize();
		Buffer.Pop(Length, GetArrayBridge(mData));
		return TProtocolState::Finished;
	}

public:
	Array<uint8_t>					mData;
};


class ApiSocket::TTcpServerPeer : public TSocketReadThread_Impl<TAnythingProtocol>, TSocketWriteThread
{
public:
	TTcpServerPeer(std::shared_ptr<SoySocket>& Socket, SoyRef ConnectionRef, std::function<void(SoyRef, const ArrayBridge<uint8_t>&&)> OnBinaryMessage) :
		TSocketReadThread_Impl(Socket, ConnectionRef),
		TSocketWriteThread(Socket, ConnectionRef),
		mOnBinaryMessage(OnBinaryMessage),
		mConnectionRef(ConnectionRef)
	{
		TSocketReadThread_Impl::Start();
		TSocketWriteThread::Start();
	}

	void				ClientConnect();

	virtual void		OnDataRecieved(std::shared_ptr<TAnythingProtocol>& Data) override;

	virtual std::shared_ptr<Soy::TReadProtocol>	AllocProtocol() override;

	void				Send(const std::string& Message);
	void				Send(const ArrayBridge<uint8_t>& Message);

public:
	SoyRef										mConnectionRef;
	std::function<void(SoyRef, const ArrayBridge<uint8_t>&&)>	mOnBinaryMessage;

	std::recursive_mutex						mMessagesLock;
	Array<Array<uint8_t>>						mMessages;		//	gr: woah this is gonna be slow!
};



class ApiSocket::TTcpServer : public SoyWorkerThread
{
public:
	TTcpServer(uint16_t ListenPort, std::function<void(SoyRef,const ArrayBridge<uint8_t>&&)> OnBinaryMessage);
	void						Send(SoyRef ClientRef, const ArrayBridge<uint8_t>&& Message);

	SoySocket&					GetSocket() { return *mSocket; }

protected:
	virtual bool				Iteration() override;

	void						AddPeer(SoyRef ClientRef);
	void						RemovePeer(SoyRef ClientRef);
	std::shared_ptr<TTcpServerPeer>	GetPeer(SoyRef ClientRef);

public:
	std::shared_ptr<SoySocket>		mSocket;

protected:
	std::recursive_mutex			mClientsLock;
	Array<std::shared_ptr<TTcpServerPeer>>	mClients;

	std::function<void(SoyRef,const ArrayBridge<uint8_t>&&)>	mOnBinaryMessage;
};


class ApiSocket::TTcpServerWrapper : public Bind::TObjectWrapper<BindType::TcpServer, TTcpServer>, public ApiSocket::TSocketWrapper
{
public:
	TTcpServerWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void				CreateTemplate(Bind::TTemplate& Template);

	virtual void			Construct(Bind::TCallback& Params) override;
	virtual void			Send(Bind::TCallback& Params) override;

	virtual std::shared_ptr<SoySocket>		GetSocket() override { return mSocket ? mSocket->mSocket : nullptr; }

public:
	std::shared_ptr<TTcpServer>	mSocket = mObject;
};


