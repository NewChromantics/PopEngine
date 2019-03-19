#include "TApiBluetooth.h"
#import <CoreBluetooth/CoreBluetooth.h>


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


class Bluetooth::TContext
{
public:
	TContext();
	
	CBCentralManager*	mManager = nullptr;
};

Bluetooth::TContext BluetoothContext;


void ApiBluetooth::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<EnumDevices_FunctionName>( ApiBluetooth::EnumDevices, Namespace );
}



void ApiBluetooth::EnumDevices(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();

	auto DoEnumDevices = [=]
	{
		Promise.Reject("todo");
	};
	
	//	not on job/thread atm, but that's okay
	DoEnumDevices();

	Params.Return( Promise );
}


Bluetooth::TContext::TContext()
{
	mManager = [[CBCentralManager alloc] init];
	mManager.retrievePeripheralsWithIdentifiers
	- (NSArray<CBPeripheral *> *)retrievePeripheralsWithIdentifiers:(NSArray<NSUUID *> *)identifiers NS_AVAILABLE(10_9, 7_0);

}

