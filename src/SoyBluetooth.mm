#include "SoyBluetooth.h"
#import <CoreBluetooth/CoreBluetooth.h>
#include <functional>
#include "SoyLib/src/SoyString.h"


namespace Bluetooth
{
	TDeviceMeta		GetMeta(CBPeripheral* Device);
	TState::TYPE	GetState(CBPeripheralState CbState);
}

class Bluetooth::TContext
{
public:
	TContext();
	~TContext();
	
	CBCentralManager*	mManager = nullptr;
};


Bluetooth::TState::TYPE Bluetooth::GetState(CBPeripheralState CbState)
{
	switch ( CbState )
	{
		case CBPeripheralStateDisconnected:		return Bluetooth::TState::Disconnected;
		case CBPeripheralStateConnecting:		return Bluetooth::TState::Connecting;
		case CBPeripheralStateConnected:		return Bluetooth::TState::Connected;
		case CBPeripheralStateDisconnecting:	return Bluetooth::TState::Disconnecting;
		default: break;
	}
	throw Soy::AssertException("Unknown bluetooth peripheral state");
}

Bluetooth::TDeviceMeta Bluetooth::GetMeta(CBPeripheral* Device)
{
	Bluetooth::TDeviceMeta Meta;
	Meta.mName = Soy::NSStringToString( Device.name );
	Meta.mState = GetState( Device.state );
	return Meta;
}





Bluetooth::TContext::TContext()
{
	mManager = [[CBCentralManager alloc] init];
}

Bluetooth::TContext::~TContext()
{
}


Bluetooth::TManager::TManager()
{
	mContext.reset( new TContext() );
}

void Bluetooth::TManager::EnumDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta&)> FoundDevice)
{
	auto* Manager = mContext->mManager;
	
	NSArray<CBPeripheral*>* Peripherals = nil;
	@try
	{
		NSArray<CBUUID*>* Services = nil;
		auto* ServiceUuidString = Soy::StringToNSString(ServiceUuid);
		auto* Uuid = [CBUUID UUIDWithString:ServiceUuidString];
		Services = @[Uuid];
		Peripherals = [Manager retrieveConnectedPeripheralsWithServices:Services];
	}
	@catch (NSException* e)
	{
		auto Error = Soy::NSErrorToString(e);
		throw Soy::AssertException(Error);
	}
	
	auto EnumDevice = [&](CBPeripheral* Device)
	{
		auto Meta = GetMeta( Device );
		FoundDevice(Meta);
	};
	Platform::NSArray_ForEach<CBPeripheral*>( Peripherals, EnumDevice );
}


