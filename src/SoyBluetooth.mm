#include "SoyBluetooth.h"
#import <CoreBluetooth/CoreBluetooth.h>
#include <functional>
#include "SoyLib/src/SoyString.h"
#include "SoyLib/src/SoyDebug.h"
#include <thread>


namespace Bluetooth
{
	TDeviceMeta		GetMeta(CBPeripheral* Device);
	TState::Type	GetState(CBPeripheralState CbState);
}

//	good example reference
//	https://github.com/DFRobot/BlunoBasicDemo/blob/master/IOS/BlunoBasicDemo/BlunoTest/Bluno/DFBlunoManager.m
@protocol BluetoothManagerDelegate;

@interface BluetoothManagerDelegate : NSObject<CBCentralManagerDelegate>
{
	Bluetooth::TContext*	mParent;
}

- (id)initWithParent:(Bluetooth::TContext*)parent;
- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *, id> *)advertisementData RSSI:(NSNumber *)RSSI;

@end


class Bluetooth::TContext
{
public:
	TContext(TManager& Manager);
	~TContext();
	
	Bluetooth::TState::Type	GetState();
	
	//	start a scan which operates in the background
	void	ScanForDevicesWithService(const std::string& ServiceUuid);
	
public:
	TManager&							mManager;
	CBCentralManager*					mPlatformManager = nullptr;
	ObjcPtr<BluetoothManagerDelegate>	mPlatformDelegate;
	
};

std::ostream& operator<<(std::ostream &out,const CBManagerState &in)
{
	switch(in)
	{
		case CBManagerStateUnknown:			out << "CBManagerStateUnknown";	break;
		case CBManagerStateResetting:		out << "CBManagerStateResetting";	break;
		case CBManagerStateUnsupported:		out << "CBManagerStateUnsupported";	break;
		case CBManagerStateUnauthorized:	out << "CBManagerStateUnauthorized";	break;
		case CBManagerStatePoweredOff:		out << "CBManagerStatePoweredOff";	break;
		case CBManagerStatePoweredOn:		out << "CBManagerStatePoweredOn";	break;
		default:
			out << "<unknown state " << static_cast<int>(in) << ">";
			break;
	}
	return out;
}


Bluetooth::TState::Type Bluetooth::GetState(CBPeripheralState CbState)
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
	
	//	get uuid first
	auto* UuidString = [Device.identifier UUIDString];
	Meta.mUuid = Soy::NSStringToString( UuidString );
	
	if ( !Device.name )
	{
		Meta.mName = Meta.mUuid;
	}
	else
	{
		Meta.mName = Soy::NSStringToString( Device.name );
	}

	Meta.mState = GetState( Device.state );
	return Meta;
}


NSArray<CBUUID*>* GetServices(const std::string& ServiceUuid)
{
	NSArray<CBUUID*>* Services = nil;
	if ( ServiceUuid.length() )
	{
		auto* ServiceUuidString = Soy::StringToNSString(ServiceUuid);
		auto* Uuid = [CBUUID UUIDWithString:ServiceUuidString];
		Services = @[Uuid];
	}
	return Services;
}


Bluetooth::TContext::TContext(TManager& Manager) :
	mManager	( Manager )
{
	mPlatformDelegate.Retain( [[BluetoothManagerDelegate alloc] initWithParent:this] );
	mPlatformManager = [[CBCentralManager alloc] initWithDelegate:mPlatformDelegate.mObject queue:nil];
}

Bluetooth::TContext::~TContext()
{
	[mPlatformManager release];
	mPlatformManager = nil;
	mPlatformDelegate.Release();
}

Bluetooth::TState::Type Bluetooth::TContext::GetState()
{
	auto State = mPlatformManager.state;
	
	switch(State)
	{
		case CBManagerStateUnknown:
		case CBManagerStateResetting:
		return Bluetooth::TState::Connecting;
		
		case CBManagerStateUnsupported:
			return Bluetooth::TState::Invalid;
		
		case CBManagerStateUnauthorized:
		case CBManagerStatePoweredOff:
		return Bluetooth::TState::Disconnected;
		
		case CBManagerStatePoweredOn:
		return Bluetooth::TState::Connected;
	}
	
	throw Soy::AssertException("Unknown bluetooth state");
}

void Bluetooth::TContext::ScanForDevicesWithService(const std::string& ServiceUuid)
{
	std::Debug << "Bluetooth scan (" << ServiceUuid << ") started" << std::endl;
	
	//	kick off a scan
	auto* Manager = mPlatformManager;
	@try
	{
		auto ManagerState = Manager.state;
		if ( ManagerState != CBManagerStatePoweredOn )
		{
			std::stringstream Error;
			Error << "Cannot start scan as manager is in state " << ManagerState << std::endl;
			throw Soy::AssertException(Error.str());
		}
		
		auto* Services = GetServices(ServiceUuid);
		//	gr: this scans for new devices. nil uid will retrieve all devices
		//		if already scanning the current scan will be replaced with this
		//		probably want a stack system or something. or at least manage it better
		//		with callbacks when it's finished etc
		[Manager stopScan];
		[Manager scanForPeripheralsWithServices:Services options:nil];
	}
	@catch (NSException* e)
	{
		auto Error = Soy::NSErrorToString(e);
		throw Soy::AssertException(Error);
	}
}

void Bluetooth::TManager::OnFoundDevice(TDeviceMeta DeviceMeta)
{
	auto* ExistingDevice = mKnownDevices.Find( DeviceMeta );
	if ( ExistingDevice )
	{
		//	todo: update missing state/meta of existing device
		return;
	}
	std::Debug << "Found new device: "  << DeviceMeta.mName << " (" << DeviceMeta.mUuid << ")" << std::endl;
	mKnownDevices.PushBack( DeviceMeta );
	
	if ( mOnDevicesChanged )
		mOnDevicesChanged();
}

void Bluetooth::TManager::OnStateChanged()
{
	auto State = mContext->GetState();
	
	//	kick off a scan
	if ( State == TState::Connected )
		mContext->ScanForDevicesWithService( std::string() );
	
	if ( mOnStateChanged )
	{
		mOnStateChanged( State );
	}
}

Bluetooth::TManager::TManager()
{
	mContext.reset( new TContext(*this) );
}

Bluetooth::TState::Type Bluetooth::TManager::GetState()
{
	return mContext->GetState();
}

/*
void Bluetooth::TManager::EnumConnectedDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta)> OnFoundDevice)
{
	auto* Manager = mContext->mManager;
	
	NSArray<CBPeripheral*>* Peripherals = nil;
	@try
	{
		auto* Services = GetServices(ServiceUuid);
		//	gr: this retrieves connected devices. UID cannot be nil
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
		OnFoundDevice(Meta);
	};
	Platform::NSArray_ForEach<CBPeripheral*>( Peripherals, EnumDevice );
}



void Bluetooth::TManager::EnumDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta)> OnFoundDevice)
{
	//	kick off a scan (don't stop old ones?)
	mContext->ScanForDevicesWithService( ServiceUuid );

	//	output everything we know of
	auto KnownDevices = mContext->mKnownDevices;
	for ( auto i=0;	i<KnownDevices.GetSize();	i++ )
	{
		auto& KnownDevice = KnownDevices[i];
		OnFoundDevice( KnownDevice );
	}

}
 */


@implementation BluetoothManagerDelegate


- (id)initWithParent:(Bluetooth::TContext*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
	}
	return self;
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *, id> *)advertisementData RSSI:(NSNumber *)RSSI
{
	auto Meta = Bluetooth::GetMeta( peripheral );
	mParent->mManager.OnFoundDevice( Meta );
	//std::Debug << "Found peripheral " << Meta.mName << " (" << Meta.mUuid << ")" << std::endl;
}


- (void)centralManagerDidUpdateState:(nonnull CBCentralManager *)central
{
	auto State = central.state;
	std::Debug << "Bluetooth manager state updated to " << State << std::endl;
	mParent->mManager.OnStateChanged();
}

@end
