#include "TApiWebsocket.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoySocket.h"

namespace ApiWebsocket
{
	const char Namespace[] = "Pop.Websocket";

	DEFINE_BIND_TYPENAME(WebsocketServer);
	DEFINE_BIND_TYPENAME(WebsocketClient);
	DEFINE_BIND_FUNCTIONNAME(GetAddress);
	DEFINE_BIND_FUNCTIONNAME(Send);
	DEFINE_BIND_FUNCTIONNAME(GetPeers);
	DEFINE_BIND_FUNCTIONNAME(WaitForMessage);
	DEFINE_BIND_FUNCTIONNAME(WaitForConnect);

	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_GetAddress, GetAddress);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_Send, Send);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_GetPeers, GetPeers);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_WaitForMessage, WaitForMessage);
}

void ApiWebsocket::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TWebsocketServerWrapper>(Namespace, "Server");
	Context.BindObjectType<TWebsocketClientWrapper>(Namespace, "Client");
}



void TWebsocketServerWrapper::Construct(Bind::TCallback &Params)
{
	auto ListenPort = Params.GetArgumentInt(0);
	
	auto OnTextMessage = [this](SoyRef Connection,const std::string& Message)
	{
		this->OnMessage( Message, Connection);
	};
	
	auto OnBinaryMessage = [this](SoyRef Connection,const Array<uint8_t>& Message)
	{
		this->OnMessage( Message, Connection);
	};
	
	mSocket.reset( new TWebsocketServer( ListenPort, OnTextMessage, OnBinaryMessage ) );
}


void TWebsocketServerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiWebsocket::BindFunction::Server_GetAddress>( &ApiSocket::TSocketWrapper::GetAddress );
	Template.BindFunction<ApiWebsocket::BindFunction::Server_Send>(&ApiSocket::TSocketWrapper::Send );
	Template.BindFunction<ApiWebsocket::BindFunction::Server_GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiWebsocket::BindFunction::Server_WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
}


void TWebsocketServerWrapper::GetConnectedPeers(ArrayBridge<SoyRef>&& Peers)
{
	if ( !mSocket )
		return;
	
	//	get clients who have finished handshaking
	mSocket->GetConnectedPeers( Peers );
}


void TWebsocketServerWrapper::Send(Bind::TCallback& Params)
{
	auto ThisSocket = mSocket;
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	auto PeerStr = Params.GetArgumentString(0);
	auto PeerRef = SoyRef( PeerStr );

	if ( Params.IsArgumentString(1) )
	{
		auto DataString = Params.GetArgumentString(1);
		ThisSocket->Send( PeerRef, DataString );
	}
	else
	{
		Array<uint8_t> Data;
		Params.GetArgumentArray(1, GetArrayBridge(Data) );
		ThisSocket->Send( PeerRef, GetArrayBridge(Data) );
	}
}



void TWebsocketClientWrapper::Construct(Bind::TCallback &Params)
{
	auto Address = Params.GetArgumentString(0);

	auto OnTextMessage = [this](SoyRef Connection, const std::string& Message)
	{
		this->OnMessage(Message, Connection);
	};

	auto OnBinaryMessage = [this](SoyRef Connection, const Array<uint8_t>& Message)
	{
		this->OnMessage(Message, Connection);
	};
	
	mSocket.reset(new TWebsocketClient(Address, OnTextMessage, OnBinaryMessage));
	mSocket->mOnConnected = std::bind(&TWebsocketClientWrapper::FlushPendingConnects, this);
	mSocket->mOnDisconnected = std::bind(&TWebsocketClientWrapper::FlushPendingConnects, this);
}


void TWebsocketClientWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiWebsocket::BindFunction::GetAddress>(&ApiSocket::TSocketWrapper::GetAddress);
	Template.BindFunction<ApiWebsocket::BindFunction::Send>(&ApiSocket::TSocketWrapper::Send);
	Template.BindFunction<ApiWebsocket::BindFunction::GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiWebsocket::BindFunction::WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
	Template.BindFunction<ApiWebsocket::BindFunction::WaitForConnect>(&TWebsocketClientWrapper::WaitForConnect);
}


void TWebsocketClientWrapper::GetConnectedPeers(ArrayBridge<SoyRef>&& Peers)
{
	if (!mSocket)
		return;

	//	get clients who have finished handshaking
	mSocket->GetConnectedPeers(Peers);
}

void TWebsocketClientWrapper::WaitForConnect(Bind::TCallback& Params)
{
	auto NewPromise = mOnConnectPromises.AddPromise(Params.mLocalContext);
	Params.Return(NewPromise);

	FlushPendingConnects();
}


void TWebsocketClientWrapper::FlushPendingConnects()
{
	//	either no data, or no-one waiting yet
	if (!mOnConnectPromises.HasPromises())
		return;

	//	gotta wait for handshake to finish, so check for connected peers
	//	todo: Check for disconnection/handshake error
	auto ConnectionError = mSocket->GetConnectionError();
	bool IsConnected = false;
	auto IsError = !ConnectionError.empty();
	if (!IsError)
	{
		//	check for connection
		BufferArray<SoyRef, 1> Peers;
		mSocket->GetConnectedPeers(GetArrayBridge(Peers));
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


void TWebsocketClientWrapper::Send(Bind::TCallback& Params)
{
	auto ThisSocket = mSocket;
	if (!ThisSocket)
		throw Soy::AssertException("Socket not allocated");

	auto PeerStr = Params.GetArgumentString(0);
	auto PeerRef = SoyRef(PeerStr);

	if (Params.IsArgumentString(1))
	{
		auto DataString = Params.GetArgumentString(1);
		ThisSocket->Send(PeerRef, DataString);
	}
	else
	{
		Array<uint8_t> Data;
		Params.GetArgumentArray(1, GetArrayBridge(Data));
		ThisSocket->Send(PeerRef, GetArrayBridge(Data));
	}
}



TWebsocketClient::TWebsocketClient(const std::string& Address, std::function<void(SoyRef, const std::string&)> OnTextMessage, std::function<void(SoyRef, const Array<uint8_t>&)> OnBinaryMessage) :
	SoyWorkerThread(Soy::StreamToString(std::stringstream() << "WebsocketClient(" << Address << ")"), SoyWorkerWaitMode::Sleep),
	mOnTextMessage(OnTextMessage),
	mOnBinaryMessage(OnBinaryMessage)
{
	mSocket.reset(new SoySocket());
	mSocket->CreateTcp(true);
	//mSocket->ListenTcp(ListenPort);

	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		AddPeer(ClientRef);
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef,const std::string& Reason)
	{
		RemovePeer(ClientRef);
		//	todo: work out if this is OUR socket and OnSocketClosed on the wrapper
	};

	mSocket->Connect(Address);

	Start();
}


bool TWebsocketClient::Iteration()
{
	if (!mSocket)
		return false;

	if (!mSocket->IsCreated())
		return true;

	//	gr: does this thread need to do anything?
/*
	//	non blocking so lets just do everything in a loop
	auto NewClient = mSocket->WaitForClient();
	if (NewClient.IsValid())
		std::Debug << "new client connected: " << NewClient << std::endl;
	*/
	return true;
}


TWebsocketServer::TWebsocketServer(uint16_t ListenPort,std::function<void(SoyRef,const std::string&)> OnTextMessage,std::function<void(SoyRef,const Array<uint8_t>&)> OnBinaryMessage) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"WebsocketServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep ),
	mOnTextMessage		( OnTextMessage ),
	mOnBinaryMessage	( OnBinaryMessage )
{
	mSocket.reset( new SoySocket() );
	mSocket->CreateTcp(true);
	mSocket->ListenTcp(ListenPort);
	
	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		AddPeer(ClientRef);
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef,const std::string& Reason)
	{
		RemovePeer(ClientRef);
	};
	
	Start();
	
}
	

bool TWebsocketServer::Iteration()
{
	if ( !mSocket )
		return false;
	
	if ( !mSocket->IsCreated() )
		return true;
	
	//	non blocking so lets just do everything in a loop
	auto NewClient = mSocket->WaitForClient();
	if ( NewClient.IsValid() )
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

void TWebsocketServer::GetConnectedPeers(ArrayBridge<SoyRef>& Clients)
{
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	for ( int c=0;	c<mClients.GetSize();	c++ )
	{
		auto& Client = *mClients[c];
		if ( !Client.mHandshake.IsCompleted() )
			continue;
		if ( !Client.mHandshake.mHasSentAcceptReply )
			continue;
		
		Clients.PushBack( Client.mConnectionRef );
	}
}

std::shared_ptr<TWebsocketServerPeer> TWebsocketServer::GetPeer(SoyRef ClientRef)
{
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	for ( int c=0;	c<mClients.GetSize();	c++ )
	{
		auto& pClient = mClients[c];
		if ( pClient->mConnectionRef == ClientRef )
			return pClient;
	}

	throw Soy::AssertException("Client not found");
}

void TWebsocketServer::AddPeer(SoyRef ClientRef)
{
	std::shared_ptr<TWebsocketServerPeer> Client( new TWebsocketServerPeer( mSocket, ClientRef, mOnTextMessage, mOnBinaryMessage ) );
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	mClients.PushBack(Client);
}

void TWebsocketServer::RemovePeer(SoyRef ClientRef)
{
	
}



void TWebsocketClient::GetConnectedPeers(ArrayBridge<SoyRef>& Clients)
{
	{
		auto& Client = *mServerPeer;
		if (!Client.mHandshake.IsCompleted())
			return;
		if (!Client.mHandshake.mHasSentAcceptReply)
			return;

		Clients.PushBack(Client.mConnectionRef);
	}
}

std::shared_ptr<TWebsocketServerPeer> TWebsocketClient::GetPeer(SoyRef ClientRef)
{
	{
		auto& pClient = mServerPeer;
		if (pClient->mConnectionRef == ClientRef)
			return pClient;
	}

	throw Soy::AssertException("Peer not found");
}

void TWebsocketClient::AddPeer(SoyRef ClientRef)
{
	std::shared_ptr<TWebsocketServerPeer> Client(new TWebsocketServerPeer(mSocket, ClientRef, mOnTextMessage, mOnBinaryMessage));
	mServerPeer = Client;
	mServerPeer->ClientConnect();
	if (mOnConnected)
		mOnConnected();
}

void TWebsocketClient::RemovePeer(SoyRef ClientRef)
{
	mServerPeer = nullptr;
	if (mOnDisconnected)
		mOnDisconnected();
}

void TWebsocketServerPeer::ClientConnect()
{
	//	send a websocket request
	std::shared_ptr<WebSocket::TMessageBuffer> MessageBuffer(new WebSocket::TMessageBuffer() );
	std::shared_ptr<Soy::TWriteProtocol> Packet(new WebSocket::TRequestProtocol(this->mHandshake, MessageBuffer));
	Push(Packet);
}

void TWebsocketServerPeer::OnDataRecieved(std::shared_ptr<WebSocket::TRequestProtocol>& pData)
{
	//auto& Data = *pData;
	
	//	this was the http request, send the reply
	if ( pData->mReplyMessage )
	{
		if ( pData->mHandshake.mHasSentAcceptReply )
			throw Soy::AssertException("Already sent handshake reply");
		/*
		auto& Message = std::Debug;
		Message << "http request " << Data.mHost << " " << Data.mUrl << std::endl;
		for ( auto Header : Data.mHeaders )
		{
			Message << Header.first << ": " << Header.second << std::endl;
		}
		Message << "Content size: " << Data.mContent.GetSize();
		for ( int i=0;	i<Data.mContent.GetSize();	i++ )
			Message << Data.mContent[i];
		Message << std::endl;
		
		*/
	
		std::Debug << "Sending handshake response to " << this->GetSocketAddress() << std::endl;
		pData->mHandshake.mHasSentAcceptReply = true;
		Push( pData->mReplyMessage );
		return;
	}
	
	//	packet with data!
	if ( pData->mMessage )
	{
		auto& Packet = *pData->mMessage;
		if ( Packet.IsCompleteTextMessage() )
		{
			mOnTextMessage( this->mConnectionRef, Packet.mTextData );
			//	gr: there's a bit of a disconnect here between Some-reponse-packet data and our persistent data
			//		should probbaly change this to like
			//	pData->PopTextMessage()
			//		and a call back to owner to clear. or a "alloc new data" thing for the response class
			std::lock_guard<std::recursive_mutex> Lock(mCurrentMessageLock);
			mCurrentMessage.reset();
		}
		if ( Packet.IsCompleteBinaryMessage() )
		{
			mOnBinaryMessage( this->mConnectionRef, Packet.mBinaryData );
			//	see above
			std::lock_guard<std::recursive_mutex> Lock(mCurrentMessageLock);
			mCurrentMessage.reset();
		}
	}
}


void TWebsocketClient::Send(SoyRef ClientRef, const std::string& Message)
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


void TWebsocketClient::Send(SoyRef ClientRef, const ArrayBridge<uint8_t>& Message)
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


void TWebsocketServer::Send(SoyRef ClientRef,const std::string& Message)
{
	auto Peer = GetPeer(ClientRef);
	if ( !Peer )
	{
		std::stringstream Error;
		Error << "No peer matching " << ClientRef;
		throw Soy::AssertException(Error.str());
	}
	Peer->Send(Message);
}


void TWebsocketServer::Send(SoyRef ClientRef,const ArrayBridge<uint8_t>& Message)
{
	auto Peer = GetPeer(ClientRef);
	if ( !Peer )
	{
		std::stringstream Error;
		Error << "No peer matching " << ClientRef;
		throw Soy::AssertException(Error.str());
	}
	Peer->Send(Message);
}


std::shared_ptr<Soy::TReadProtocol> TWebsocketServerPeer::AllocProtocol()
{
	//	this needs revising... or locking at least
	std::lock_guard<std::recursive_mutex> Lock(mCurrentMessageLock);
	if ( !mCurrentMessage )
		mCurrentMessage.reset( new WebSocket::TMessageBuffer() );
	
	auto* NewProtocol = new WebSocket::TRequestProtocol(mHandshake,mCurrentMessage);
	return std::shared_ptr<Soy::TReadProtocol>( NewProtocol );
}


void TWebsocketServerPeer::Send(const std::string& Message)
{
	//	gr: cannot send if handshake hasn't completed
	if ( !this->mHandshake.IsCompleted() )
	{
		throw Soy::AssertException("Sending message before handshake complete");
	}
	std::shared_ptr<Soy::TWriteProtocol> Packet( new WebSocket::TMessageProtocol( this->mHandshake, Message ) );
	Push( Packet );
}

void TWebsocketServerPeer::Send(const ArrayBridge<uint8_t>& Message)
{
	//	gr: cannot send if handshake hasn't completed
	if (!this->mHandshake.IsCompleted())
	{
		throw Soy::AssertException("Sending message before handshake complete");
	}
	std::shared_ptr<Soy::TWriteProtocol> Packet(new WebSocket::TMessageProtocol(this->mHandshake, Message));
	Push(Packet);
}

