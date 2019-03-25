#include "TApiInput.h"
#include "SoyHid.h"



namespace ApiInput
{
	const char Namespace[] = "Pop.Input";

	const char EnumDevices_FunctionName[] = "EnumDevices";
	DEFINE_BIND_FUNCTIONNAME(OnDevicesChanged);
	
	const char InputDevice_TypeName[] = "Device";
	const char GetState_FunctionName[] = "GetState";

	void	EnumDevices(Bind::TCallback& Params);
	void	OnDevicesChanged(Bind::TCallback& Params);
	
	class TContextManager;
}


class ApiInput::TContextManager
{
public:
	TContextManager();
	
	Bind::TPromiseQueue		mOnDevicesChangedPromises;
	Hid::TContext			mContext;
};

ApiInput::TContextManager ContextManager;



ApiInput::TContextManager::TContextManager()
{
	mContext.mOnDevicesChanged = [this]()
	{
		mOnDevicesChangedPromises.Resolve();
	};
}

void ApiInput::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<EnumDevices_FunctionName>( EnumDevices, Namespace );
	Context.BindGlobalFunction<OnDevicesChanged_FunctionName>( OnDevicesChanged, Namespace );

	Context.BindObjectType<TInputDeviceWrapper>( Namespace );
}



void ApiInput::OnDevicesChanged(Bind::TCallback& Params)
{
	auto Promise = ContextManager.mOnDevicesChangedPromises.AddPromise( Params.mContext );
	
	Params.Return( Promise );
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
			
			ContextManager.mContext.EnumDevices( EnumDevice );
			
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
	mDevice.reset( new Hid::TDevice( ContextManager.mContext, DeviceName) );
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

