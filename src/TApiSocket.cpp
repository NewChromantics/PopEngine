#include "TApiSocket.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoySocket.h"

namespace ApiSocket
{
	const char Namespace[] = "Pop.Socket";

	DEFINE_BIND_TYPENAME(UdpServer);
	DEFINE_BIND_TYPENAME(UdpClient);
	DEFINE_BIND_TYPENAME(TcpClient);
	DEFINE_BIND_TYPENAME(TcpServer);

	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpServer_GetAddress, GetAddress);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpServer_Send, Send);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpServer_GetPeers, GetPeers);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpServer_WaitForMessage, WaitForMessage);

	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_GetAddress, GetAddress);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_Send, Send);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_GetPeers, GetPeers);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_WaitForMessage, WaitForMessage);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(UdpClient_WaitForConnect, WaitForConnect);

	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpClient_GetAddress, GetAddress);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpClient_Send, Send);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpClient_GetPeers, GetPeers);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpClient_WaitForMessage, WaitForMessage);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpClient_WaitForConnect, WaitForConnect);

	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpServer_GetAddress, GetAddress);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpServer_Send, Send);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpServer_GetPeers, GetPeers);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(TcpServer_WaitForMessage, WaitForMessage);
}

void ApiSocket::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TUdpServerWrapper>(Namespace);
	Context.BindObjectType<TUdpClientWrapper>(Namespace);
	Context.BindObjectType<TTcpServerWrapper>(Namespace);
	Context.BindObjectType<TTcpClientWrapper>(Namespace);
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

void ApiSocket::TSocketClientWrapper::OnConnected()
{
	FlushPendingConnects();
}

void ApiSocket::TSocketClientWrapper::OnSocketClosed(const std::string& Reason)
{
	mClosedReason = Reason;
	if (mClosedReason.length() == 0)
		mClosedReason = "<Unspecified socket close reason>";

	FlushPendingMessages();
	FlushPendingConnects();
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
	auto SocketError = GetSocketError();
	if (!pMessage && SocketError.empty())
	{
		return;
	}

	auto Flush = [this, pMessage](Bind::TLocalContext& Context) mutable
	{		
		auto HandlePromise = [&](Bind::TLocalContext& LocalContext, Bind::TPromise& Promise)
		{
			//	send messages before errors
			if (pMessage)
			{
				auto MessageObject = PacketToObject(Context,*pMessage);
				Promise.Resolve(LocalContext, MessageObject);
			}
			else
			{
				auto Error = this->GetSocketError();
				Promise.Reject(LocalContext, Error);
			}
		};
		mOnMessagePromises.Flush(HandlePromise);
	};
	auto& Context = mOnMessagePromises.GetContext();
	Context.Queue(Flush);
}


void ApiSocket::TSocketClientWrapper::WaitForConnect(Bind::TCallback& Params)
{
	auto NewPromise = mOnConnectPromises.AddPromise(Params.mLocalContext);
	Params.Return(NewPromise);

	FlushPendingConnects();
}

void ApiSocket::TSocketClientWrapper::FlushPendingConnects()
{
	//	either no data, or no-one waiting yet
	if (!mOnConnectPromises.HasPromises())
		return;

	//	gotta wait for handshake to finish, so check for connected peers
	auto Socket = GetSocket();
	//	todo: Check for disconnection/handshake error
	auto ConnectionError = GetConnectionError();
	bool IsConnected = false;
	auto IsError = !ConnectionError.empty();
	if (!IsError)
	{
		//	check for connection
		BufferArray<SoyRef, 1> Peers;
		GetConnectedPeers(GetArrayBridge(Peers));
		IsConnected = !Peers.IsEmpty();
	}

	//	resolve when we're either connected or not
	if (!IsConnected && !IsError)
		return;

	auto Flush = [=](Bind::TLocalContext& Context)
	{
		if (IsConnected)
			mOnConnectPromises.Resolve();
		else//if IsError
			mOnConnectPromises.Reject(ConnectionError);
	};

	auto& Context = mOnConnectPromises.GetContext();
	Context.Queue(Flush);
}


void ApiSocket::TUdpServerWrapper::Construct(Bind::TCallback& Params)
{
	auto ListenPort = Params.GetArgumentInt(0);
	bool Broadcast = false;
	if ( !Params.IsArgumentUndefined(1) )
		Broadcast = Params.GetArgumentBool(1);
	
	auto OnBinaryMessage = [this](SoyRef Sender,const Array<uint8_t>& Message)
	{
		this->OnMessage( Message, Sender );
	};
	mSocket.reset( new TUdpServer(ListenPort, Broadcast, OnBinaryMessage ) );
}


void ApiSocket::TUdpServerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiSocket::BindFunction::UdpServer_GetAddress>( &ApiSocket::TSocketWrapper::GetAddress );
	Template.BindFunction<ApiSocket::BindFunction::UdpServer_Send>(&ApiSocket::TSocketWrapper::Send );
	Template.BindFunction<ApiSocket::BindFunction::UdpServer_GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiSocket::BindFunction::UdpServer_WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
}


void ApiSocket::TUdpClientWrapper::Construct(Bind::TCallback& Params)
{
	auto Hostname = Params.GetArgumentString(0);
	auto Port = Params.GetArgumentInt(1);
	
	auto OnBinaryMessage = [this](SoyRef Sender, const Array<uint8_t>& Message)
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
	mSocket.reset(new TSocketClient( TProtocol::Udp, Hostname, Port, OnBinaryMessage, OnConnected, OnDisconnected));
}


void ApiSocket::TUdpClientWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_GetAddress>(&ApiSocket::TSocketWrapper::GetAddress);
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_Send>(&ApiSocket::TSocketWrapper::Send);
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
	Template.BindFunction<ApiSocket::BindFunction::UdpClient_WaitForConnect>(&ApiSocket::TSocketClientWrapper::WaitForConnect);
}


void ApiSocket::TTcpClientWrapper::Construct(Bind::TCallback& Params)
{
	auto Hostname = Params.GetArgumentString(0);
	auto Port = Params.GetArgumentInt(1);

	auto OnBinaryMessage = [this](SoyRef Sender, const Array<uint8_t>& Message)
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
	mSocket.reset(new TSocketClient(TProtocol::Tcp, Hostname, Port, OnBinaryMessage, OnConnected, OnDisconnected));
}


void ApiSocket::TTcpClientWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiSocket::BindFunction::TcpClient_GetAddress>(&ApiSocket::TSocketWrapper::GetAddress);
	Template.BindFunction<ApiSocket::BindFunction::TcpClient_Send>(&ApiSocket::TSocketWrapper::Send);
	Template.BindFunction<ApiSocket::BindFunction::TcpClient_GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiSocket::BindFunction::TcpClient_WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
	Template.BindFunction<ApiSocket::BindFunction::TcpClient_WaitForConnect>(&ApiSocket::TSocketClientWrapper::WaitForConnect);
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
	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__, 2);

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
		//	gr: if you provide a u32 array, and we grab as u8,
		//		this code will re-interpret.
		//	we need GetArgumentRawArray() ?
		if (!Params.IsArgumentArrayU8(1))
			throw Soy::AssertException("Send 2nd argument is not a Uint8Array, this will get recast to bytes and lose data");
		//	gr: maybe we need to handle other types too?
		Params.GetArgumentArray(1, GetArrayBridge(Data));
	}

	auto& Socket = *ThisSocket;
	//	gr: flaw in the soy socket paradigm perhaps? the connection is away from the owner...
	//		so we need to manually tell the Socket when a client is error'd
	try
	{
		//	this can now throw if a non-existent connection
		//	for clients connecting to a server, this suggests the connection is already severed (eg in a recv)
		auto Connection = Socket.GetConnection( Sender );
		auto DataChars = GetArrayBridge(Data).GetSubArray<char>(0,Data.GetSize());

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



ApiSocket::TUdpServer::TUdpServer(uint16_t ListenPort,bool Broadcast,std::function<void(SoyRef,const Array<uint8_t>&)> OnBinaryMessage) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"UdpServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep ),
	mOnBinaryMessage	( OnBinaryMessage )
{
	mSocket.reset( new SoySocket() );
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
	

bool ApiSocket::TUdpServer::Iteration()
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
		
		this->mOnBinaryMessage( Sender, RecvBuffer8 );
	};
	mSocket->EnumConnections( RecvFromConnection );

	return true;
}



ApiSocket::TSocketClient::TSocketClient(TProtocol::TYPE Protocol,const std::string& Hostname,uint16_t Port, std::function<void(SoyRef,const Array<uint8_t>&)> OnBinaryMessage, std::function<void()> OnConnected, std::function<void(const std::string&)> OnDisconnected) :
	SoyWorkerThread		(Soy::StreamToString(std::stringstream() << "UdpClient(" << Hostname << ":" << Port << ")"), SoyWorkerWaitMode::Sleep),
	mOnBinaryMessage	(OnBinaryMessage),
	mOnConnected		(OnConnected),
	mOnDisconnected		(OnDisconnected)
{
	mSocket.reset(new SoySocket());

	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		//AddClient(ClientRef);
		if (this->mOnConnected)
			mOnConnected();
	};

	mSocket->mOnDisconnect = [=](SoyRef ClientRef, const std::string& Reason)
	{
		if (this->mOnDisconnected)
			mOnDisconnected(Reason);
	};

	if (Protocol == TProtocol::Tcp)
	{
		mSocket->CreateTcp();
		mSocket->Connect(Hostname.c_str(), Port);
	}
	else 
	{
		auto Broadcast = (Protocol == TProtocol::UdpBroadcast);
		mSocket->CreateUdp(Broadcast);
		mSocket->UdpConnect(Hostname.c_str(), Port);
	}

	Start();

}


bool ApiSocket::TSocketClient::Iteration()
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

			this->mOnBinaryMessage( Sender, RecvBuffer8 );
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


void ApiSocket::TTcpServerWrapper::Construct(Bind::TCallback& Params)
{
	auto ListenPort = Params.GetArgumentInt(0);

	auto OnBinaryMessage = [this](SoyRef Sender, const Array<uint8_t>& Message)
	{
		this->OnMessage(Message, Sender);
	};
	mSocket.reset(new TTcpServer(ListenPort, OnBinaryMessage));
}

void ApiSocket::TTcpServerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiSocket::BindFunction::TcpServer_GetAddress>(&ApiSocket::TSocketWrapper::GetAddress);
	Template.BindFunction<ApiSocket::BindFunction::TcpServer_Send>(&ApiSocket::TSocketWrapper::Send);
	Template.BindFunction<ApiSocket::BindFunction::TcpServer_GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiSocket::BindFunction::TcpServer_WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
}



void ApiSocket::TTcpServerWrapper::Send(Bind::TCallback& Params)
{
	auto ThisSocket = mSocket;
	if (!ThisSocket)
		throw Soy::AssertException("Socket not allocated");

	auto PeerStr = Params.GetArgumentString(0);
	auto PeerRef = SoyRef(PeerStr);

	Array<uint8_t> Data;
	Params.GetArgumentArray(1, GetArrayBridge(Data));
	ThisSocket->Send(PeerRef, GetArrayBridge(Data));
}




ApiSocket::TTcpServer::TTcpServer(uint16_t ListenPort, std::function<void(SoyRef,const Array<uint8_t>&)> OnBinaryMessage) :
	SoyWorkerThread(Soy::StreamToString(std::stringstream() << "TTcpServer(" << ListenPort << ")"), SoyWorkerWaitMode::Sleep),
	mOnBinaryMessage(OnBinaryMessage)
{
	mSocket.reset(new SoySocket());
	mSocket->CreateTcp(true);
	mSocket->ListenTcp(ListenPort);

	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		AddPeer(ClientRef);
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef, const std::string& Reason)
	{
		RemovePeer(ClientRef);
	};

	Start();

}


bool ApiSocket::TTcpServer::Iteration()
{
	if (!mSocket)
		return false;

	if (!mSocket->IsCreated())
		return true;

	//	non blocking so lets just do everything in a loop
	auto NewClient = mSocket->WaitForClient();
	if (NewClient.IsValid())
		std::Debug << "new client connected: " << NewClient << std::endl;
	/*
	Array<char> RecvBuffer;
	auto RecvFromConnection = [&](SoyRef ClientRef,SoySocketConnection ClientCon)
	{
		RecvBuffer.Clear();
		ClientCon.Recieve( GetArrayBridge(RecvBuffer) );
		if ( !RecvBuffer.IsEmpty() )
		{
			auto Client = GetClient(ClientRef);
			Client->OnRecvData( GetArrayBridge(RecvBuffer) );
		}
	};
	mSocket->EnumConnections( RecvFromConnection );
	*/
	return true;
}


std::shared_ptr<ApiSocket::TTcpServerPeer> ApiSocket::TTcpServer::GetPeer(SoyRef ClientRef)
{
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	for (int c = 0; c < mClients.GetSize(); c++)
	{
		auto& pClient = mClients[c];
		if (pClient->mConnectionRef == ClientRef)
			return pClient;
	}

	throw Soy::AssertException("Client not found");
}

void ApiSocket::TTcpServer::AddPeer(SoyRef ClientRef)
{
	std::shared_ptr<TTcpServerPeer> Client(new TTcpServerPeer(mSocket, ClientRef, mOnBinaryMessage));
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	mClients.PushBack(Client);
}

void ApiSocket::TTcpServer::RemovePeer(SoyRef ClientRef)
{

}

void ApiSocket::TTcpServer::Send(SoyRef ClientRef, const ArrayBridge<uint8_t>& Message)
{
	auto Peer = GetPeer(ClientRef);
	if (!Peer)
	{
		std::stringstream Error;
		Error << "No peer matching " << ClientRef;
		throw Soy::AssertException(Error.str());
	}
	Peer->Send(Message);
}


void ApiSocket::TTcpServerPeer::ClientConnect()
{
}

void ApiSocket::TTcpServerPeer::OnDataRecieved(std::shared_ptr<TAnythingProtocol>& pData)
{
	auto& Data = pData->mData;
	mOnBinaryMessage(this->mConnectionRef, Data);
}


std::shared_ptr<Soy::TReadProtocol> ApiSocket::TTcpServerPeer::AllocProtocol()
{
	return std::make_shared<TAnythingProtocol>();
}


void ApiSocket::TTcpServerPeer::Send(const ArrayBridge<uint8_t>& Message)
{
	auto Packet = std::make_shared<TAnythingProtocol>();
	Packet->mData.Copy(Message);
	Push(Packet);
}


