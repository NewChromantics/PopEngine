#include "TApiBluetooth.h"


const char EnumDevices_FunctionName[] = "EnumDevices";
const char OnDevicesChanged_FunctionName[] = "OnDevicesChanged";
const char Startup_FunctionName[] = "Startup";
//const char BluetoothDevice_TypeName[] = "Device";


namespace ApiBluetooth
{
	const char Namespace[] = "Pop.Bluetooth";
	
	void	Startup(Bind::TCallback& Params);
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
	
	std::shared_ptr<Bluetooth::TManager> mManager;
	Array<Bind::TPromise>		mPendingStartupPromises;
	Array<Bind::TPromise>		mPendingOnDevicesChangedPromises;
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
	Context.BindGlobalFunction<OnDevicesChanged_FunctionName>( OnDevicesChanged, Namespace );
	Context.BindGlobalFunction<EnumDevices_FunctionName>( EnumDevices, Namespace );
}



void ApiBluetooth::Startup(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();
	
	auto& Instance = GetBluetoothInstance();
	Instance.AddStartupPromise(Promise);
	
	Params.Return( Promise );
}

void ApiBluetooth::EnumDevices(Bind::TCallback& Params)
{
	//	future planning
	auto Promise = Params.mContext.CreatePromise();

	auto DoEnumDevices = [=](Bind::TContext& Context)
	{
		Array<Bind::TObject> Devices;
		auto OnDevice = [&](Bluetooth::TDeviceMeta DeviceMeta)
		{
			auto Device = Context.CreateObjectInstance();
			Device.SetString("Name", DeviceMeta.mName);
			Devices.PushBack(Device);
		};
		auto& Instance = GetBluetoothInstance();
		for ( auto i=0;	i<Instance.mManager->mKnownDevices.GetSize();	i++ )
		{
			auto& Device = Instance.mManager->mKnownDevices[i];
			OnDevice( Device );
		}

		Promise.Resolve( GetArrayBridge(Devices) );
	};

	//	currently looking for ones we've already found
	DoEnumDevices( Params.mContext );

	Params.Return( Promise );
}



void ApiBluetooth::OnDevicesChanged(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();
	
	auto& Instance = GetBluetoothInstance();
	Instance.AddOnDevicesChangedPromise(Promise);
	
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
}


void ApiBluetooth::TManagerInstance::AddStartupPromise(Bind::TPromise &Promise)
{
	mPendingStartupPromises.PushBack( Promise );
	
	//	check for immediate resolve
	auto State = this->mManager->GetState();
	OnStateChanged( State );
}

void ApiBluetooth::TManagerInstance::AddOnDevicesChangedPromise(Bind::TPromise &Promise)
{
	mPendingOnDevicesChangedPromises.PushBack( Promise );
}

void ApiBluetooth::TManagerInstance::OnStateChanged(Bluetooth::TState::Type NewState)
{
	//	only interested in a hard state
	switch ( NewState )
	{
		case Bluetooth::TState::Connected:
		case Bluetooth::TState::Disconnected:
		case Bluetooth::TState::Invalid:
			break;
		
		//	in-progress in someway
		default:
			return;
	}
	
	
	//	pop all promises in case they change during callback
	auto Promises = mPendingStartupPromises;
	mPendingStartupPromises.Clear();
	
	for ( auto p=0;	p<Promises.GetSize();	p++ )
	{
		auto& Promise = Promises[p];
		
		if ( NewState == Bluetooth::TState::Connected )
		{
			Promise.Resolve("Connected");
		}
		else if ( NewState == Bluetooth::TState::Disconnected )
		{
			Promise.Reject("Disconnected");
		}
		else
		{
			Promise.Reject("Not Supported");
		}
	}
}


void ApiBluetooth::TManagerInstance::OnDevicesChanged()
{
	//	pop all promises in case they change during callback
	auto Promises = mPendingOnDevicesChangedPromises;
	mPendingOnDevicesChangedPromises.Clear();
	
	for ( auto p=0;	p<Promises.GetSize();	p++ )
	{
		auto& Promise = Promises[p];
		Promise.Resolve("Changed");
	}
}


