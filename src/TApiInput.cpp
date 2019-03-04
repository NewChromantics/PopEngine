#include "TApiInput.h"
#include "SoyHid.h"

using namespace v8;

Hid::TContext HidApiContext;

const char EnumDevices_FunctionName[] = "EnumDevices";

const char InputDevice_TypeName[] = "Device";
const char GetState_FunctionName[] = "GetState";

namespace ApiInput
{
	const char Namespace[] = "Pop.Input";
	
	v8::Local<v8::Value>	EnumDevices(const v8::CallbackInfo& Params);
}

void ApiInput::Bind(TV8Container& Container)
{
	Container.CreateGlobalObjectInstance("", Namespace);

	Container.BindGlobalFunction<EnumDevices_FunctionName>( ApiInput::EnumDevices, Namespace );

	Container.BindObjectType( TInputDeviceWrapper::GetObjectTypeName(), TInputDeviceWrapper::CreateTemplate, TInputDeviceWrapper::Allocate<TInputDeviceWrapper>, Namespace );
}



v8::Local<v8::Value> ApiInput::EnumDevices(const v8::CallbackInfo& Params)
{
	auto* Isolate = Params.mIsolate;

	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( Params.GetIsolate(), Resolver );

	auto* Container = &Params.mContainer;
	
	auto DoEnumDevices = [=]
	{
		try
		{
			Array<Soy::TInputDeviceMeta> DeviceMetas;
			auto EnumDevice = [&](Soy::TInputDeviceMeta& Meta)
			{
				DeviceMetas.PushBack( Meta );
			};
			
			HidApiContext.EnumDevices( EnumDevice );
			
			auto OnCompleted = [=](Local<Context> Context)
			{
				auto& Isolate = *Context->GetIsolate();
				auto GetString = [&](const std::string& String)
				{
					return v8::GetString( Isolate, String );
				};

				auto ResolverLocal = ResolverPersistent->GetLocal(Isolate);
				//ResolverPersistent.Reset();
				auto GetValue = [&](size_t Index)
				{
					auto& Meta = DeviceMetas[Index];
					auto Object = v8::Object::New( &Isolate );
					Object->Set( GetString("Name"), GetString(Meta.mName) );
					Object->Set( GetString("Serial"), GetString(Meta.mSerial) );
					Object->Set( GetString("Vendor"), GetString(Meta.mVendor) );
					Object->Set( GetString("UsbPath"), GetString(Meta.mUsbPath) );
					return Object;
				};
				auto DevicesArray = v8::GetArray( Isolate, DeviceMetas.GetSize(), GetValue );
				ResolverLocal->Resolve( DevicesArray );
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
				auto Error = v8::GetString( *Isolate, ExceptionString );
				ResolverLocal->Reject( Error );
			};
			Container->QueueScoped( OnError );
		}
	};
	
	//	not on job atm
	DoEnumDevices();

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
	
	mDevice.reset( new Hid::TDevice( HidApiContext, DeviceName) );
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

	auto InputState = This.mDevice->GetState();
	//	output an object with name, different axis', buttons
	
	auto State = v8::Object::New( &Params.GetIsolate() );
	
	auto GetButtonElement = [&](size_t Index)
	{
		auto Number = v8::Number::New( &Params.GetIsolate(), InputState.mButton[Index] );
		return Local<Value>::Cast( Number );
	};
	auto ButtonArray = v8::GetArray( Params.GetIsolate(), InputState.mButton.GetSize(), GetButtonElement );
	auto ButtonsString = v8::GetString( Params.GetIsolate(), "Buttons" );

	auto xString = v8::GetString( Params.GetIsolate(), "x" );
	auto yString = v8::GetString( Params.GetIsolate(), "y" );
	auto GetAxisElement = [&](size_t Index)
	{
		auto xNumber = v8::Number::New( &Params.GetIsolate(), InputState.mAxis[Index].x );
		auto yNumber = v8::Number::New( &Params.GetIsolate(), InputState.mAxis[Index].y );
		auto Axis = v8::Object::New( &Params.GetIsolate() );
		Axis->Set( xString, xNumber );
		Axis->Set( yString, yNumber );
		return Local<Value>::Cast( Axis );
	};
	auto AxisArray = v8::GetArray( Params.GetIsolate(), InputState.mAxis.GetSize(), GetAxisElement );
	auto AxisString = v8::GetString( Params.GetIsolate(), "Axis" );

	State->Set( ButtonsString, ButtonArray );
	State->Set( AxisString, AxisArray );

	return State;
}
