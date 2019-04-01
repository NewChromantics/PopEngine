#include "TApiBluetooth.h"


namespace ApiBluetooth
{
	const char Namespace[] = "Pop.Bluetooth";

	DEFINE_BIND_FUNCTIONNAME(EnumDevices);
	DEFINE_BIND_FUNCTIONNAME(OnDevicesChanged);
	DEFINE_BIND_FUNCTIONNAME(OnStatusChanged);
	DEFINE_BIND_FUNCTIONNAME(Startup);
	
	DEFINE_BIND_FUNCTIONNAME(Connect);
	DEFINE_BIND_FUNCTIONNAME(ReadCharacteristic);


	void	Startup(Bind::TCallback& Params);
	void	OnStatusChanged(Bind::TCallback& Params);
	void	EnumDevices(Bind::TCallback& Params);
	void	OnDevicesChanged(Bind::TCallback& Params);

	class TManagerInstance;
	TManagerInstance&	GetBluetoothInstance();
}

namespace Bluetooth
{
	class TContext;
}


class ApiBluetooth::TManagerInstance
{
public:
	TManagerInstance();
	
	void					OnStateChanged(Bluetooth::TState::Type NewState);
	void					OnDevicesChanged();
	void					AddStartupPromise(Bind::TPromise& Promise);
	void					AddOnDevicesChangedPromise(Bind::TPromise& Promise);
	std::shared_ptr<Bluetooth::TDeviceHandle>	AllocDevice(const std::string& Uuid);

private:
	void					OnDeviceChanged(Bluetooth::TDevice& Device);
	void					OnDeviceRecv(Bluetooth::TDevice& Device);

public:
	Array<std::shared_ptr<Bluetooth::TDeviceHandle>>	mHandles;	
	
	std::shared_ptr<Bluetooth::TManager> mManager;
	Bind::TPromiseQueue		mStartupPromises;
	Bind::TPromiseQueue		mOnDevicesChangedPromises;
};


ApiBluetooth::TManagerInstance& ApiBluetooth::GetBluetoothInstance()
{
	static std::shared_ptr<TManagerInstance> gInstance;
	if ( gInstance )
		return *gInstance;
	
	gInstance.reset( new TManagerInstance );
	return *gInstance;
}


void ApiBluetooth::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<Startup_FunctionName>( Startup, Namespace );
	Context.BindGlobalFunction<OnStatusChanged_FunctionName>( OnStatusChanged, Namespace );
	Context.BindGlobalFunction<OnDevicesChanged_FunctionName>( OnDevicesChanged, Namespace );
	Context.BindGlobalFunction<EnumDevices_FunctionName>( EnumDevices, Namespace );

	Context.BindObjectType<TBluetoothDeviceWrapper>( Namespace );
}



void ApiBluetooth::Startup(Bind::TCallback& Params)
{
	std::string ServiceFilter;
	if ( !Params.IsArgumentUndefined(0) )
		ServiceFilter = Params.GetArgumentString(0);
	
	auto& Instance = GetBluetoothInstance();
	Instance.mManager->Scan(ServiceFilter);
	
	auto Promise = Instance.mStartupPromises.AddPromise( Params.mContext );
	
	//	check for immediate resolve
	auto State = Instance.mManager->GetState();
	Instance.OnStateChanged( State );
	
	Params.Return( Promise );
}




//	gr: same as Startup() but doesn't resolve instantly if connected
void ApiBluetooth::OnStatusChanged(Bind::TCallback& Params)
{
	auto& Instance = GetBluetoothInstance();
	auto Promise = Instance.mStartupPromises.AddPromise( Params.mContext );
	Params.Return( Promise );
}


void ApiBluetooth::EnumDevices(Bind::TCallback& Params)
{
	//	future planning
	auto Promise = Params.mContext.CreatePromise(__FUNCTION__);

	auto DoEnumDevices = [=](Bind::TContext& Context)
	{
		Array<Bind::TObject> Devices;
		auto OnDevice = [&](Bluetooth::TDeviceMeta& DeviceMeta)
		{
			auto Device = Context.CreateObjectInstance();
			Device.SetString("Name", DeviceMeta.GetName() );
			Device.SetString("Uuid", DeviceMeta.mUuid);
			Device.SetArray("Services", GetArrayBridge(DeviceMeta.mServices) );
			Devices.PushBack(Device);
		};
		auto& Instance = GetBluetoothInstance();
		for ( auto i=0;	i<Instance.mManager->mDevices.GetSize();	i++ )
		{
			auto& Device = Instance.mManager->mDevices[i];
			OnDevice( Device->mMeta );
		}

		Promise.Resolve( GetArrayBridge(Devices) );
	};

	//	currently looking for ones we've already found
	DoEnumDevices( Params.mContext );

	Params.Return( Promise );
}



void ApiBluetooth::OnDevicesChanged(Bind::TCallback& Params)
{
	auto& Instance = GetBluetoothInstance();

	auto Promise = Instance.mOnDevicesChangedPromises.AddPromise( Params.mContext );

	Params.Return( Promise );
}

ApiBluetooth::TManagerInstance::TManagerInstance()
{
	mManager.reset( new Bluetooth::TManager );

	auto StateChanged = [this](Bluetooth::TState::Type State)
	{
		this->OnStateChanged( State );
	};
	mManager->mOnStateChanged = StateChanged;

	auto DevicesChanged = [this]()
	{
		this->OnDevicesChanged();
	};
	mManager->mOnDevicesChanged = DevicesChanged;
	
	auto DeviceChanged = [this](Bluetooth::TDevice& Device)
	{
		this->OnDeviceChanged(Device);
	};
	mManager->mOnDeviceChanged = DeviceChanged;
	
	auto DeviceRecv = [this](Bluetooth::TDevice& Device)
	{
		this->OnDeviceRecv(Device);
	};
	mManager->mOnDeviceRecv = DeviceChanged;
}


void ApiBluetooth::TManagerInstance::OnStateChanged(Bluetooth::TState::Type NewState)
{
	switch ( NewState )
	{
		case Bluetooth::TState::Connected:
			mStartupPromises.Resolve("Connected");
			return;
		
		case Bluetooth::TState::Disconnected:
			mStartupPromises.Reject("Disconnected");
			return;
	
		case Bluetooth::TState::Unsupported:
			mStartupPromises.Reject("Not Supported");
			break;
		
		//	in-progress in someway, don't react
		default:
			break;
	}
}


void ApiBluetooth::TManagerInstance::OnDevicesChanged()
{
	mOnDevicesChangedPromises.Resolve("Devices changed");
}


void ApiBluetooth::TManagerInstance::OnDeviceChanged(Bluetooth::TDevice& Device)
{
	//	find a matching handle
	std::shared_ptr<Bluetooth::TDeviceHandle> Handle;
	for ( auto i=0;	i<mHandles.GetSize();	i++ )
		if ( Device == mHandles[i]->mUuid )
			Handle = mHandles[i];
	if ( !Handle )
		return;
	
	if ( Handle->mOnStateChanged )
		Handle->mOnStateChanged();
}

std::shared_ptr<Bluetooth::TDeviceHandle> ApiBluetooth::TManagerInstance::AllocDevice(const std::string& Uuid)
{
	std::shared_ptr<Bluetooth::TDeviceHandle> Handle( new Bluetooth::TDeviceHandle );
	
	Handle->mUuid = Uuid;
	Handle->mGetState = [this,Uuid]()
	{
		auto& Device = mManager->GetDevice( Uuid );
		return Device.mState;
	};
	
	mHandles.PushBack( Handle );
	return Handle;
}


void TBluetoothDeviceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceUuid = Params.GetArgumentString(0);
	auto& Instance = ApiBluetooth::GetBluetoothInstance();
	
	mDevice = Instance.AllocDevice( DeviceUuid );
	mDevice->mOnStateChanged = [this]()	{	this->OnStateChanged();	};
}


void TBluetoothDeviceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiBluetooth::Connect_FunctionName>( Connect );
	Template.BindFunction<ApiBluetooth::ReadCharacteristic_FunctionName>( ReadCharacteristic );
}



void TBluetoothDeviceWrapper::Connect(Bind::TCallback& Params)
{
	auto& This = Params.This<TBluetoothDeviceWrapper>();

	auto Promise = This.mConnectPromises.AddPromise( Params.mContext );

	//	connect if not already connected
	if ( This.mDevice->GetState() == Bluetooth::TState::Connected )
	{
		//	resolve early if already connected
		This.OnStateChanged();
	}
	else
	{
		auto& Instance = ApiBluetooth::GetBluetoothInstance();
		Instance.mManager->ConnectDevice( This.mDevice->mUuid );
	}
	
	Params.Return( Promise );
}

void TBluetoothDeviceWrapper::OnStateChanged()
{
	auto State = mDevice->GetState();
	
	switch ( State )
	{
		case Bluetooth::TState::Connected:
			mConnectPromises.Resolve("Connected");
			break;
		
		case Bluetooth::TState::Disconnected:
			mConnectPromises.Reject("Disconnected");
			break;
		
		case Bluetooth::TState::Unsupported:
			mConnectPromises.Reject("Not Supported");
			break;
		
		//	in-progress in someway
		default:
			break;
	}
}


void TBluetoothDeviceWrapper::ReadCharacteristic(Bind::TCallback& Params)
{
	auto& This = Params.This<TBluetoothDeviceWrapper>();

	//	shouldn't need service really
	auto Characteristic = Params.GetArgumentString(0);

	if ( This.mReadCharacteristicUuid.length() != 0 )
	{
		if ( This.mReadCharacteristicUuid != Characteristic )
		{
			std::stringstream Error;
			Error << "Currently only supporting one characteristic at a time. Currently: " << This.mReadCharacteristicUuid << "; requesting: " << Characteristic;
			throw Soy::AssertException( Error.str() );
		}
	}
	
	This.mReadCharacteristicUuid = Characteristic;
	
	//	subscribe in case it hasn't already
	auto& Instance = ApiBluetooth::GetBluetoothInstance();
	Instance.mManager->DeviceListen( This.mDevice->mUuid, Characteristic );

	auto Promise = This.mReadCharacteristicPromises.AddPromise( Params.mContext );

	//	flush any data that might already be pending
	BufferArray<uint8_t,1> NewData;
	This.OnRecvData( Characteristic, GetArrayBridge(NewData) );
	
	Params.Return( Promise );
}


void TBluetoothDeviceWrapper::OnRecvData(const std::string& Characteristic,ArrayBridge<uint8_t>&& NewData)
{
	{
		std::lock_guard<std::mutex> Lock( mReadCharacteristicBufferLock );
		mReadCharacteristicBuffer.PushBackArray( NewData );
	}
	
	//	flush data
	if ( !mReadCharacteristicPromises.HasPromises() )
		return;
	if ( mReadCharacteristicBuffer.IsEmpty() )
		return;
	
	auto Flush = [this](Bind::TContext& Context)
	{
		//	turn data to an array
		Array<uint8_t> PoppedData;
		{
			std::lock_guard<std::mutex> Lock( mReadCharacteristicBufferLock );
			PoppedData = mReadCharacteristicBuffer;
			mReadCharacteristicBuffer.Clear();
		}
		auto Data = Context.CreateArray( GetArrayBridge(PoppedData) );
		auto HandlePromise = [&](Bind::TPromise& Promise)
		{
			Promise.Resolve( Data );
		};
		mReadCharacteristicPromises.Flush( HandlePromise );
	};
	auto& Context = mReadCharacteristicPromises.GetContext();
	Context.Queue( Flush );
}

