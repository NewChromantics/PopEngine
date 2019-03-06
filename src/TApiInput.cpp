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
	
	void	EnumDevices(Bind::TCallback& Params);
}

void ApiInput::Bind(TV8Container& Container)
{
	Container.CreateGlobalObjectInstance("", Namespace);

	Container.BindGlobalFunction<EnumDevices_FunctionName>( ApiInput::EnumDevices, Namespace );

	Container.BindObjectType( TInputDeviceWrapper::GetObjectTypeName(), TInputDeviceWrapper::CreateTemplate, TInputDeviceWrapper::Allocate<TInputDeviceWrapper>, Namespace );
}



void ApiInput::EnumDevices(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();

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
			
			auto OnCompleted = [=](Bind::TContext& Context)
			{
				//ResolverPersistent.Reset();
				auto GetValue = [&](size_t Index)
				{
					auto& Meta = DeviceMetas[Index];
					auto Object = Context.CreateObjectInstance();
					Object.Set("Name", Meta.mName );
					Object->Set("Serial", Meta.mSerial );
					Object->Set("Vendor", Meta.mVendor );
					Object->Set("UsbPath", Meta.mUsbPath );
					return Object;
				};
				auto DevicesArray = Context.CreateArray( DeviceMetas.GetSize(), GetValue );
				Promise->Resolve( DevicesArray );
			};
			
			//	queue the completion, doesn't need to be done instantly
			Context.Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
			
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Local<Context> Context)
			{
				Resolver->Reject( ExceptionString );
			};
			Container->QueueScoped( OnError );
		}
	};
	
	//	not on job/thread atm, but that's okay
	DoEnumDevices();

	Params.Return( Resolver );
}



void TInputDeviceWrapper::Construct(Bind::TCallback& Params)
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



v8::Local<v8::Value> TInputDeviceWrapper::GetState(v8::TCallback& Params)
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

