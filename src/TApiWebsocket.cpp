#include "TApiWebsocket.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoySocket.h"

using namespace v8;

const char GetAddress_FunctionName[] = "GetAddress";
const char Send_FunctionName[] = "Send";
const char GetPeers_FunctionName[] = "GetPeers";


void ApiWebsocket::Bind(TV8Container& Container)
{
	Container.BindObjectType("WebsocketServer", TWebsocketServerWrapper::CreateTemplate, nullptr );
}



void TWebsocketServerWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	try
	{
		if ( !Arguments.IsConstructCall() )
			throw Soy::AssertException("Expecting to be used as constructor.");
	
		auto This = Arguments.This();
		auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
		
		auto ListenPortArg = Arguments[0].As<Number>()->Uint32Value();
		
		//	alloc window
		//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
		//		but it also needs to know of the V8container to run stuff
		//		cyclic hell!
		auto* NewWrapper = new TWebsocketServerWrapper(ListenPortArg);
		
		//	store persistent handle to the javascript object
		NewWrapper->mHandle.Reset( Isolate, Arguments.This() );
		NewWrapper->mContainer = &Container;

		//	set fields
		This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWrapper ) );
		
		// return the new object back to the javascript caller
		Arguments.GetReturnValue().Set( This );
	}
	catch(std::exception& e)
	{
		auto Str = v8::GetString( *Isolate, e.what() );
		auto Exception = Isolate->ThrowException(Str);
		Arguments.GetReturnValue().Set(Exception);
	}
}


Local<FunctionTemplate> TWebsocketServerWrapper::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);

	Container.BindFunction<GetAddress_FunctionName>( InstanceTemplate, GetAddress );
	Container.BindFunction<Send_FunctionName>( InstanceTemplate, Send );
	Container.BindFunction<GetPeers_FunctionName>( InstanceTemplate, GetPeers );

	return ConstructorFunc;
}



TWebsocketServerWrapper::TWebsocketServerWrapper(uint16_t ListenPort) :
	mContainer		( nullptr )
{
	auto OnTextMessage = [this](SoyRef Connection,const std::string& Message)
	{
		this->OnMessage(Message);
	};
	
	auto OnBinaryMessage = [this](SoyRef Connection,const Array<uint8_t>& Message)
	{
		this->OnMessage(Message);
	};
	
	mSocket.reset( new TWebsocketServer(ListenPort, OnTextMessage, OnBinaryMessage ) );
}


void TWebsocketServerWrapper::OnMessage(const std::string& Message)
{
	auto SendJsMessage = [=](Local<Context> Context)
	{
		auto& Container = *this->mContainer;
		auto& Isolate = *Context->GetIsolate();
		auto ThisHandle = Local<Object>::New( &Isolate, this->mHandle );
		BufferArray<Local<Value>,1> Args;
		auto FunctionHandle = v8::GetFunction( Context, ThisHandle, "OnMessage" );
		
		auto MessageHandle = v8::GetString( Isolate, Message );
		Args.PushBack( MessageHandle );
		
		Container.ExecuteFunc( Context, FunctionHandle, ThisHandle, GetArrayBridge(Args) );
	};
	
	mContainer->QueueScoped( SendJsMessage );
}

void TWebsocketServerWrapper::OnMessage(const Array<uint8_t>& Message)
{
	Array<uint8_t> MessageCopy( Message );
	auto SendJsMessage = [=](Local<Context> Context)
	{
		auto& Container = *this->mContainer;
		auto& Isolate = *Context->GetIsolate();
		auto ThisHandle = Local<Object>::New( &Isolate, this->mHandle );
		BufferArray<Local<Value>,1> Args;
		auto FunctionHandle = v8::GetFunction( Context, ThisHandle, "OnMessage" );
		
		auto MessageHandle = v8::GetTypedArray( Isolate, GetArrayBridge(MessageCopy) );
		Args.PushBack( MessageHandle );
		
		Container.ExecuteFunc( Context, FunctionHandle, ThisHandle, GetArrayBridge(Args) );
	};
	
	mContainer->QueueScoped( SendJsMessage );
}


void TWebsocketServerWrapper::Send(Bind::TCallback& Params)
{
	auto& This = Params.This<TWebsocketServerWrapper>();
	auto ThisSocket = This.mSocket;
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	auto PeerStr = Params.GetArgumentString(0);
	auto PeerRef = SoyRef( PeerStr );

	if ( Params.IsArgumentString(1) )
	{
		auto DataString = Params.GetArgumentString(0);
		ThisSocket->Send( PeerRef, DataString );
	}
	else
	{
		Array<uint8_t> Data;
		Params.GetArgumentArray(1, GetArrayBridge(Data) );
		//v8::EnumArray<v8::Uint8Array>(DataHandle,GetArrayBridge(Data) );
		ThisSocket->Send( PeerRef, GetArrayBridge(Data) );
	}
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
		AddClient(ClientRef);
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef)
	{
		RemoveClient(ClientRef);
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


std::shared_ptr<TWebsocketServerPeer> TWebsocketServer::GetClient(SoyRef ClientRef)
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

void TWebsocketServer::AddClient(SoyRef ClientRef)
{
	std::shared_ptr<TWebsocketServerPeer> Client( new TWebsocketServerPeer( mSocket, ClientRef, mOnTextMessage, mOnBinaryMessage ) );
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	mClients.PushBack(Client);
}

void TWebsocketServer::RemoveClient(SoyRef ClientRef)
{
	
}


void TWebsocketServerPeer::OnDataRecieved(std::shared_ptr<WebSocket::TRequestProtocol>& pData)
{
	auto& Data = *pData;
	
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


void TWebsocketServer::Send(SoyRef ClientRef,const std::string& Message)
{
	auto Peer = GetClient(ClientRef);
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
	auto Peer = GetClient(ClientRef);
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
	throw Soy::AssertException("Encode to websocket protocol");
	
}

