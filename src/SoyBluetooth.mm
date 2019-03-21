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
- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral;
- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(nullable NSError *)error;
- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(nullable NSError *)error;

@end


class Bluetooth::TContext
{
public:
	TContext(TManager& Manager);
	~TContext();
	
	Bluetooth::TState::Type	GetState();
	
	//	start a scan which operates in the background
	void				ScanForDevicesWithService(const std::string& ServiceUuid);
	CBPeripheral*		GetPeripheral(const std::string& DeviceUid);
	
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
	Meta.mState = GetState( Device.state );

	//	get uuid first and use it for the name if there's no name
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
	
	auto EnumService = [&](CBService* Service)
	{
		auto Uuid = [Service.UUID UUIDString];
		auto UuidString = Soy::NSStringToString( Uuid );
		Meta.mServices.PushBack( UuidString );
	};
	Platform::NSArray_ForEach<CBService*>( Device.services, EnumService );

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



CBPeripheral* GetPeripheral(Bluetooth::TPlatformDevice* Device)
{
	auto* Peripheral = static_cast<CBPeripheral*>( Device );
	return Peripheral;
}

CBPeripheral* GetPeripheral(Bluetooth::TDevice& Device)
{
	return GetPeripheral( Device.mPlatformDevice );
}

Bluetooth::TPlatformDevice* GetPlatformDevice(CBPeripheral* Peripheral)
{
	auto* PlatformDevice = static_cast<Bluetooth::TPlatformDevice*>( Peripheral );
	return PlatformDevice;
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
			return Bluetooth::TState::Unknown;
		
		case CBManagerStateResetting:
		return Bluetooth::TState::Connecting;
		
		case CBManagerStateUnsupported:
			return Bluetooth::TState::Unsupported;
		
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



Bluetooth::TManager::TManager()
{
	mContext.reset( new TContext(*this) );
}


void Bluetooth::TManager::SetDeviceState(TPlatformDevice* PlatformDevice,TState::Type NewState)
{
	auto* Peripheral = GetPeripheral( PlatformDevice );
	auto Meta = GetMeta( Peripheral );
	
	//	here is where we have to see if uuids are duplicated for different devices...
	auto& Device = GetDevice( Meta.mUuid );

	//	set platform pointer
	if ( !Device.mPlatformDevice && PlatformDevice )
	{
		//	gr: need to retain before connecting
		//	this will be replaced soon with a proper type
		Device.mPlatformDevice = PlatformDevice;
		[Peripheral retain];
	}
	
	//	update name
	if ( Device.mMeta.mName.length() == 0 )
		Device.mMeta.mName = Meta.mName;

	//	update state
	//	todo: when we discover a device, gotta try and not override our connected state
	if ( Device.mState != NewState )
	{
		//	when unknown, we don't overwrite
		if ( NewState != TState::Unknown )
		{
			std::Debug << "Device (" << Device.mMeta.GetName() << ") state changed" << std::endl;
			Device.mState = NewState;
		}
	}
	
	OnDeviceChanged( Device );
	/*
	std::shared_ptr<TDevice> NewDevice( new TDevice(DeviceMeta.mUuid) );
	NewDevice->mMeta = DeviceMeta;
	std::Debug << "Found new device: "  << DeviceMeta.mName << " (" << DeviceMeta.mUuid << ")" << std::endl;
	mDevices.PushBack( NewDevice );
	
	if ( mOnDevicesChanged )
		mOnDevicesChanged();
	 */
}

void Bluetooth::TManager::Scan(const std::string& SpecificService)
{
	mScanService = SpecificService;
	auto State = this->GetState();
	if ( State == TState::Connected )
	{
		mContext->ScanForDevicesWithService( mScanService );
	}
}


void Bluetooth::TManager::OnStateChanged()
{
	auto State = mContext->GetState();
	
	//	kick off a scan
	if ( State == TState::Connected )
		mContext->ScanForDevicesWithService( mScanService );
	
	if ( mOnStateChanged )
	{
		mOnStateChanged( State );
	}
}

void Bluetooth::TManager::OnDeviceChanged(Bluetooth::TDevice& Device)
{
	if ( mOnDevicesChanged )
		mOnDevicesChanged();
	
	if ( mOnDeviceChanged )
		mOnDeviceChanged( Device );
}

Bluetooth::TState::Type Bluetooth::TManager::GetState()
{
	return mContext->GetState();
}

Bluetooth::TDevice& Bluetooth::TManager::GetDevice(const std::string& Uuid)
{
	std::shared_ptr<TDevice> pDevice;
	for ( auto i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = *mDevices[i];
		if ( Device.mMeta == Uuid )
			pDevice = mDevices[i];
	}
	
	if ( !pDevice )
	{
		pDevice.reset( new Bluetooth::TDevice() );
		pDevice->mMeta.mUuid = Uuid;
		mDevices.PushBack( pDevice );
	}
	
	return *pDevice;
}

void Bluetooth::TManager::ConnectDevice(const std::string& Uuid)
{
	auto& Device = GetDevice(Uuid);
	
	if ( Device.mState == TState::Connected )
		std::Debug << "Warning, device already connected and trying to-reconnect..." << std::endl;
	
	auto* Peripheral = GetPeripheral( Device );
	if ( !Peripheral )
	{
		throw Soy::AssertException("Couldn't find peripheral, currently need to discover before connect");
	}

	SetDeviceState( Device.mPlatformDevice, TState::Connecting );
	[mContext->mPlatformManager connectPeripheral:Peripheral options:nil];
}

void Bluetooth::TManager::DeviceRecv(const std::string& DeviceUuid,const std::string& Service,const std::string& Char)
{
	auto& Device = GetDevice(DeviceUuid);
	auto* peripheral = GetPeripheral( Device );
	if ( !peripheral )
	{
		throw Soy::AssertException("Couldn't find peripheral, currently need to discover before recv");
	}
	
	auto* ServiceUid = [CBUUID UUIDWithString:Soy::StringToNSString(Service)];
	auto* CharUid = [CBUUID UUIDWithString:Soy::StringToNSString(Char)];

	auto WasSet = 0;
	
	if ( peripheral.services == nil )
		std::Debug << "Warning, looking in services, but not discovered yet" << std::endl;
	
	for ( CBService* service in peripheral.services )
	{
		if ([service.UUID isEqual:ServiceUid])
		{
			for ( CBCharacteristic* characteristic in service.characteristics )
			{
				if ([characteristic.UUID isEqual:CharUid])
				{
					//	if (characteristic.properties & CBCharacteristicPropertyNotify) return YES;
					auto enable = YES;
					[peripheral setNotifyValue:enable forCharacteristic:characteristic];
					WasSet++;
				}
			}
		}
	}
	
	if ( WasSet == 0 )
		throw Soy::AssertException("Characteristic not found");
}

void Bluetooth::TManager::DisconnectDevice(const std::string& Uuid)
{
	auto& Device = GetDevice(Uuid);
	auto* Peripheral = GetPeripheral(Device);
	if ( Peripheral )
	{
		SetDeviceState( Device.mPlatformDevice, TState::Disconnecting );
		[mContext->mPlatformManager connectPeripheral:Peripheral options:nil];
	}
	else
	{
		SetDeviceState( Device.mPlatformDevice, TState::Disconnected );
	}
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
	//	tell this to list services (really wanna only do this once)
	if ( peripheral.services == nil )
	{
		/*
		NSArray<CBUUID *>* ServiceFilter = nil;
		[peripheral discoverServices:ServiceFilter];
		*/
	}
	
	auto* PlatformDevice = GetPlatformDevice( peripheral );
	mParent->mManager.SetDeviceState( PlatformDevice, Bluetooth::TState::Unknown );
	//std::Debug << "Found peripheral " << Meta.mName << " (" << Meta.mUuid << ")" << std::endl;
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
	//	good time to do this apparently
	//	https://github.com/DFRobot/BlunoBasicDemo/blob/master/IOS/BlunoBasicDemo/BlunoTest/Bluno/DFBlunoManager.m#L187
	[peripheral discoverServices:nil];
	
	auto* PlatformDevice = GetPlatformDevice( peripheral );
	mParent->mManager.SetDeviceState( PlatformDevice, Bluetooth::TState::Connected );
}

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(nullable NSError *)error
{
	auto* PlatformDevice = GetPlatformDevice( peripheral );
	mParent->mManager.SetDeviceState( PlatformDevice, Bluetooth::TState::Disconnected );
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(nullable NSError *)error
{
	auto* PlatformDevice = GetPlatformDevice( peripheral );
	mParent->mManager.SetDeviceState( PlatformDevice, Bluetooth::TState::Disconnected );

}


- (void)centralManagerDidUpdateState:(nonnull CBCentralManager *)central
{
	auto State = central.state;
	std::Debug << "Bluetooth manager state updated to " << State << std::endl;
	mParent->mManager.OnStateChanged();
}

@end
