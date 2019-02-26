#include "SoyHid.h"
#include "SoyLib/src/SoyDebug.h"


namespace Hid
{
	auto Allocator = kCFAllocatorDefault;
	
	void	IsOkay(IOReturn Result,const char* Context);
}


namespace Platform
{
	uint32_t		GetNumber(CFTypeRef Value);
	std::string		GetString(CFTypeRef Value);
	
}

uint32_t Platform::GetNumber(CFTypeRef Value)
{
	//	throw?
	if ( Value == nullptr )
		return 0;
	
	auto ValueType = CFGetTypeID(Value);
	if ( ValueType != CFNumberGetTypeID() )
	{
		auto sr2 = CFCopyDescription( Value );
		const char *psz = CFStringGetCStringPtr(sr2, kCFStringEncodingMacRoman);
		std::stringstream Error;
		Error << "CFTypeID value is not a number; " << psz;
		throw Soy::AssertException( Error.str() );
	}
	
	//	apparently you can just cast. http://idevapps.com/forum/showthread.php?tid=1220
	auto ValueNumber = reinterpret_cast<CFNumberRef>( Value );
	uint32_t Number32;
	if ( !CFNumberGetValue( ValueNumber, kCFNumberIntType, &Number32 ) )
	{
		throw Soy::AssertException("Failed to get int number");
	}
	
	return Number32;
}

std::string Platform::GetString(CFTypeRef Value)
{
	if ( Value == nullptr )
		return std::string();
	
	auto ValueType = CFGetTypeID(Value);
	
	if ( ValueType == CFNumberGetTypeID() )
	{
		auto Number = GetNumber(Value);
		std::stringstream NumberString;
		NumberString << Number;
		return NumberString.str();
	}
	
	if ( ValueType != CFStringGetTypeID() )
	{
		auto sr2 = CFCopyDescription( Value );
		const char *psz = CFStringGetCStringPtr(sr2, kCFStringEncodingMacRoman);
		std::stringstream Error;
		Error << "CFTypeID value is not a string; " << psz;
		throw Soy::AssertException( Error.str() );
	}
	
	//	apparently you can just cast. http://idevapps.com/forum/showthread.php?tid=1220
	auto ValueString = reinterpret_cast<CFStringRef>( Value );
	
	auto Encoding = kCFStringEncodingMacRoman;
	auto* StringChars = CFStringGetCStringPtr( ValueString, Encoding );
	if ( !StringChars )
		return std::string();
	
	return std::string( StringChars );
}


void Hid::IsOkay(IOReturn Result,const char *Context)
{
	if ( Result == kIOReturnSuccess )
		return;
	
	std::stringstream Error;
	Error << "IOKitHIDApi error: " << Result << " during " << Context;
	throw Soy::AssertException(Error.str());
}


bool Soy::TInputDeviceMeta::operator==(const TInputDeviceMeta& That) const
{
	if ( this->mName != That.mName )		return false;
	if ( this->mSerial != That.mSerial )	return false;
	if ( this->mVendor != That.mVendor )	return false;

	//	this could change?
	//if ( this->mUsbPath != That.mUsbPath )	return false;
	return true;
}

	
	
Hid::TContext::TContext()
{
	mManager = IOHIDManagerCreate( Allocator, kIOHIDOptionsTypeNone);

	ListenForDevices();
}

Hid::TContext::~TContext()
{
	//IOHIDManagerUnscheduleFromRunLoop(c->hid_manager, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
	IOHIDManagerClose( mManager, kIOHIDOptionsTypeNone );
	CFRelease( mManager );
}

//	https://github.com/suzukiplan/gamepad-osx/blob/master/gamepad.c
static void append_matching_dictionary(CFMutableArrayRef matcher, uint32_t page, uint32_t use)
{
	CFMutableDictionaryRef result = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!result) return;
	CFNumberRef pageNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
	CFDictionarySetValue(result, CFSTR(kIOHIDDeviceUsagePageKey), pageNumber);
	CFRelease(pageNumber);
	CFNumberRef useNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &use);
	CFDictionarySetValue(result, CFSTR(kIOHIDDeviceUsageKey), useNumber);
	CFRelease(useNumber);
	CFArrayAppendValue(matcher, result);
	CFRelease(result);
}

void Hid::TContext::ListenForDevices()
{
	CFMutableArrayRef Filter = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	//	gr: these are untyped enums that are 8bit...
	uint32_t _FilterUsages[] =
	{
		kHIDUsage_GD_Joystick,
		kHIDUsage_GD_GamePad,
		//kHIDUsage_GD_Keyboard,
		//kHIDUsage_GD_Mouse
	};
	auto FilterUsages = GetRemoteArray(_FilterUsages);
	for ( auto f=0;	f<FilterUsages.GetSize();	f++ )
	{
		auto Usage = FilterUsages[f];
		append_matching_dictionary( Filter, kHIDPage_GenericDesktop, Usage );
	}
	IOHIDManagerSetDeviceMatchingMultiple( mManager, Filter );
	CFRelease(Filter);
	
	auto OnConnected = [](void* Context,IOReturn Result,void* Sender,IOHIDDeviceRef Device)
	{
		auto* This = reinterpret_cast<TContext*>( Context );
		This->OnDeviceConnected( Device, Result );
	};
	
	auto OnDisconnected = [](void* Context,IOReturn Result,void* Sender,IOHIDDeviceRef Device)
	{
		auto* This = reinterpret_cast<TContext*>( Context );
		This->OnDeviceDisconnected( Device, Result );
	};

	IOHIDManagerRegisterDeviceMatchingCallback( mManager, OnConnected, this );
	IOHIDManagerRegisterDeviceRemovalCallback( mManager, OnDisconnected, this );

	IOHIDManagerScheduleWithRunLoop( mManager, CFRunLoopGetMain(), kCFRunLoopCommonModes );
	
	auto Result = IOHIDManagerOpen( mManager, kIOHIDOptionsTypeNone );
	IsOkay( Result, "IOHIDManagerOpen" );
}


void Hid::TContext::EnumDevices(std::function<void(Soy::TInputDeviceMeta&)> Enum)
{
	std::lock_guard<std::mutex> Lock( mDeviceMetasLock );
	for ( auto i=0;	i<mDeviceMetas.GetSize();	i++ )
	{
		auto& Meta = mDeviceMetas[i];
		Enum( Meta );
	}
}



std::string IOHIDDeviceGetProperty_AsString(IOHIDDeviceRef Device,CFStringRef Key)
{
	auto Value = IOHIDDeviceGetProperty( Device, Key );
	auto Str = Platform::GetString( Value );
	return Str;
}

ssize_t IOHIDDeviceGetProperty_AsNumber(IOHIDDeviceRef Device,CFStringRef Key)
{
	auto Value = IOHIDDeviceGetProperty( Device, Key );
	auto Number = Platform::GetNumber( Value );
	return Number;
}

Soy::TInputDeviceMeta GetMeta(IOHIDDeviceRef Device)
{
	Soy::TInputDeviceMeta Meta;
	
	auto Transport = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDTransportKey) );
	auto Product = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDProductKey) );
	auto Manufacturer = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDManufacturerKey) );
	auto Serial = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDSerialNumberKey) );
	auto StandardType = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDStandardTypeKey) );
	auto Usage = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDDeviceUsageKey) );
	auto PhysicalUnique = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDPhysicalDeviceUniqueIDKey) );
	
	//	numbers
	auto VendorSource = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDVendorIDSourceKey) );
	auto Vendor = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDVendorIDKey) );
	auto Version = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDVersionNumberKey) );
	auto ProductId = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDProductIDKey) );
	auto CountryCode = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDCountryCodeKey) );
	auto Location = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDLocationIDKey) );
	auto UniqueId = IOHIDDeviceGetProperty_AsString( Device, CFSTR(kIOHIDUniqueIDKey) );

	
	Meta.mName = Product;
	Meta.mConnected = true;
	
	Meta.mVendor = Manufacturer;
	if ( !Meta.mVendor.length() )
		Meta.mVendor = Vendor;
	
	Meta.mUsbPath = PhysicalUnique;
	if ( !Meta.mUsbPath.length() )
		Meta.mUsbPath = UniqueId;

	Meta.mSerial = Serial;
	if ( !Meta.mSerial.length() )
		Meta.mSerial = Meta.mUsbPath;
	

	return Meta;
}

void Hid::TContext::OnDeviceConnected(IOHIDDeviceRef Device,IOReturn Result)
{
	//	find existing or add
	auto Meta = GetMeta( Device );

	std::lock_guard<std::mutex> Lock( mDeviceMetasLock );
	
	auto* ExistingMeta = mDeviceMetas.Find( Meta );
	if ( ExistingMeta )
	{
		std::Debug << "Hid device re-connected" << Meta.mName << std::endl;
		ExistingMeta->mConnected = true;
		return;
	}
	
	std::Debug << "Hid device connected " << Meta.mName << std::endl;
	mDeviceMetas.PushBack( Meta );
}

void Hid::TContext::OnDeviceDisconnected(IOHIDDeviceRef Device,IOReturn Result)
{
	//	find existing and mark disconnected
	auto Meta = GetMeta( Device );

	std::lock_guard<std::mutex> Lock( mDeviceMetasLock );
	auto* ExistingMeta = mDeviceMetas.Find( Meta );
	if ( !ExistingMeta )
	{
		std::Debug << "Didn't know of Hid Device " << Meta.mName << std::endl;
		return;
	}
	std::Debug << "Hid device disconnected" << Meta.mName << std::endl;
	ExistingMeta->mConnected = false;
}


Hid::TDevice::TDevice(TContext& Context,const std::string& DeviceName)
{
	OpenDevice( Context, DeviceName );
}


Hid::TDevice::~TDevice()
{
	//mDevice = nullptr;
}

void Hid::TDevice::OpenDevice(TContext& Context,const std::string& Reference)
{
	/*
	auto OpenPath = [&](const std::string& Path)
	{
		//	could bail out early, but we don't want to hide ambiguity
		auto* NewDevice = hid_open_path( Path.c_str() );
		if ( mDevice && NewDevice )
		{
			std::stringstream Error;
			Error << "Already opened device, ambiguious name/serial/usb path " << Reference;
			throw Soy::AssertException( Error.str() );
		}
		mDevice = NewDevice;
		return mDevice != nullptr;
	};
	
	//	try path first
	if ( OpenPath(Reference) )
		return;
	
	//	enumerate devices and try and match name/serial
	auto OnDevice = [&](Soy::TInputDeviceMeta& Meta)
	{
		OpenPath( Meta.mName );
		OpenPath( Meta.mSerial );
		OpenPath( Meta.mUsbPath );
	};
	Context.EnumDevices( OnDevice );
	
	if ( !mDevice )
	{
		std::stringstream Error;
		Error << "Didn't find a device matching " << Reference;
		throw Soy::AssertException(Error.str());
	}
	
	//	setup device
	hid_set_nonblocking( mDevice, 1 );
	*/
}

Soy::TInputDeviceState Hid::TDevice::GetState()
{
	/*
	if ( !mDevice )
		throw Soy::AssertException("Missing device");
	
	uint8_t Buffer[17];
	
	// Request state (cmd 0x81). The first byte is the report number (0x1).
	Buffer[0] = 0x1;
	Buffer[1] = 0x81;
	auto Result = hid_write( mDevice, Buffer, sizeofarray(Buffer) );
	if ( Result < 0 )
	{
		std::Debug << "Failed to write to HidApi device: " << Result << std::endl;
		return mLastState;
	}
	
	Result = hid_read( mDevice, Buffer, sizeofarray(Buffer) );
	if ( Result == 0 )
	{
		std::Debug << "Waiting for HidApi device: " << Result << std::endl;
		return mLastState;
	}
	if ( Result < 0 )
	{
		std::Debug << "Failed to read HidApi device: " << Result << std::endl;
		return mLastState;
	}

	//	result is length;
	size_t ByteCount = Result;
	auto BufferData = GetRemoteArray( &Buffer[0], ByteCount );
	mLastState.mButtons.Copy(BufferData);
*/
	return mLastState;
}

