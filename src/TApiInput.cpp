#include "TApiInput.h"
#include "SoyInput.h"

using namespace v8;

const char EnumDevices_FunctionName[] = "EnumDevices";

const char InputDevice_TypeName[] = "InputDevice";
const char GetState_FunctionName[] = "GetState";

namespace ApiInput
{
	const char Namespace[] = "Input";
}

void ApiInput::Bind(TV8Container& Container)
{
	Container.BindObjectType( Namespace, TInputWrapper::CreateTemplate, nullptr );

	Container.BindObjectType( TInputDeviceWrapper::GetObjectTypeName(), TInputDeviceWrapper::CreateTemplate, TInputDeviceWrapper::Allocate<TInputDeviceWrapper> );
}


void TInputWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
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
	auto* NewWrapper = new TInputWrapper();
	
	//	store persistent handle to the javascript object
	NewWrapper->mHandle = v8::GetPersistent( *Isolate, Arguments.This() );
	NewWrapper->mContainer = &Container;
	
	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWrapper ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}


Local<FunctionTemplate> TInputWrapper::CreateTemplate(TV8Container& Container)
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


v8::Local<v8::Value> TInputWrapper::EnumDevices(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	//auto& This = v8::GetObject<TMediaWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;

	//auto* pThis = &This;
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( Params.GetIsolate(), Resolver );

	auto* Container = &Params.mContainer;
	
	auto DoEnumDevices = [=]
	{
		try
		{
			Array<std::string> DeviceNames;
			auto EnumDevice = [&](const Soy::TInputDeviceMeta& Meta)
			{
				DeviceNames.PushBack( Meta.mName );
				//DeviceNames.PushBack( Meta.mSerial );
				//DeviceNames.PushBack( Meta.mUsbPath );
			};
			
			Soy::EnumInputDevices( EnumDevice );
			
			auto OnCompleted = [=](Local<Context> Context)
			{
				//	return face points here
				//	gr: can't do this unless we're in the javascript thread...
				auto ResolverLocal = ResolverPersistent->GetLocal(*Isolate);
				//ResolverPersistent.Reset();
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
			std::Debug << e.what() << std::endl;
			
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Local<Context> Context)
			{
				auto ResolverLocal = ResolverPersistent->GetLocal(*Isolate);
				//ResolverPersistent.Reset();
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



void TInputDeviceWrapper::Construct(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;

	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	auto DeviceNameHandle = Arguments[0];
	/*
	auto OnFrameExtracted = [=](const SoyTime Time,size_t StreamIndex)
	{
		//std::Debug << "Got stream[" << StreamIndex << "] frame at " << Time << std::endl;
		this->OnNewFrame(StreamIndex);
	};
	*/

	//	create device
	auto DeviceName = v8::GetString( DeviceNameHandle );
	
	mDevice.reset( new Soy::TInputDevice(DeviceName) );
}


Local<FunctionTemplate> TInputDeviceWrapper::CreateTemplate(TV8Container& Container)
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
	Container.BindFunction<GetState_FunctionName>( InstanceTemplate, GetState );

	return ConstructorFunc;
}



v8::Local<v8::Value> TInputDeviceWrapper::GetState(const v8::CallbackInfo& Params)
{
	auto& This = Params.GetThis<TInputDeviceWrapper>();

	//	output an object with name, different axis', buttons
	throw Soy::AssertException("todo: GetState()");
	
	return v8::Undefined(Params.mIsolate);
}

