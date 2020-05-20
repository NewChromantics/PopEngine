#include "SoyBluetooth.h"
#import <CoreBluetooth/CoreBluetooth.h>
#include <functional>
#include "SoyLib/src/SoyString.h"
#include "SoyLib/src/SoyDebug.h"
#include <thread>

//	need a better place in SoyLib for this
namespace Platform
{
	void NSDataToArray(NSData* Data,ArrayBridge<uint8>&& Array)
	{
		auto DataSize = Data.length;
		auto DataArray = GetRemoteArray( (uint8*)Data.bytes, DataSize );
		Array.PushBackArray( DataArray );
	}
	
	NSData* ArrayToNSData(const ArrayBridge<uint8>& Array);
	NSData* ArrayToNSData(const ArrayBridge<uint8>&& Array)	{	return ArrayToNSData(Array);	};
}

namespace Bluetooth
{
	TDeviceMeta		GetMeta(CBPeripheral* Device);
	TState::Type	GetState(CBPeripheralState CbState);
}

NSData* Platform::ArrayToNSData(const ArrayBridge<uint8>& Array)
{
	void* Data = (void*)Array.GetArray();
	return [[NSData alloc] initWithBytesNoCopy:Data length:Array.GetDataSize() freeWhenDone:false];
}


//	good example reference
//	https://github.com/DFRobot/BlunoBasicDemo/blob/master/IOS/BlunoBasicDemo/BlunoTest/Bluno/DFBlunoManager.m
@protocol BluetoothManagerDelegate;
@protocol BluetoothDeviceDelegate;


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


@interface BluetoothDeviceDelegate : NSObject<CBPeripheralDelegate>
{
	Bluetooth::TPlatformDeviceDelegate*	mParent;
}

- (id)initWithParent:(Bluetooth::TPlatformDeviceDelegate*)parent;

- (void)peripheralDidUpdateName:(CBPeripheral *)peripheral NS_AVAILABLE(10_9, 6_0);
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(nullable NSError *)error;
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(nullable NSError *)error;
- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(nullable NSError *)error;

@end

//	keep this class as minimal as possible. High level stuff in TDevice
class Bluetooth::TPlatformDeviceDelegate
{
public:
	TPlatformDeviceDelegate(CBPeripheral* Peripheral,std::function<void()> OnChanged,std::function<void(ArrayBridge<uint8_t>&)> OnRecv) :
		mPeripheral	( Peripheral ),
		mOnChanged	( OnChanged ),
		mOnRecv		( OnRecv )
	{
		auto* Delegate = [[BluetoothDeviceDelegate alloc] initWithParent:this ];
		mDelegate.Retain( Delegate );
		[mPeripheral.mObject setDelegate:mDelegate.mObject];
	}
	
	void 	OnRecv(ArrayBridge<uint8_t>&& Data)	{	if ( mOnRecv )	mOnRecv( Data );	}
	void	OnNameChanged()			{	if ( mOnChanged )	mOnChanged();	}
	void	OnServicesChanged()		{	if ( mOnChanged )	mOnChanged();	}
	
public:
	std::function<void()>						mOnChanged;
	std::function<void(ArrayBridge<uint8_t>&)>	mOnRecv;		//	should probbaly send UUID of characteristic here
	ObjcPtr<CBPeripheral>						mPeripheral;
	ObjcPtr<BluetoothDeviceDelegate>			mDelegate;
};


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
	if ( !Device.mPlatformDeviceDelegate )
		return nullptr;
	return Device.mPlatformDeviceDelegate->mPeripheral.mObject;
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


void Bluetooth::TManager::UpdateDeviceMeta(TDevice& Device)
{
	SetDeviceState( Device, Device.mState );
}

void Bluetooth::TManager::OnDeviceRecv(TDevice& Device,ArrayBridge<uint8_t>& Data)
{
	if ( !mOnDeviceRecv )
	{
		std::Debug << "No device recv() callback, " << Data.GetDataSize() << " bytes lost" << std::endl;
		return;
	}

	//	add to buffer & notify
	Device.mRecvBuffer.Push( Data );
	mOnDeviceRecv( Device );
}


void Bluetooth::TManager::SetDeviceState(TDevice& Device,TState::Type NewState)
{
	auto* Peripheral = GetPeripheral( Device );
	auto* PlatformDevice = GetPlatformDevice( Peripheral );
	SetDeviceState( PlatformDevice, NewState );
}

void Bluetooth::TManager::SetDeviceState(TPlatformDevice* PlatformDevice,TState::Type NewState)
{
	auto* Peripheral = GetPeripheral( PlatformDevice );
	auto Meta = GetMeta( Peripheral );
	
	//	here is where we have to see if uuids are duplicated for different devices...
	auto& Device = GetDevice( Meta.mUuid );

	//	set platform pointer
	if ( !Device.mPlatformDeviceDelegate && PlatformDevice )
	{
		//	gr: need to retain before connecting
		//	this will be replaced soon with a proper type
		auto OnMetaChanged = [this,&Device]()
		{
			this->UpdateDeviceMeta( Device );
		};
		auto OnRecv = [this,&Device](ArrayBridge<uint8_t>& Data)
		{
			this->OnDeviceRecv( Device, Data );
		};
		Device.mPlatformDeviceDelegate.reset( new TPlatformDeviceDelegate(Peripheral, OnMetaChanged, OnRecv ) );
	}
	
	//	update name
	if ( Device.mMeta.mName.length() == 0 )
		Device.mMeta.mName = Meta.mName;

	//	update services
	for ( auto i=0;	i<Meta.mServices.GetSize();	i++ )
		Device.mMeta.mServices.PushBackUnique( Meta.mServices[i] );
	
	Device.SubscribeToCharacteristics();
	
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
	
	if ( Device.mState == TState::Connecting )
	{
		std::Debug << "Warning, device already Connecting and trying to-reconnect..." << std::endl;
		return;
	}

	auto* Peripheral = GetPeripheral( Device );
	if ( !Peripheral )
	{
		throw Soy::AssertException("Couldn't find peripheral, currently need to discover before connect");
	}

	auto* PlatformDevice = GetPlatformDevice(Peripheral);
	SetDeviceState( PlatformDevice, TState::Connecting );
	std::Debug << "Connecting to peripheral: " << Uuid << std::endl;
	[mContext->mPlatformManager connectPeripheral:Peripheral options:nil];
}

void Bluetooth::TManager::DeviceListen(const std::string& DeviceUuid,const std::string& Char)
{
	auto& Device = GetDevice(DeviceUuid);
	
	if ( !Device.mPlatformDeviceDelegate )
	{
		throw Soy::AssertException("Couldn't find delegate, device needs to connect first.");
	}
	
	Device.SubscribeToCharacteristics( Char );
}

void Bluetooth::TManager::DisconnectDevice(const std::string& Uuid)
{
	auto& Device = GetDevice(Uuid);
	auto* Peripheral = GetPeripheral(Device);
	if ( Peripheral )
	{
		SetDeviceState( Device, TState::Disconnecting );
		[mContext->mPlatformManager connectPeripheral:Peripheral options:nil];
	}
	else
	{
		SetDeviceState( Device, TState::Disconnected );
	}
}

void Bluetooth::TDevice::SubscribeToCharacteristics(const std::string& NewChracteristic)
{
	if ( NewChracteristic.length() )
	{
		//	validate the UUID
		@try
		{
			/*auto* CharUid =*/ [CBUUID UUIDWithString:Soy::StringToNSString(NewChracteristic)];
		}
		@catch(NSException* e)
		{
			std::stringstream Error;
			Error << NewChracteristic << " is not a valid UUID";
			throw Soy::AssertException(Error.str());
		}
		
		mPendingCharacteristics.PushBackUnique( NewChracteristic );
	}
	
	//	nothing to do
	if ( mPendingCharacteristics.IsEmpty() )
		return;
	
	if ( !mPlatformDeviceDelegate )
	{
		std::Debug << "Cannot subscribe yet, waiting to connect" << std::endl;
		return;
	}
	
	auto* Peripheral = mPlatformDeviceDelegate->mPeripheral.mObject;
	if ( !Peripheral )
		throw Soy::AssertException("Expected peripheral");
	
	//	currently we just search all services for this characteristic.
	//	maybe some devices have multiple characteristics for multiple services...
	for ( int pc=mPendingCharacteristics.GetSize()-1;	pc>=0;	pc-- )
	{
		auto& PendingCharacteristic = mPendingCharacteristics[pc];
		auto* CharUid = [CBUUID UUIDWithString:Soy::StringToNSString(PendingCharacteristic)];
		auto WasSet = 0;
		
		for ( CBService* service in Peripheral.services )
		{
			for ( CBCharacteristic* characteristic in service.characteristics )
			{
				auto CharacteristicString = Soy::NSStringToString( [characteristic.UUID UUIDString] );
				if ( ![characteristic.UUID isEqual:CharUid])
				{
					std::Debug << "Found characteristic " << CharacteristicString << ", looking for " << PendingCharacteristic << std::endl;
					continue;
				}
				
				auto CanSubscribe = (characteristic.properties & CBCharacteristicPropertyNotify)!=0;
				if ( !CanSubscribe )
				{
					std::Debug << "Not able to subscribe to " << PendingCharacteristic << std::endl;
				}
				
				auto enable = YES;
				[Peripheral setNotifyValue:enable forCharacteristic:characteristic];
				WasSet++;
				std::Debug << mMeta.GetName() << " subscribed to " << PendingCharacteristic << std::endl;
				
				//	trigger a read of current data
				[Peripheral readValueForCharacteristic:characteristic];
			}
		}
		
		if ( !WasSet )
			continue;
		
		mPendingCharacteristics.RemoveBlock(pc,1);
	}
	
	if ( !mPendingCharacteristics.IsEmpty() )
	{
		std::Debug << "Bluetooth device " << mMeta.GetName() << " still waiting to subscribe to ";
		for ( auto i=0;	i<mPendingCharacteristics.GetSize();	i++ )
			std::Debug << mPendingCharacteristics[i] << ",";
		std::Debug << std::endl;
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
	//	if you subscribe to specific services, it'll find characteristics
	auto* ServiceFilter = GetServices("dfb0");
	//[peripheral discoverServices:ServiceFilter];
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




@implementation BluetoothDeviceDelegate


- (id)initWithParent:(Bluetooth::TPlatformDeviceDelegate*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
	}
	return self;
}

- (void)peripheralDidUpdateName:(CBPeripheral *)peripheral
{
	mParent->OnNameChanged();
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(nullable NSError *)error
{
	//	find all characteristics
	for ( CBService* service in peripheral.services )
	{
		[peripheral discoverCharacteristics:nil forService:service];
	}
	
	if ( error )
		std::Debug << "peripheral didDiscoverServices " << Soy::NSErrorToString(error) << std::endl;
	mParent->OnServicesChanged();
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(nullable NSError *)error
{
	if ( error )
		std::Debug << "peripheral didDiscoverCharacteristicsForService " << Soy::NSErrorToString(error) << std::endl;
	mParent->OnServicesChanged();
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(nullable NSError *)error
{
	if ( error )
		std::Debug << "peripheral didUpdateValueForCharacteristic " << Soy::NSErrorToString(error) << std::endl;

	Array<uint8_t> Data;
	if ( characteristic.value )
	{
		Platform::NSDataToArray( characteristic.value, GetArrayBridge(Data) );
	}
	mParent->OnRecv( GetArrayBridge(Data) );
	
	std::Debug << "recv (x" << Data.GetSize() << "): ";
	for ( auto i=0;	i<Data.GetSize();	i++ )
	{
		std::Debug << (char)Data[i];
	}
	std::Debug << std::endl;

	//	read more!
	//[peripheral readValueForCharacteristic:characteristic];
}


@end
