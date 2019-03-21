#include "TApiInput.h"
#include "SoyHid.h"


Hid::TContext HidApiContext;


namespace ApiInput
{
	const char Namespace[] = "Pop.Input";

	const char EnumDevices_FunctionName[] = "EnumDevices";
	
	const char InputDevice_TypeName[] = "Device";
	const char GetState_FunctionName[] = "GetState";

	void	EnumDevices(Bind::TCallback& Params);
}

void ApiInput::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<EnumDevices_FunctionName>( ApiInput::EnumDevices, Namespace );

	Context.BindObjectType<TInputDeviceWrapper>();
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
					Object.SetString("Name", Meta.mName );
					Object.SetString("Serial", Meta.mSerial );
					Object.SetString("Vendor", Meta.mVendor );
					Object.SetString("UsbPath", Meta.mUsbPath );
					return Object;
				};
				auto DevicesArray = Context.CreateArray( DeviceMetas.GetSize(), GetValue );
				Promise.Resolve( DevicesArray );
			};
			
			//	queue the completion, doesn't need to be done instantly
			Params.mContext.Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
			
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Bind::TContext& Context)
			{
				Promise.Reject( ExceptionString );
			};
			Params.mContext.Queue( OnError );
		}
	};
	
	//	not on job/thread atm, but that's okay
	DoEnumDevices();

	Params.Return( Promise );
}



void TInputDeviceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
	mDevice.reset( new Hid::TDevice( HidApiContext, DeviceName) );
}


void TInputDeviceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiInput::GetState_FunctionName>( GetState );
}



void TInputDeviceWrapper::GetState(Bind::TCallback& Params)
{
	auto& This = Params.This<TInputDeviceWrapper>();

	auto InputState = This.mDevice->GetState();
	
	auto State = Params.mContext.CreateObjectInstance();
	State.SetArray("Buttons", GetArrayBridge(InputState.mButton) );

	auto GetAxisElement = [&](size_t Index)
	{
		auto& Axis = InputState.mAxis[Index];
		auto AxisObject = Params.mContext.CreateObjectInstance();
		AxisObject.SetFloat("x", Axis.x );
		AxisObject.SetFloat("y", Axis.y );
		return AxisObject;
	};
	auto AxisArray = Params.mContext.CreateArray( InputState.mAxis.GetSize(), GetAxisElement );
	State.SetArray("Axis", AxisArray );

	Params.Return( State );
}

