#include "TApiBluetooth.h"


const char EnumDevices_FunctionName[] = "EnumDevices";
const char Startup_FunctionName[] = "Startup";
const char BluetoothDevice_TypeName[] = "Device";


namespace ApiBluetooth
{
	const char Namespace[] = "Pop.Bluetooth";
	
	void	Startup(Bind::TCallback& Params);
	void	EnumDevices(Bind::TCallback& Params);
}

namespace Bluetooth
{
	class TContext;
}

Bluetooth::TManager& GetBluetoothContext()
{
	static std::shared_ptr<Bluetooth::TManager> gContext;
	if ( gContext )
		return *gContext;
	
	gContext.reset( new Bluetooth::TManager );
	return *gContext;
}


void ApiBluetooth::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<EnumDevices_FunctionName>( EnumDevices, Namespace );
	Context.BindGlobalFunction<Startup_FunctionName>( Startup, Namespace );
}


void ApiBluetooth::Startup(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();
	
	auto WaitForConnected = [=]()
	{
		//	todo: timeout
		while ( true )
		{
			auto& Manager = GetBluetoothContext();
			auto State = Manager.GetState();
			
			if ( State == Bluetooth::TState::Connected )
			{
				Promise.Resolve("Connected");
				return;
			}
			
			if ( State == Bluetooth::TState::Disconnected )
			{
				Promise.Reject("Disconnected");
				return;
			}
			
			if ( State == Bluetooth::TState::Invalid )
			{
				Promise.Reject("Bluetooth not supported");
				return;
			}

			std::Debug << "Waiting on bluetooth state..." << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(500) );
		}
	};
	
	//	gr: this is super blocking, so need a thread!
	WaitForConnected();
	
	Params.Return( Promise );
}

void ApiBluetooth::EnumDevices(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();
	
	std::string Service;
	if ( !Params.IsArgumentUndefined(0) )
		Service = Params.GetArgumentString(0);
	
	auto DoEnumDevices = [=](Bind::TContext& Context)
	{
		auto& Manager = GetBluetoothContext();
		Array<Bind::TObject> Devices;
		auto OnDevice = [&](Bluetooth::TDeviceMeta DeviceMeta)
		{
			auto Device = Context.CreateObjectInstance();
			Device.SetString("Name", DeviceMeta.mName);
			Devices.PushBack(Device);
		};
		Manager.EnumDevicesWithService(Service, OnDevice);
		Promise.Resolve( GetArrayBridge(Devices) );
	};
	
	//	not on job/thread atm, but that's okay
	//	gr: this is super blocking, so need a thread!
	DoEnumDevices( Params.mContext );

	Params.Return( Promise );
}


