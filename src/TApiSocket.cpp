#include "TApiSocket.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoySocket.h"

namespace ApiSocket
{
	const char Namespace[] = "Pop.Socket";

	DEFINE_BIND_TYPENAME(UdpBroadcastServer);
	DEFINE_BIND_TYPENAME(UdpClient);

	DEFINE_BIND_FUNCTIONNAME(GetAddress);
	DEFINE_BIND_FUNCTIONNAME(Send);
	DEFINE_BIND_FUNCTIONNAME(GetPeers);
	DEFINE_BIND_FUNCTIONNAME(WaitForMessage);

	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_GetAddress, GetAddress);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_Send,Send);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_GetPeers,GetPeers);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_WaitForMessage,WaitForMessage);
}

void ApiSocket::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TUdpBroadcastServerWrapper>(Namespace);
	Context.BindObjectType<TUdpClientWrapper>(Namespace);
}


void ApiSocket::TSocketWrapper::WaitForMessage(Bind::TCallback& Params)
{
	auto NewPromise = mOnMessagePromises.AddPromise(Params.mLocalContext);
	Params.Return(NewPromise);

	FlushPendingMessages();
}


void ApiSocket::TSocketWrapper::OnMessage(const Array<uint8_t>& Message, SoyRef Sender)
{
	auto Packet = std::make_shared<TBinaryPacket>();
	Packet->mData = Message;
	Packet->mPeer = Sender;

	{
		std::lock_guard<std::mutex> Lock(mMessagesLock);
		mMessages.PushBack(Packet);
	}
	FlushPendingMessages();
}


void ApiSocket::TSocketWrapper::OnMessage(const std::string& Message, SoyRef Sender)
{
	auto Packet = std::make_shared<TStringPacket>();
	Packet->mData = Message;
	Packet->mPeer = Sender;

	{
		std::lock_guard<std::mutex> Lock(mMessagesLock);
		mMessages.PushBack(Packet);
	}
	FlushPendingMessages();
}

void ApiSocket::TSocketWrapper::OnConnected()
{
	//	todo: generic connection promises
}

void ApiSocket::TSocketWrapper::OnSocketClosed(const std::string& Reason)
{
	mClosedReason = Reason;
	if (mClosedReason.length() == 0)
		mClosedReason = "<Unspecified socket close reason>";

	FlushPendingMessages();
}

Bind::TObject PacketToObject(Bind::TLocalContext& Context, ApiSocket::TPacket& Packet)
{
	auto Object = Context.mGlobalContext.CreateObjectInstance(Context);
	if (Packet.IsBinary())
		Object.SetArray("Data", GetArrayBridge(Packet.GetBinary()));
	else
		Object.SetString("Data", Packet.GetString());
	Object.SetString("Peer", Packet.mPeer.ToString());
	return Object;
}


void ApiSocket::TSocketWrapper::FlushPendingMessages()
{
	//	either no data, or no-one waiting yet
	if (!mOnMessagePromises.HasPromises())
		return;

	//	pop as many messages as possible before error
	std::shared_ptr<ApiSocket::TPacket> pMessage;
	if (!mMessages.IsEmpty())
	{
		std::lock_guard<std::mutex> Lock(mMessagesLock);
		pMessage = mMessages.PopAt(0);
	}

	//	no message? if no error either, nothing to do
	if (!pMessage && mClosedReason.empty())
	{
		return;
	}

	auto Flush = [this, pMessage](Bind::TLocalContext& Context) mutable
	{		
		auto HandlePromise = [&](Bind::TLocalContext& LocalContext, Bind::TPromise& Promise)
		{
			if (pMessage)
			{
				auto MessageObject = PacketToObject(Context,*pMessage);
				Promise.Resolve(LocalContext, MessageObject);
			}
			else
			{
				Promise.Reject(LocalContext,mClosedReason);
			}
		};
		mOnMessagePromises.Flush(HandlePromise);
	};
	auto& Context = mOnMessagePromises.GetContext();
	Context.Queue(Flush);
}



void TUdpBroadcastServerWrapper::Construct(Bind::TCallback& Params)
{
	//auto& This = Params.This<TUdpBroadcastServerWrapper>();

	auto ListenPort = Params.GetArgumentInt(0);

	
	auto OnBinaryMessage = [this](const Array<uint8_t>& Message,SoyRef Sender)
	{
		this->OnMessage( Message, Sender );
	};
	mSocket.reset( new TUdpBroadcastServer(ListenPort, OnBinaryMessage ) );
}


void TUdpBroadcastServerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiSocket::BindFunction::GetAddress>( &ApiSocket::TSocketWrapper::GetAddress );
	Template.BindFunction<ApiSocket::BindFunction::Send>(&ApiSocket::TSocketWrapper::Send );
	Template.BindFunction<ApiSocket::BindFunction::GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiSocket::BindFunction::WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
}


void TUdpClientWrapper::Construct(Bind::TCallback& Params)
{
	auto Hostname = Params.GetArgumentString(0);
	auto Port = Params.GetArgumentInt(1);
	
	auto OnBinaryMessage = [this](const Array<uint8_t>& Message, SoyRef Sender)
	{
		this->OnMessage(Message, Sender);
	};
	auto OnConnected = [this]()
	{
		this->OnConnected();
	};
	auto OnDisconnected = [this](const std::string& Reason)
	{
		this->OnSocketClosed(Reason);
	};
	mSocket.reset(new TUdpClient(Hostname, Port, OnBinaryMessage, OnConnected, OnDisconnected));
}


void TUdpClientWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_GetAddress>(&ApiSocket::TSocketWrapper::GetAddress);
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_Send>(&ApiSocket::TSocketWrapper::Send);
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
}




void ApiSocket::TSocketWrapper::GetAddress(Bind::TCallback& Params)
{
	auto& This = Params.This<TSocketWrapper>();
	auto ThisSocket = This.GetSocket();
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	//	we return all the addresses
	Array<Bind::TObject> Addresses;
	auto AppendAddress = [&](std::string& InterfaceName,SoySockAddr& InterfaceAddress)
	{
		std::stringstream AddressStr;
		AddressStr << InterfaceAddress;

		auto Address = Params.mContext.CreateObjectInstance( Params.mLocalContext );
		Address.SetString("Address", AddressStr.str());
		Address.SetString("Name", InterfaceName);
		Addresses.PushBack(Address);
	};
	ThisSocket->GetSocketAddresses( AppendAddress );
	
	Params.Return( GetArrayBridge(Addresses) );
}


void ApiSocket::TSocketWrapper::Send(Bind::TCallback& Params)
{
	auto ThisSocket = GetSocket();
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	auto SenderStr = Params.GetArgumentString(0);
	auto Sender = SoyRef( SenderStr );

	Array<uint8_t> Data;
	if (Params.IsArgumentString(1))
	{
		//	string to buffer
		auto DataString = Params.GetArgumentString(1);
		Data.Alloc(DataString.length());
		for (auto i = 0; i < DataString.length(); i++)
		{
			auto Char = DataString[i];
			auto Byte = *reinterpret_cast<uint8_t*>(&Char);
			Data.PushBack(Byte);
		}
	}
	else
	{
		//	gr: maybe we need to handle other types too?
		Params.GetArgumentArray(1, GetArrayBridge(Data));
	}

	auto& Socket = *ThisSocket;
	auto Connection = Socket.GetConnection( Sender );
	auto DataChars = GetArrayBridge(Data).GetSubArray<char>(0,Data.GetSize());

	//	gr: flaw in the soy socket paradigm perhaps? the connection is away from the owner...
	//		so we need to manually tell the Socket when a client is error'd
	try
	{
		Connection.Send(GetArrayBridge(DataChars), Socket.IsUdp());
	}
	catch (std::exception& e)
	{
		Socket.Disconnect(Sender, e.what());
		throw;
	}
}

void ApiSocket::TSocketWrapper::GetConnectedPeers(ArrayBridge<SoyRef>&& Peers)
{
	auto pSocket = this->GetSocket();
	auto& Socket = *pSocket;
	auto EnumPeer = [&](SoyRef ConnectionRef,SoySocketConnection Connection)
	{
		Peers.PushBack( ConnectionRef );
	};
	Socket.EnumConnections( EnumPeer );
}

void ApiSocket::TSocketWrapper::GetPeers(Bind::TCallback& Params)
{
	auto ThisSocket = GetSocket();
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	//	get connection references
	Array<SoyRef> Peers;
	GetConnectedPeers( GetArrayBridge(Peers) );
	
	Array<std::string> PeerNames;
	for ( auto i=0;	i<Peers.GetSize();	i++ )
	{
		auto PeerRef = Peers[i];
		PeerNames.PushBack( PeerRef.ToString() );
	}
	
	Params.Return( GetArrayBridge(PeerNames) );
}



TUdpBroadcastServer::TUdpBroadcastServer(uint16_t ListenPort,std::function<void(const Array<uint8_t>&,SoyRef)> OnBinaryMessage) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"UdpBroadcastServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep ),
	mOnBinaryMessage	( OnBinaryMessage )
{
	mSocket.reset( new SoySocket() );
	auto Broadcast = true;
	mSocket->CreateUdp(Broadcast);
	mSocket->ListenUdp(ListenPort,true);
	/*
	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		AddClient(ClientRef);
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef)
	{
		RemoveClient(ClientRef);
	};
	*/
	Start();
	
}
	

bool TUdpBroadcastServer::Iteration()
{
	if ( !mSocket )
		return false;
	
	if ( !mSocket->IsCreated() )
		return true;
	
	//	non blocking so lets just do everything in a loop
	Array<char> RecvBuffer;
	Array<uint8_t> RecvBuffer8;
	auto RecvFromConnection = [&](SoyRef ConnectionRef,SoySocketConnection Connection)
	{
		RecvBuffer.Clear();
		
		//	on udp ConnectionRef is us!
		auto Sender = Connection.Recieve( GetArrayBridge(RecvBuffer), *mSocket );
		if ( !Sender.IsValid() || RecvBuffer.IsEmpty() )
			return;
		
		//	cast buffer. Would prefer SoySocket to be uint8
		auto RecvBufferCastTo8 = GetArrayBridge(RecvBuffer).GetSubArray<uint8_t>( 0, RecvBuffer.GetSize() );
		RecvBuffer8.Copy( RecvBufferCastTo8 );
		
		this->mOnBinaryMessage( RecvBuffer8, Sender );
	};
	mSocket->EnumConnections( RecvFromConnection );

	return true;
}



TUdpClient::TUdpClient(const std::string& Hostname,uint16_t Port, std::function<void(const Array<uint8_t>&, SoyRef)> OnBinaryMessage, std::function<void()> OnConnected, std::function<void(const std::string&)> OnDisconnected) :
	SoyWorkerThread		(Soy::StreamToString(std::stringstream() << "UdpClient(" << Hostname << ":" << Port << ")"), SoyWorkerWaitMode::Sleep),
	mOnBinaryMessage	(OnBinaryMessage),
	mOnConnected		(mOnConnected),
	mOnDisconnected		(OnDisconnected)
{
	mSocket.reset(new SoySocket());
	auto Broadcast = false;
	mSocket->CreateUdp(Broadcast);
	mSocket->UdpConnect(Hostname.c_str(),Port);
	
	
	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		//AddClient(ClientRef);
		if (this->mOnConnected)
			mOnConnected();
	};
	
	mSocket->mOnDisconnect = [=](SoyRef ClientRef,const std::string& Reason)
	{
		if (this->mOnDisconnected)
			mOnDisconnected(Reason);
	};
	Start();

}


bool TUdpClient::Iteration()
{
	if (!mSocket)
		return false;

	if (!mSocket->IsCreated())
		return true;

	//	non blocking so lets just do everything in a loop
	Array<char> RecvBuffer;
	Array<uint8_t> RecvBuffer8;
	auto RecvFromConnection = [&](SoyRef ConnectionRef, SoySocketConnection Connection)
	{
		RecvBuffer.Clear();

		try
		{
			//	on udp ConnectionRef is us!
			auto Sender = Connection.Recieve(GetArrayBridge(RecvBuffer), *mSocket);
			if (!Sender.IsValid() || RecvBuffer.IsEmpty())
				return;
			
			//	cast buffer. Would prefer SoySocket to be uint8
			auto RecvBufferCastTo8 = GetArrayBridge(RecvBuffer).GetSubArray<uint8_t>(0, RecvBuffer.GetSize());
			RecvBuffer8.Copy(RecvBufferCastTo8);

			this->mOnBinaryMessage(RecvBuffer8, Sender);
		}
		catch (std::exception& e)
		{
			mSocket->Disconnect(ConnectionRef, e.what());
			throw;
		}
	};

	//	with udp CLIENT, if we get an error, we should abort the thread (assume disconnected/unreachable)
	try
	{
		mSocket->EnumConnections(RecvFromConnection);
	}
	catch (std::exception& e)
	{
		//	don't kill this thread on recv error, disconnect and end thread (let thread go until parent kills this?)
		std::Debug << "UDP enum&recv exception: " << e.what() << ". Killing thread" << std::endl;
		return false;
	}

	//	gr: if we recieved nothing, sleep?
	//		as this is udp client, maybe it should be blocking instead of sleeping
	return true;
}
