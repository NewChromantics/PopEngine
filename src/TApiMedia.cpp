#include "TApiMedia.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

#include "PopMovie/AvfVideoCapture.h"


using namespace v8;

const char EnumDevices_FunctionName[] = "EnumDevices";


void ApiMedia::Bind(TV8Container& Container)
{
	Container.BindObjectType("Media", TMediaWrapper::CreateTemplate );
}


void TMediaWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
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
	
	//	alloc window
	//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
	//		but it also needs to know of the V8container to run stuff
	//		cyclic hell!
	auto* NewWrapper = new TMediaWrapper();
	
	//	store persistent handle to the javascript object
	NewWrapper->mHandle.Reset( Isolate, Arguments.This() );
	NewWrapper->mContainer = &Container;
	
	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWrapper ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}


Local<FunctionTemplate> TMediaWrapper::CreateTemplate(TV8Container& Container)
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
	
	//	add members
	Container.BindFunction<EnumDevices_FunctionName>( InstanceTemplate, EnumDevices );
	
	return ConstructorFunc;
}



template<typename TYPE>
v8::Persistent<TYPE,CopyablePersistentTraits<TYPE>> MakeLocal(v8::Isolate* Isolate,Local<TYPE> LocalHandle)
{
	Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
	PersistentHandle.Reset( Isolate, LocalHandle );
	return PersistentHandle;
}

v8::Local<v8::Value> TMediaWrapper::EnumDevices(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	//auto& This = v8::GetObject<TMediaWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;

	//auto* pThis = &This;
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( *Isolate, Resolver );

	auto* Container = &Params.mContainer;
	
	auto DoEnumDevices = [=]
	{
		try
		{
			Array<std::string> DeviceNames;
			auto EnumDevice = [&](const std::string& Name)
			{
				DeviceNames.PushBack(Name);
			};
			::Platform::EnumCaptureDevices(EnumDevice);
			
			auto OnCompleted = [=](Local<Context> Context)
			{
				//	return face points here
				//	gr: can't do this unless we're in the javascript thread...
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent );
				auto GetValue = [&](size_t Index)
				{
					return v8::GetString(*Context->GetIsolate(), DeviceNames[Index] );
				};
				auto DeviceNamesArray = v8::GetArray( *Context->GetIsolate(), DeviceNames.GetSize(), GetValue );
				ResolverLocal->Resolve( DeviceNamesArray );
			};
			
			//	queue the completion, doesn't need to be done instantly
			Container->QueueScoped( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Local<Context> Context)
			{
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent );
				auto Error = String::NewFromUtf8( Isolate, ExceptionString.c_str() );
				ResolverLocal->Reject( Error );
			};
			Container->QueueScoped( OnError );
		}
	};
	
	DoEnumDevices();
	//auto& Dlib = This.mDlibJobQueue;
	//Dlib.PushJob( RunFaceDetector );

	//	return the promise
	auto Promise = Resolver->GetPromise();
	return Promise;
}

