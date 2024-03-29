#include "TApiWebsocket.h"
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
	DEFINE_BIND_FUNCTIONNAME(Disconnect);

	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_GetAddress, GetAddress);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_Send, Send);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_GetPeers, GetPeers);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_WaitForMessage, WaitForMessage);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Server_Disconnect, Disconnect);
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
	auto ListenPort16 = size_cast<uint16_t>(ListenPort);

	auto OnTextMessage = [this](SoyRef Connection,const std::string& Message)
	{
		this->OnMessage( Message, Connection);
	};
	
	auto OnBinaryMessage = [this](SoyRef Connection,const ArrayBridge<uint8_t>&& Message)
	{
		this->OnMessage( Message, Connection);
	};
	
	mSocket.reset( new TWebsocketServer( ListenPort16, OnTextMessage, OnBinaryMessage ) );
}


void TWebsocketServerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiWebsocket::BindFunction::Server_GetAddress>( &ApiSocket::TSocketWrapper::GetAddress );
	Template.BindFunction<ApiWebsocket::BindFunction::Server_Send>(&ApiSocket::TSocketWrapper::Send );
	Template.BindFunction<ApiWebsocket::BindFunction::Server_GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiWebsocket::BindFunction::Server_WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
	Template.BindFunction<ApiWebsocket::BindFunction::Server_Disconnect>( &ApiSocket::TSocketWrapper::Disconnect );
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
	auto Hostname = Params.GetArgumentString(0);
	auto Port = Params.GetArgumentInt(1);
	auto Port16 = size_cast<uint16_t>(Port);

	auto OnTextMessage = [this](SoyRef Connection, const std::string& Message)
	{
		this->OnMessage(Message, Connection);
	};

	auto OnBinaryMessage = [this](SoyRef Connection, const ArrayBridge<uint8_t>&& Message)
	{
		this->OnMessage(Message, Connection);
	};
	
	auto OnDisconnected = [this](const std::string& Reason)
	{
		OnSocketClosed(Reason);
	};
	
	mSocket.reset(new TWebsocketClient(Hostname, Port16, OnTextMessage, OnBinaryMessage));
	mSocket->mOnConnected = std::bind(&TWebsocketClientWrapper::FlushPendingConnects, this);
	mSocket->mOnDisconnected = OnDisconnected;
}


void TWebsocketClientWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiWebsocket::BindFunction::GetAddress>(&ApiSocket::TSocketWrapper::GetAddress);
	Template.BindFunction<ApiWebsocket::BindFunction::Send>(&ApiSocket::TSocketWrapper::Send);
	Template.BindFunction<ApiWebsocket::BindFunction::GetPeers>(&ApiSocket::TSocketWrapper::GetPeers);
	Template.BindFunction<ApiWebsocket::BindFunction::WaitForMessage>(&ApiSocket::TSocketWrapper::WaitForMessage);
	Template.BindFunction<ApiWebsocket::BindFunction::WaitForConnect>(&TWebsocketClientWrapper::WaitForConnect);
	Template.BindFunction<ApiWebsocket::BindFunction::Disconnect>(&ApiSocket::TSocketWrapper::Disconnect);
}


void TWebsocketClientWrapper::GetConnectedPeers(ArrayBridge<SoyRef>&& Peers)
{
	if (!mSocket)
		return;

	//	get clients who have finished handshaking
	mSocket->GetConnectedPeers(Peers);
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



TWebsocketClient::TWebsocketClient(const std::string& Hostname,uint16_t Port,std::function<void(SoyRef, const std::string&)> OnTextMessage, std::function<void(SoyRef, const ArrayBridge<uint8_t>&&)> OnBinaryMessage) :
	SoyWorkerThread(Soy::StreamToString(std::stringstream() << "WebsocketClient(" << Hostname << ":" << Port << ")"), SoyWorkerWaitMode::Wake ),
	mOnTextMessage	( OnTextMessage ),
	mOnBinaryMessage( OnBinaryMessage ),
	mServerHost		( Hostname )
{
	mSocket.reset(new SoySocket());
	//mSocket->ListenTcp(ListenPort);

	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		AddPeer(ClientRef);
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef,const std::string& Reason)
	{
		//	gr: do this on another thread
		mDisconnectPeer = ClientRef;
		Wake();
		//RemovePeer(ClientRef);
		//	todo: work out if this is OUR socket and OnSocketClosed on the wrapper
	};

	mSocket->Connect(Hostname.c_str(), Port);

	Start();
}


bool TWebsocketClient::CanSleep()
{
	//	sleep if nothing queued to disconnect
	return mDisconnectPeer.IsValid() == false;
}

bool TWebsocketClient::Iteration()
{
	if ( mDisconnectPeer.IsValid() )
	{
		RemovePeer(mDisconnectPeer);
		
		//	just to let thread sleep again
		mDisconnectPeer = SoyRef();
	}
	/*
	//	gr: this

	if (!mSocket)
		return false;

	if (!mSocket->IsCreated())
		return true;

	//	gr: does this thread need to do anything?
/ *
	//	non blocking so lets just do everything in a loop
	auto NewClient = mSocket->WaitForClient();
	if (NewClient.IsValid())
		std::Debug << "new client connected: " << NewClient << std::endl;
	*/
	return true;
}


TWebsocketServer::TWebsocketServer(uint16_t ListenPort,std::function<void(SoyRef,const std::string&)> OnTextMessage,std::function<void(SoyRef,const ArrayBridge<uint8_t>&&)> OnBinaryMessage) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"WebsocketServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep ),
	mOnTextMessage		( OnTextMessage ),
	mOnBinaryMessage	( OnBinaryMessage )
{
	mSocket.reset( new SoySocket() );
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
	auto OnError = [](const std::string& Error)
	{
		std::Debug << "Websocket Server Peer error: " << Error << std::endl;
	};
	auto OnConnected = []()
	{
		//	on client handshake completed
	};
	std::shared_ptr<TWebsocketServerPeer> Client( new TWebsocketServerPeer( mSocket, ClientRef, mOnTextMessage, mOnBinaryMessage, OnConnected, OnError ) );
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	mClients.PushBack(Client);
}

void TWebsocketServer::RemovePeer(SoyRef ClientRef)
{
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	for ( int i=static_cast<int>(mClients.GetSize())-1;	i>=0;	i-- )
	{
		auto pClient = mClients[i];
		if ( pClient->mConnectionRef != ClientRef )
			continue;
		
		//	move client into other list as this func may be called from a disconnection
		//	we can't block-stop the thread
		pClient->Stop(false);
		mClients.RemoveBlock(i,1);
		mDeadClients.PushBack(pClient);
	}
}



void TWebsocketClient::GetConnectedPeers(ArrayBridge<SoyRef>& Clients)
{
	if (mServerPeer )
	{
		auto& Client = *mServerPeer;
		if (!Client.mHandshake.IsCompleted())
			return;
		if (Client.mHandshake.mWebSocketAcceptedKey.empty())
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
	auto OnConnected = [this]()
	{
		std::Debug << __PRETTY_FUNCTION__ << " connected" << std::endl;
		if (mOnConnected)
			mOnConnected();
	};
	auto OnError = [this](const std::string& Error)
	{
		std::Debug << "WebsocketClientPeer error " << Error << std::endl;
		if ( mOnDisconnected )
			mOnDisconnected(Error);
	};
	std::shared_ptr<TWebsocketClientPeer> Client(new TWebsocketClientPeer(mSocket, ClientRef, mOnTextMessage, mOnBinaryMessage, OnConnected, OnError ));
	mServerPeer = Client;
	mServerPeer->ClientConnect(mServerHost);

	//	gr: call anyway, this will get ignore because handshake isnt finished, but may want a semi-connected thing later
	if (mOnConnected)
		mOnConnected();
}

void TWebsocketClient::RemovePeer(SoyRef ClientRef)
{
	//	already disconnected, only call the callback once
	if ( !mServerPeer )
		return;
		
	mServerPeer = nullptr;
	if (mOnDisconnected)
		mOnDisconnected("RemovePeer");
}


TWebsocketServerPeer::TWebsocketServerPeer(std::shared_ptr<SoySocket>& Socket, SoyRef ConnectionRef, std::function<void(SoyRef, const std::string&)> OnTextMessage, std::function<void(SoyRef, const ArrayBridge<uint8_t>&&)> OnBinaryMessage,std::function<void()> OnConnected,std::function<void(const std::string&)> OnError) :
	TSocketReadThread	(Socket, ConnectionRef),
	TSocketWriteThread	(Socket, ConnectionRef),
	mOnTextMessage		(OnTextMessage),
	mOnBinaryMessage	(OnBinaryMessage),
	mOnConnected		(OnConnected),
	mConnectionRef		(ConnectionRef),
	mOnError			(OnError)
{
	TSocketReadThread::mOnDataRecieved = [this](std::shared_ptr<Soy::TReadProtocol>& Data)
	{
		OnDataRecieved(Data);
	};
	TSocketWriteThread::mOnStreamError = [this](const std::string& Error)
	{
		this->mOnError(Error);
	};
	TSocketWriteThread::mOnShutdown = [this](bool WhatWasThisParam)
	{
		std::Debug << "TWebsocketServerPeer OnShutdown" << std::endl;
	};

	TSocketReadThread::Start();
	TSocketWriteThread::Start();
}

TWebsocketServerPeer::~TWebsocketServerPeer()
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	TSocketReadThread::Stop();
	TSocketWriteThread::Stop();

	TSocketReadThread::WaitToFinish();
	TSocketWriteThread::WaitToFinish();
}

void TWebsocketServerPeer::ClientConnect(const std::string& Host)
{
	//	send a websocket request
	std::shared_ptr<WebSocket::TMessageBuffer> MessageBuffer(new WebSocket::TMessageBuffer() );
	std::shared_ptr<Soy::TWriteProtocol> Packet(new WebSocket::TRequestProtocol( this->mHandshake, MessageBuffer, Host ));
	Push(Packet);
}

void TWebsocketServerPeer::Stop(bool WaitToFinish)
{
	if ( WaitToFinish )
	{
		TSocketReadThread::WaitToFinish();
		TSocketWriteThread::WaitToFinish();
	}
	else
	{
		TSocketReadThread::Stop();
		TSocketWriteThread::Stop();
	}
}

void TWebsocketClientPeer::OnDataRecieved(std::shared_ptr<Soy::TReadProtocol>& pData)
{
	auto& Data = dynamic_cast<WebSocket::THandshakeResponseProtocol&>(*pData);
	if (Data.mHandshake.IsCompleted())
	{
		//	todo, only send once
		this->mOnConnected();
	}
	//	completed response, dont need to do anything?

	//	packet with data!
	if (Data.mMessage)
	{
		auto& Packet = *Data.mMessage;
		if (Packet.IsCompleteTextMessage())
		{
			mOnTextMessage(this->mConnectionRef, Packet.mTextData);
			//	gr: there's a bit of a disconnect here between Some-reponse-packet data and our persistent data
			//		should probbaly change this to like
			//	pData->PopTextMessage()
			//		and a call back to owner to clear. or a "alloc new data" thing for the response class
			std::lock_guard<std::recursive_mutex> Lock(mCurrentMessageLock);
			mCurrentMessage.reset();
		}
		if (Packet.IsCompleteBinaryMessage())
		{
			mOnBinaryMessage(this->mConnectionRef, GetArrayBridge(Packet.mBinaryData) );
			//	see above
			std::lock_guard<std::recursive_mutex> Lock(mCurrentMessageLock);
			mCurrentMessage.reset();
		}
	}
}

void TWebsocketServerPeer::OnDataRecieved(std::shared_ptr<Soy::TReadProtocol>& pData)
{
	auto& Data = dynamic_cast<WebSocket::TRequestProtocol&>(*pData);

	//auto& Data = *pData;
	
	//	this was the http request, send the reply
	if ( Data.mReplyMessage )
	{
		if ( Data.mHandshake.mHasSentAcceptReply )
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
		Data.mHandshake.mHasSentAcceptReply = true;
		Push( Data.mReplyMessage );
		return;
	}
	
	//	packet with data!
	if ( Data.mMessage )
	{
		auto& Packet = *Data.mMessage;
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
			mOnBinaryMessage( this->mConnectionRef, GetArrayBridge(Packet.mBinaryData) );
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
	
	auto* NewProtocol = new WebSocket::TRequestProtocol(mHandshake,mCurrentMessage,"XXX");
	return std::shared_ptr<Soy::TReadProtocol>( NewProtocol );
}


std::shared_ptr<Soy::TReadProtocol> TWebsocketClientPeer::AllocProtocol()
{
	//	this needs revising... or locking at least
	std::lock_guard<std::recursive_mutex> Lock(mCurrentMessageLock);
	if (!mCurrentMessage)
		mCurrentMessage.reset(new WebSocket::TMessageBuffer());

	auto* NewProtocol = new WebSocket::THandshakeResponseProtocol(mHandshake, mCurrentMessage);
	return std::shared_ptr<Soy::TReadProtocol>(NewProtocol);
}


void TWebsocketServerPeer::Send(const std::string& Message)
{
	//	gr: cannot send if handshake hasn't completed
	if ( !this->mHandshake.IsCompleted() )
	{
		throw Soy::AssertException("Sending message before handshake complete");
	}
	auto FromServer = IsSendingFromServer();
	std::shared_ptr<Soy::TWriteProtocol> Packet( new WebSocket::TMessageProtocol( FromServer, this->mHandshake, Message ) );
	Push( Packet );
}

void TWebsocketServerPeer::Send(const ArrayBridge<uint8_t>& Message)
{
	//	gr: cannot send if handshake hasn't completed
	if (!this->mHandshake.IsCompleted())
	{
		throw Soy::AssertException("Sending message before handshake complete");
	}
	auto FromServer = IsSendingFromServer();
	std::shared_ptr<Soy::TWriteProtocol> Packet(new WebSocket::TMessageProtocol( FromServer, this->mHandshake, Message));
	Push(Packet);
}

