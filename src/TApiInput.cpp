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
	
	void					EnumDevices(Bind::TLocalContext& Context,Bind::TPromise& Promise);	//	return data into promise

	Bind::TPromiseQueue		mOnDevicesChangedPromises;
	Hid::TContext			mContext;
};


ApiInput::TContextManager& GetContextManager()
{
	static std::shared_ptr<ApiInput::TContextManager> gContextManager;
	if ( !gContextManager )
	{
		gContextManager.reset( new ApiInput::TContextManager() );
	}
	return *gContextManager;
}



ApiInput::TContextManager::TContextManager()
{
	mContext.mOnDevicesChanged = [this]()
	{
		if ( !mOnDevicesChangedPromises.HasContext() )
		{
			std::Debug << "OnDevicesChanged never requested, lost device-changed callback" << std::endl;
			return;
		}
		
		auto Flush = [this](Bind::TLocalContext& Context)
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

void ApiInput::TContextManager::EnumDevices(Bind::TLocalContext& Context,Bind::TPromise& Promise)
{
	Array<Soy::TInputDeviceMeta> DeviceMetas;
	auto EnumDevice = [&](Soy::TInputDeviceMeta& Meta)
	{
		DeviceMetas.PushBack( Meta );
	};

	GetContextManager().mContext.EnumDevices( EnumDevice );

	Array<JsCore::TObject> Devices;
	for ( auto i=0;	i<DeviceMetas.GetSize();	i++ )
	{
		auto& Meta = DeviceMetas[i];
		auto Object = Context.mGlobalContext.CreateObjectInstance( Context );
		Object.SetString("Name", Meta.mName );
		Object.SetString("Serial", Meta.mSerial );
		Object.SetString("Vendor", Meta.mVendor );
		Object.SetString("UsbPath", Meta.mUsbPath );
		Object.SetBool("Connected", Meta.mConnected );
		Devices.PushBack( Object );
	};
	auto DevicesArray = JsCore::GetArray( Context.mLocalContext, GetArrayBridge(Devices) );
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
	auto& ContextManager = GetContextManager();
	bool MissedFlush = ContextManager.mOnDevicesChangedPromises.PopMissedFlushes();
	auto Promise = ContextManager.mOnDevicesChangedPromises.AddPromise( Params.mLocalContext );
	
	//	need to flush here the first time...
	//	gr: + if there's a change on a thread, and we haven't re-subscribed yet...
	//		need something better for auto-flushing
	if ( MissedFlush )
	{
		ContextManager.EnumDevices( Params.mLocalContext, Promise );
	}
	
	Params.Return( Promise );
}


void ApiInput::EnumDevices(Bind::TCallback& Params)
{
	auto& ContextManager = GetContextManager();
	auto Promise = Params.mContext.CreatePromise( Params.mLocalContext, __FUNCTION__);
	ContextManager.EnumDevices( Params.mLocalContext, Promise );
	Params.Return( Promise );
}



void TInputDeviceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
	auto& ContextManager = GetContextManager();
	mDevice.reset( new Hid::TDevice( ContextManager.mContext, DeviceName) );
	mDevice->mOnStateChanged = [this]()
	{
		auto Resolve = [this](Bind::TLocalContext& Context)
		{
			this->mOnStateChangedPromises.Resolve();
		};
		this->mContext.Queue(Resolve);
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
	
	auto State = Params.mContext.CreateObjectInstance( Params.mLocalContext );
	State.SetArray("Buttons", GetArrayBridge(InputState.mButton) );

	Array<Bind::TObject> AxisObjects;
	for ( auto i=0;	i<InputState.mAxis.GetSize();	i++ )
	{
		auto& Axis = InputState.mAxis[i];
		auto AxisObject = Params.mContext.CreateObjectInstance( Params.mLocalContext );
		AxisObject.SetFloat("x", Axis.x );
		AxisObject.SetFloat("y", Axis.y );
		AxisObjects.PushBack( AxisObject );
	};
	State.SetArray("Axis", GetArrayBridge(AxisObjects) );

	Params.Return( State );
}


void TInputDeviceWrapper::OnStateChanged(Bind::TCallback& Params)
{
	auto& This = Params.This<TInputDeviceWrapper>();
	
	auto Promise = This.mOnStateChangedPromises.AddPromise( Params.mLocalContext );
	
	if ( This.mOnStateChangedPromises.PopMissedFlushes() )
		This.mDevice->mOnStateChanged();
	
	Params.Return( Promise );
}
