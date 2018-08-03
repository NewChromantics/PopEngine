#include "TApiWebsocket.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include <SoySocket.h>

using namespace v8;



void ApiWebsocket::Bind(TV8Container& Container)
{
	Container.BindObjectType("WebsocketServer", TWebsocketServerWrapper::CreateTemplate );
}



void TWebsocketServerWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
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
	
	
	return ConstructorFunc;
}



TWebsocketServerWrapper::TWebsocketServerWrapper(uint16_t ListenPort) :
	mContainer		( nullptr )
{
	mSocket.reset( new TWebsocketServer(ListenPort) );
}



TWebsocketServer::TWebsocketServer(uint16_t ListenPort) :
	SoyWorkerThread	( Soy::StreamToString(std::stringstream()<<"WebsocketServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep )
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


std::shared_ptr<TClient> TWebsocketServer::GetClient(SoyRef ClientRef)
{
	/*
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	for ( int c=0;	c<mClients.GetSize();	c++ )
	{
		auto& pClient = mClients[c];
		if ( pClient->mRef == ClientRef )
			return pClient;
	}
	 */
	throw Soy::AssertException("Client not found");
}

void TWebsocketServer::AddClient(SoyRef ClientRef)
{
	auto OnTextMessage = [](const std::string& Message)
	{
		std::Debug << Message << std::endl;
	};
	auto OnBinaryMessage = [](const Array<uint8_t>& Message)
	{
		std::Debug << "Binary message: x" << Message.GetDataSize() << " bytes " << std::endl;
	};
	
	std::shared_ptr<TClient> Client( new TClient( mSocket, ClientRef, OnTextMessage, OnBinaryMessage ) );
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	mClients.PushBack(Client);
}

void TWebsocketServer::RemoveClient(SoyRef ClientRef)
{
	
}


void TClient::OnDataRecieved(std::shared_ptr<WebSocket::TRequestProtocol>& pData)
{
	auto& Data = *pData;
	
	//	this was the http request, send the reply
	if ( pData->mReplyMessage )
	{
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
		
		
		std::Debug << "Sending handshake response" << std::endl;
		Push( pData->mReplyMessage );
		return;
	}
	
	//	packet with data!
	auto& Packet = pData->mMessage;
	if ( Packet.IsCompleteTextMessage() )
	{
		mOnTextMessage( Packet.mTextData );
		//	gr: there's a bit of a disconnect here between Some-reponse-packet data and our persistent data
		//		should probbaly change this to like
		//	pData->PopTextMessage()
		//		and a call back to owner to clear. or a "alloc new data" thing for the response class
		mCurrentMessage.reset();
	}
	if ( Packet.IsCompleteBinaryMessage() )
	{
		mOnBinaryMessage( Packet.mBinaryData );
		//	see above
		mCurrentMessage.reset();
	}

}



std::shared_ptr<Soy::TReadProtocol> TClient::AllocProtocol()
{
	if ( !mCurrentMessage )
		mCurrentMessage.reset( new WebSocket::TMessage() );
	
	auto* NewProtocol = new WebSocket::TRequestProtocol(mHandshake,*mCurrentMessage);
	return std::shared_ptr<Soy::TReadProtocol>( NewProtocol );
}
	
