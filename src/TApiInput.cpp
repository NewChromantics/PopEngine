#include "TApiInput.h"
#include "SoyHid.h"



namespace ApiInput
{
	const char Namespace[] = "Pop.Input";

	const char EnumDevices_FunctionName[] = "EnumDevices";
	DEFINE_BIND_FUNCTIONNAME(OnDevicesChanged);

	DEFINE_BIND_TYPENAME(Device);
	DEFINE_BIND_FUNCTIONNAME(GetState);
	DEFINE_BIND_FUNCTIONNAME(OnStateChanged);

	void	EnumDevices(Bind::TCallback& Params);
	void	OnDevicesChanged(Bind::TCallback& Params);
	
	class TContextManager;
}


class ApiInput::TContextManager
{
public:
	TContextManager();
	
	void					EnumDevices(Bind::TContext& Context,Bind::TPromise& Promise);	//	return data into promise

	Bind::TPromiseQueue		mOnDevicesChangedPromises;
	Hid::TContext			mContext;
};

ApiInput::TContextManager ContextManager;



ApiInput::TContextManager::TContextManager()
{
	mContext.mOnDevicesChanged = [this]()
	{
		if ( !mOnDevicesChangedPromises.HasContext() )
		{
			std::Debug << "OnDevicesChanged never requested, lost device-changed callback" << std::endl;
			return;
		}
		
		auto Flush = [this](Bind::TContext& Context)
		{
			auto HandlePromise = [&](Bind::TPromise& Promise)
			{
				this->EnumDevices( Context, Promise );
			};
			mOnDevicesChangedPromises.Flush( HandlePromise );
		};
		auto& BindContext = mOnDevicesChangedPromises.GetContext();
		BindContext.Queue( Flush );
	};
}

void ApiInput::TContextManager::EnumDevices(Bind::TContext& Context,Bind::TPromise& Promise)
{
	Array<Soy::TInputDeviceMeta> DeviceMetas;
	auto EnumDevice = [&](Soy::TInputDeviceMeta& Meta)
	{
		DeviceMetas.PushBack( Meta );
	};

	ContextManager.mContext.EnumDevices( EnumDevice );

	auto GetValue = [&](size_t Index)
	{
		auto& Meta = DeviceMetas[Index];
		auto Object = Context.CreateObjectInstance();
		Object.SetString("Name", Meta.mName );
		Object.SetString("Serial", Meta.mSerial );
		Object.SetString("Vendor", Meta.mVendor );
		Object.SetString("UsbPath", Meta.mUsbPath );
		Object.SetBool("Connected", Meta.mConnected );
		return Object;
	};
	auto DevicesArray = Context.CreateArray( DeviceMetas.GetSize(), GetValue );
	Promise.Resolve( DevicesArray );
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
	bool MissedFlush = ContextManager.mOnDevicesChangedPromises.PopMissedFlushes();
	auto Promise = ContextManager.mOnDevicesChangedPromises.AddPromise( Params.mContext );
	
	//	need to flush here the first time...
	//	gr: + if there's a change on a thread, and we haven't re-subscribed yet...
	//		need something better for auto-flushing
	if ( MissedFlush )
	{
		ContextManager.EnumDevices( Params.mContext, Promise );
	}
	
	Params.Return( Promise );
}


void ApiInput::EnumDevices(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();
	ContextManager.EnumDevices( Params.mContext, Promise );
	Params.Return( Promise );
}



void TInputDeviceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
	mDevice.reset( new Hid::TDevice( ContextManager.mContext, DeviceName) );
	mDevice->mOnStateChanged = [this]()
	{
		this->mOnStateChangedPromises.Resolve();
	};
}


void TInputDeviceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiInput::GetState_FunctionName>( GetState );
	Template.BindFunction<ApiInput::OnStateChanged_FunctionName>( OnStateChanged );

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


void TInputDeviceWrapper::OnStateChanged(Bind::TCallback& Params)
{
	auto& This = Params.This<TInputDeviceWrapper>();
	
	auto Promise = This.mOnStateChangedPromises.AddPromise( Params.mContext );
	
	if ( This.mOnStateChangedPromises.PopMissedFlushes() )
		This.mDevice->mOnStateChanged();
	
	Params.Return( Promise );
}
