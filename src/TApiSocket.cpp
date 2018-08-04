#include "TApiSocket.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include <SoySocket.h>

using namespace v8;



void ApiSocket::Bind(TV8Container& Container)
{
	Container.BindObjectType("UdpBroadcastServer", TUdpBroadcastServerWrapper::CreateTemplate );
}



void TUdpBroadcastServerWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
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
	auto* NewWrapper = new TUdpBroadcastServerWrapper(ListenPortArg);
	
	//	store persistent handle to the javascript object
	NewWrapper->mHandle.Reset( Isolate, Arguments.This() );
	NewWrapper->mContainer = &Container;

	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWrapper ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}


Local<FunctionTemplate> TUdpBroadcastServerWrapper::CreateTemplate(TV8Container& Container)
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



TUdpBroadcastServerWrapper::TUdpBroadcastServerWrapper(uint16_t ListenPort) :
	mContainer		( nullptr )
{
	auto OnBinaryMessage = [this](const Array<uint8_t>& Message,SoyRef Sender)
	{
		this->OnMessage( Message, Sender );
	};
	
	mSocket.reset( new TUdpBroadcastServer(ListenPort, OnBinaryMessage ) );
}


void TUdpBroadcastServerWrapper::OnMessage(const Array<uint8_t>& Message,SoyRef Sender)
{
	auto SendJsMessage = [=](Local<Context> Context)
	{
		auto& Container = *this->mContainer;
		auto& Isolate = *Context->GetIsolate();
		auto ThisHandle = Local<Object>::New( &Isolate, this->mHandle );
		auto FunctionHandle = v8::GetFunction( Context, ThisHandle, "OnMessage" );
		
		BufferArray<Local<Value>,2> Args;
		auto MessageHandle = v8::GetTypedArray( Isolate, GetArrayBridge(Message) );
		auto RefHandle = v8::Number::New( &Isolate, Sender.GetInt64() );
		Args.PushBack( MessageHandle );
		Args.PushBack( RefHandle );

		Container.ExecuteFunc( Context, FunctionHandle, ThisHandle, GetArrayBridge(Args) );
	};
	
	mContainer->QueueScoped( SendJsMessage );
}




TUdpBroadcastServer::TUdpBroadcastServer(uint16_t ListenPort,std::function<void(const Array<uint8_t>&,SoyRef)> OnBinaryMessage) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"UdpBroadcastServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep ),
	mOnBinaryMessage	( OnBinaryMessage )
{
	mSocket.reset( new SoySocket() );
	auto Broadcast = true;
	mSocket->CreateUdp(Broadcast);
	mSocket->ListenUdp(ListenPort);
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


