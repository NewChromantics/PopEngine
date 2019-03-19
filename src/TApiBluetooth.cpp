#include "TApiBluetooth.h"


const char EnumDevices_FunctionName[] = "EnumDevices";
const char BluetoothDevice_TypeName[] = "Device";


namespace ApiBluetooth
{
	const char Namespace[] = "Pop.Bluetooth";
	
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

	Context.BindGlobalFunction<EnumDevices_FunctionName>( ApiBluetooth::EnumDevices, Namespace );
}



void ApiBluetooth::EnumDevices(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();

	auto DoEnumDevices = [=](Bind::TContext& Context)
	{
		auto& Manager = GetBluetoothContext();
		Array<Bind::TObject> Devices;
		auto OnDevice = [&](Bluetooth::TDeviceMeta& DeviceMeta)
		{
			auto Device = Context.CreateObjectInstance();
			Device.SetString("Name", DeviceMeta.mName);
			Devices.PushBack(Device);
		};
		Manager.EnumDevicesWithService("", OnDevice);
		Promise.Resolve( GetArrayBridge(Devices) );
	};
	
	//	not on job/thread atm, but that's okay
	DoEnumDevices( Params.mContext );

	Params.Return( Promise );
}


