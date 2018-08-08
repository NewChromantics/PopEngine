#include "TApiHttp.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include <SoySocket.h>

using namespace v8;

const char GetAddress_FunctionName[] = "GetAddress";
const char Send_FunctionName[] = "Send";
const char GetPeers_FunctionName[] = "GetPeers";


void ApiHttp::Bind(TV8Container& Container)
{
	Container.BindObjectType("HttpServer", THttpServerWrapper::CreateTemplate, nullptr );
}



void THttpServerWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
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
		auto* NewWrapper = new THttpServerWrapper(ListenPortArg);
		
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


Local<FunctionTemplate> THttpServerWrapper::CreateTemplate(TV8Container& Container)
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



THttpServerWrapper::THttpServerWrapper(uint16_t ListenPort) :
	mContainer		( nullptr )
{
	mSocket.reset( new THttpServer(ListenPort ) );
}




THttpServer::THttpServer(uint16_t ListenPort) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"HttpServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep )
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
	

bool THttpServer::Iteration()
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


std::shared_ptr<THttpServerPeer> THttpServer::GetClient(SoyRef ClientRef)
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

void THttpServer::AddClient(SoyRef ClientRef)
{
	std::shared_ptr<THttpServerPeer> Client( new THttpServerPeer( mSocket, ClientRef ) );
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	mClients.PushBack(Client);
}

void THttpServer::RemoveClient(SoyRef ClientRef)
{
	
}


void THttpServerPeer::OnDataRecieved(std::shared_ptr<Http::TRequestProtocol>& pData)
{
	auto& Data = *pData;
	
	//	send http reply
	std::Debug << "Send http reply" << std::endl;
}


