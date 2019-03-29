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
		kHIDUsage_GD_Keyboard,
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
		//	gr: I'm wondering if this is causing HIDD errors if it takes too long...
		Soy::TScopeTimerPrint Timer("Hid Device connected handler",1);
		auto* This = reinterpret_cast<TContext*>( Context );
		This->OnDeviceConnected( Device, Result );
	};
	
	auto OnDisconnected = [](void* Context,IOReturn Result,void* Sender,IOHIDDeviceRef Device)
	{
		Soy::TScopeTimerPrint Timer("Hid Device disconnected handler",1);
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

Hid::TDeviceMeta GetMeta(IOHIDDeviceRef Device)
{
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

	
	Hid::TDeviceMeta Meta(Device);

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
	
	Meta.mName = Product;
	if ( !Meta.mName.length() )
		Meta.mName = Meta.mUsbPath;//Meta.mSerial;

	return Meta;
}

void Hid::TContext::OnDeviceConnected(IOHIDDeviceRef Device,IOReturn Result)
{
	//	find existing or add
	auto Meta = GetMeta( Device );

	{
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
	OnDevicesChanged();
}

void Hid::TContext::OnDeviceDisconnected(IOHIDDeviceRef Device,IOReturn Result)
{
	//	find existing and mark disconnected
	auto Meta = GetMeta( Device );
	{
		std::lock_guard<std::mutex> Lock( mDeviceMetasLock );
		auto* ExistingMeta = mDeviceMetas.Find( Meta );
		if ( !ExistingMeta )
		{
			std::Debug << "Didn't know of Hid Device " << Meta.mName << std::endl;
			return;
		}
		std::Debug << "Hid device disconnected" << Meta.mName << std::endl;
		ExistingMeta->ReleaseDevice();
		ExistingMeta->mConnected = false;
	}
	OnDevicesChanged();
}

void Hid::TContext::OnDevicesChanged()
{
	//	don't let external stuff bring us down
	try
	{
		if ( mOnDevicesChanged )
			mOnDevicesChanged();
	}
	catch(std::exception& e)
	{
		std::Debug << "OnDevicesChanged callback exception: " << e.what() << std::endl;
	}
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
	//	get the meta, which should contain the device ref
	TDeviceMeta MatchedMeta(nullptr);
	bool FoundMatch = false;
	
	//	enumerate devices and try and match name/serial
	auto OnDevice = [&](Soy::TInputDeviceMeta& SoyMeta)
	{
		auto& Meta = static_cast<Hid::TDeviceMeta&>(SoyMeta);
		
		bool Match = false;
		Match |= Meta.mSerial == Reference;
		Match |= Meta.mName == Reference;
		Match |= Meta.mUsbPath == Reference;
		if ( !Match )
			return;
		
		if ( FoundMatch )
		{
			std::stringstream Error;
			Error << "Already found matching device, ambiguious name/serial/usb path " << Reference;
			throw Soy::AssertException( Error.str() );
		}
		FoundMatch = true;
		MatchedMeta = Meta;
	};
	Context.EnumDevices( OnDevice );
	
	if ( !FoundMatch )
	{
		std::stringstream Error;
		Error << "Didn't find a device matching " << Reference;
		throw Soy::AssertException(Error.str());
	}
	
	//	open the device
	Bind(MatchedMeta);
}

const char* GetElementType(IOHIDElementType Type)
{
	switch ( Type )
	{
		case kIOHIDElementTypeInput_Misc:		return "Misc";
		case kIOHIDElementTypeInput_Button:		return "Button";
		case kIOHIDElementTypeInput_Axis:		return "Axis";
		case kIOHIDElementTypeInput_ScanCodes:	return "ScanCodes";
		case kIOHIDElementTypeOutput:			return "Output";
		case kIOHIDElementTypeFeature:			return "Feature";
		case kIOHIDElementTypeCollection:		return "Collection";
		default: return "Unknown IOHIDElementType";
	};
}

void Hid::TDevice::Bind(TDeviceMeta& Device)
{
	//	gr: maybe need to store retained for race conditions and hope API
	//		errors if it's been disconnected
	auto DevicePtr = Device.GetDevice();
	if ( !DevicePtr )
		throw Soy::AssertException("Trying to bind to device that's been released");
	
	auto Result = IOHIDDeviceOpen( DevicePtr, kIOHIDOptionsTypeNone);
	IsOkay(Result, "IOHIDDeviceOpen");
	mDevice = Device;
	
	InitButtons();
	
	auto OnInput = [](void* context,IOReturn result,void* sender,IOHIDValueRef State)
	{
		//	gr: wondering if this is causing problems
		Soy::TScopeTimerPrint Timer("Hid Device input handler",1);
		auto* This = reinterpret_cast<TDevice*>(context);
		auto Value = IOHIDValueGetIntegerValue(State);
		auto Element = IOHIDValueGetElement(State);
		This->UpdateButton( Element, Value );
	};
	
	IOHIDDeviceRegisterInputValueCallback( DevicePtr, OnInput, this );
	IOHIDDeviceScheduleWithRunLoop( DevicePtr, CFRunLoopGetCurrent(), kCFRunLoopCommonModes );
}

void Hid::TDevice::Unbind()
{
	auto DevicePtr = mDevice.GetDevice();
	if ( DevicePtr )
	{
		auto Result = IOHIDDeviceClose( DevicePtr, kIOHIDOptionsTypeNone);
		IsOkay(Result, "IOHIDDeviceOpen");
	}
}

std::string GetPageName(uint32_t Page)
{
	switch ( Page )
	{
		case kHIDPage_Undefined:			return "kHIDPage_Undefined";
		case kHIDPage_GenericDesktop:		return "kHIDPage_GenericDesktop";
		case kHIDPage_Simulation:			return "kHIDPage_Simulation";
		case kHIDPage_VR:					return "kHIDPage_VR";
		case kHIDPage_Sport:				return "kHIDPage_Sport";
		case kHIDPage_Game:						return "kHIDPage_Game";
		case kHIDPage_GenericDeviceControls:	return "kHIDPage_GenericDeviceControls";
		case kHIDPage_KeyboardOrKeypad:			return "kHIDPage_KeyboardOrKeypad";
		case kHIDPage_LEDs:						return "kHIDPage_LEDs";
		case kHIDPage_Button:					return "kHIDPage_Button";
		case kHIDPage_Ordinal:					return "kHIDPage_Ordinal";
		case kHIDPage_Telephony:				return "kHIDPage_Telephony";
		case kHIDPage_Consumer:				return "kHIDPage_Consumer";
		case kHIDPage_Digitizer:			return "kHIDPage_Digitizer";
		case kHIDPage_PID:					return "kHIDPage_PID";
		case kHIDPage_Unicode:				return "kHIDPage_Unicode";
		case kHIDPage_AlphanumericDisplay:	return "kHIDPage_AlphanumericDisplay";
		case kHIDPage_Sensor:				return "kHIDPage_Sensor";
		case kHIDPage_Monitor:				return "kHIDPage_Monitor";
		case kHIDPage_MonitorEnumerated:	return "kHIDPage_MonitorEnumerated";
		case kHIDPage_MonitorVirtual:		return "kHIDPage_MonitorVirtual";
		case kHIDPage_MonitorReserved:		return "kHIDPage_MonitorReserved";
		case kHIDPage_PowerDevice:			return "kHIDPage_PowerDevice";
		case kHIDPage_BatterySystem:		return "kHIDPage_BatterySystem";
		case kHIDPage_PowerReserved:				return "kHIDPage_PowerReserved";
		case kHIDPage_PowerReserved2:				return "kHIDPage_PowerReserved2";
		case kHIDPage_BarCodeScanner:				return "kHIDPage_BarCodeScanner";
		case kHIDPage_WeighingDevice:		return "kHIDPage_WeighingDevice";
		//case kHIDPage_Scale:				return "kHIDPage_Scale";
		case kHIDPage_MagneticStripeReader:	return "kHIDPage_MagneticStripeReader";
		case kHIDPage_CameraControl:		return "kHIDPage_CameraControl";
		case kHIDPage_Arcade:				return "kHIDPage_Arcade";
		
		//	handle other cases specially
		default:
		break;
	}
	
	std::stringstream Description;
	if ( Page >= kHIDPage_VendorDefinedStart )
	{
		Description << "VendorDefined";
	}
	else
	{
		Description << "Reserved";
	}
	Description << "0x" << std::hex << Page;
	return Description.str();
}

std::string GetTypeName(IOHIDElementType Type)
{
	switch(Type)
	{
		case kIOHIDElementTypeInput_Misc:		return "Misc";
		case kIOHIDElementTypeInput_Button:		return "Button";
		case kIOHIDElementTypeInput_Axis:		return "Axis";
		case kIOHIDElementTypeInput_ScanCodes:	return "ScanCodes";
		case kIOHIDElementTypeOutput:			return "Output";
		case kIOHIDElementTypeFeature:			return "Feature";
		case kIOHIDElementTypeCollection:		return "Collection";
		default:
		return "Unhandled";
	}
}

std::string GetDesktopUsageName(uint32_t Usage)
{
	switch(Usage)
	{
		case kHIDUsage_GD_Pointer:	return "Pointer";
		case kHIDUsage_GD_Mouse:	return "Mouse";
		case kHIDUsage_GD_Joystick:	return "Joystick";
		case kHIDUsage_GD_GamePad:	return "GamePad";
		case kHIDUsage_GD_Keyboard:	return "Keyboard";
		case kHIDUsage_GD_Keypad:	return "Keypad";
		case kHIDUsage_GD_MultiAxisController:	return "MultiAxisController";
		case kHIDUsage_GD_TabletPCSystemControls:	return "TabletPCSystemControls";
		case kHIDUsage_GD_AssistiveControl:	return "AssistiveControl";
		case kHIDUsage_GD_X:	return "X";
		case kHIDUsage_GD_Y:	return "Y";
		case kHIDUsage_GD_Z:	return "Z";
		case kHIDUsage_GD_Rx:	return "Rx";
		case kHIDUsage_GD_Ry:	return "Ry";
		case kHIDUsage_GD_Rz:	return "Rz";
		case kHIDUsage_GD_Slider:	return "Slider";
		case kHIDUsage_GD_Dial:	return "Dial";
		case kHIDUsage_GD_Wheel:	return "Wheel";
		case kHIDUsage_GD_Hatswitch:	return "Hatswitch";
		case kHIDUsage_GD_CountedBuffer:	return "CountedBuffer";
		case kHIDUsage_GD_ByteCount:	return "ByteCount";
		case kHIDUsage_GD_MotionWakeup:	return "MotionWakeup";
		case kHIDUsage_GD_Start:	return "Start";
		case kHIDUsage_GD_Select:	return "Select";
		case kHIDUsage_GD_Vx:	return "Vx";
		case kHIDUsage_GD_Vy:	return "Vy";
		case kHIDUsage_GD_Vz:	return "Vz";
		case kHIDUsage_GD_Vbrx:	return "Vbrx";
		case kHIDUsage_GD_Vbry:	return "Vbry";
		case kHIDUsage_GD_Vbrz:	return "Vbrz";
		case kHIDUsage_GD_Vno:	return "Vno";
		case kHIDUsage_GD_SystemControl:	return "SystemControl";
		case kHIDUsage_GD_SystemPowerDown:	return "SystemPowerDown";
		case kHIDUsage_GD_SystemSleep:	return "SystemSleep";
		case kHIDUsage_GD_SystemWakeUp:	return "SystemWakeUp";
		case kHIDUsage_GD_SystemContextMenu:	return "SystemContextMenu";
		case kHIDUsage_GD_SystemMainMenu:	return "SystemMainMenu";
		case kHIDUsage_GD_SystemAppMenu:	return "SystemAppMenu";
		case kHIDUsage_GD_SystemMenuHelp:	return "SystemMenuHelp";
		case kHIDUsage_GD_SystemMenuExit:	return "SystemMenuExit";
		case kHIDUsage_GD_SystemMenuSelect:	return "SystemMenuSelect";
		//case kHIDUsage_GD_SystemMenu:	return "SystemMenu";
		case kHIDUsage_GD_SystemMenuRight:	return "SystemMenuRight";
		case kHIDUsage_GD_SystemMenuLeft:	return "SystemMenuLeft";
		case kHIDUsage_GD_SystemMenuUp:	return "SystemMenuUp";
		case kHIDUsage_GD_SystemMenuDown:	return "SystemMenuDown";
		case kHIDUsage_GD_DPadUp:	return "DPadUp";
		case kHIDUsage_GD_DPadDown:	return "DPadDown";
		case kHIDUsage_GD_DPadRight:	return "DPadRight";
		case kHIDUsage_GD_DPadLeft:	return "DPadLeft";
		case kHIDUsage_GD_Reserved:	return "Reserved";
		
		default:
		{
			std::stringstream UnknownName;
			UnknownName << "<Usage " << Usage << "/0x" << std::hex << Usage << ">";
			return UnknownName.str();
		}
	}
}


std::string GetKeyboardUsageName(uint32_t Usage)
{
	//	these should be standard codes from HID->keyboard code
	//	todo: find proper table
	switch ( Usage )
	{
		case 30:	return "1";
		case 31:	return "2";
		case 32:	return "3";
		case 33:	return "4";
		case 34:	return "5";
		case 35:	return "6";
		case 36:	return "7";
		case 37:	return "8";
		case 38:	return "9";
		case 39:	return "0";

		case 4:		return "a";
		case 5:		return "b";
		case 6:		return "c";
		case 7:		return "d";
		case 8:		return "e";
		case 9:		return "f";
		case 10:	return "g";
		case 11:	return "h";
		case 12:	return "i";
		case 13:	return "j";
		case 14:	return "k";
		case 15:	return "l";
		case 16:	return "m";
		case 17:	return "n";
		case 18:	return "o";
		case 19:	return "p";
		case 20:	return "q";
		case 21:	return "r";
		case 22:	return "s";
		case 23:	return "t";
		case 24:	return "u";
		case 25:	return "v";
		case 26:	return "w";
		case 27:	return "x";
		case 28:	return "y";
		case 29:	return "z";

		case 41:	return "esc";
		case 44:	return "space";
		case 45:	return "-";
		case 46:	return "+";
		case 53:	return "grave";
		case 79:	return "right";
		case 80:	return "left";
		case 81:	return "down";
		case 82:	return "up";

		default:
		break;
	}
	
	std::stringstream Name;
	Name << "0x" << std::hex << Usage;
	return Name.str();
}


void GetElementChildren(CFArrayRef Elements,std::function<void(IOHIDElementRef)> EnumElement)
{
	if ( Elements == nullptr )
		return;
	
	auto Count = CFArrayGetCount(Elements);
	for ( auto i=0;	i<Count;	i++ )
	{
		auto ArrayElement = const_cast<void*>( CFArrayGetValueAtIndex( Elements, i ) );
		auto Element = static_cast<IOHIDElementRef>(ArrayElement);
		if ( CFGetTypeID(Element) != IOHIDElementGetTypeID() )
		{
			std::Debug << "Hid::TDevice::InitButtons got button #" << i << " that is not IOHIDElementType" << std::endl;
			continue;
		}
		
		EnumElement( Element );
	}
}


void GetMeta(IOHIDElementRef Button,size_t UnknownAxisIndex,std::function<void(const Soy::TInputDeviceButtonMeta&)> EnumMeta)
{
	IOHIDElementType Type = IOHIDElementGetType(Button);
	
	auto Usage = IOHIDElementGetUsage(Button);
	auto Page = IOHIDElementGetUsagePage(Button);
	auto HidName = Platform::GetString( IOHIDElementGetName(Button) );
	auto Virtual = IOHIDElementIsVirtual( Button )!=0;
	auto ReportId = IOHIDElementGetReportID(Button);
	// As a physical element can appear in the device twice (in different collections) and can be
	// represented by different IOHIDElementRef objects, we look at the IOHIDElementCookie which
	// is meant to be unique for each physical element.
	IOHIDElementCookie Cookie = IOHIDElementGetCookie(Button);
	auto PageName = GetPageName( Page );
	auto TypeName = GetTypeName( Type );
	
	//	recurse into collection
	if ( Type == kIOHIDElementTypeCollection )
	{
		std::stringstream Error;
		Error << "todo: handle collection: " << HidName << " Page=" << PageName;
		throw Soy::AssertException(Error.str());
	}

	auto ThrowError = [&](const std::string& ErrorDescription)
	{
		std::stringstream Error;
		Error << ErrorDescription << " " << TypeName;
		Error << " Page= " << PageName;
		Error << " Usage=" << Usage;
		Error << " Cookie=" << Cookie;
		Error << " HidName=" << HidName;
		throw Soy::AssertException(Error.str() );
	};
	
	Soy::TInputDeviceButtonMeta Meta;
	Meta.mCookie = Cookie;
	Meta.mName = HidName;
	
	//	gr: new approach, go via page categorisation first
	if ( Page == kHIDPage_GenericDesktop )
	{
		if ( Meta.mName.length() == 0 )
			Meta.mName = GetDesktopUsageName( Usage );
	
		EnumMeta( Meta );
		return;
	}
	
	if ( Page == kHIDPage_KeyboardOrKeypad )
	{
		if ( Type == kIOHIDElementTypeInput_Button )
		{
			//	some cases are not valid for indexes
			if ( Usage == kHIDUsage_Undefined )
				ThrowError("Not expecting Undefined(0) usage for keyboard button");
			if ( Usage == -1 )
				ThrowError("Not expecting -1 usage for keyboard button");

			Meta.mType = Soy::TInputDeviceButtonType::Button;
			if ( Meta.mName.length() == 0 )
				Meta.mName = GetKeyboardUsageName( Usage );
			Meta.mIndex = Usage;
			EnumMeta( Meta );
			return;
		}
	}

	ThrowError("Unhandled HID");
	
	
	/*
	
	//	0 is "undefined", -1 appears for keyboards, but it shouldn't...
	if ( Usage == -1 )
	{
		std::stringstream Error;
		Error << "Not expecting HIDElementUsage to be -1. ";
		Error << " Page=" << Page;
		Error << " Name=" << Name;
		Error << " Cookie=" << Cookie;
		throw Soy::AssertException( Error.str() );
	}

	if ( Name.length() == 0 )
	{
		std::stringstream NewName;
		if ( Type == kIOHIDElementTypeInput_Button )
		{
			NewName << "Button" << Usage;
			std::Debug << "Setting button name to Button+Usage (" << NewName.str() << "). Usagename = " << GetDesktopUsageName(Usage) << " page=" << PageName << std::endl;
		}
		else if ( Page == kHIDPage_GenericDesktop )
		{
			NewName << GetDesktopUsageName(Usage);
		}
		else
		{
			NewName << GetElementType(Type) << Cookie;
		}
		
		//	always indicate fake name with a *
		NewName << '*';
		Name = NewName.str();
	}
	
	
	Meta.mName = Name;
	Meta.mCookie = Cookie;

	//	enum children
	//if ( Type == kIOHIDElementTypeCollection )
	Array<Soy::TInputDeviceButtonMeta> ChildMetas;
	{
		auto ChildElements = IOHIDElementGetChildren( Button );
		auto AddChild = [&](const Soy::TInputDeviceButtonMeta& ChildMeta)
		{
			ChildMetas.PushBack( ChildMeta );
		};
		auto OnChildElement = [&](IOHIDElementRef Child)
		{
			GetMeta( Child, UnknownAxisIndex, AddChild );
			//auto ChildMeta = GetMeta( Child, UnknownAxisIndex, OnChild );
			//ChildMeta.mName =
		};
		GetElementChildren( ChildElements, OnChildElement );
		if ( ChildMetas.GetSize() > 0 )
			std::Debug << Meta.mName << " has " << ChildMetas.GetSize() << " children" <<std::endl;
	}
	

	
	
	if ( Type == kIOHIDElementTypeInput_Button )
	{
		Meta.mType = Soy::TInputDeviceButtonType::Button;
		if ( Usage == kHIDUsage_Undefined )
			throw Soy::AssertException("Not expecting button usage to be zero");
		Meta.mIndex = Usage-1;
	}
	else if ( Page == kHIDPage_GenericDesktop && Usage == kHIDUsage_GD_Hatswitch )
	{
		Meta.mType = Soy::TInputDeviceButtonType::Axis;
		Meta.mIndex = UnknownAxisIndex;
	}
	else if ( Page == kHIDPage_GenericDesktop && Usage == kHIDUsage_GD_X )
	{
		Meta.mType = Soy::TInputDeviceButtonType::Button;
		Meta.mIndex = UnknownAxisIndex;
	}
	else if ( Page == kHIDPage_GenericDesktop && Usage == kHIDUsage_GD_Y )
	{
		Meta.mType = Soy::TInputDeviceButtonType::Button;
		Meta.mIndex = UnknownAxisIndex;
	}
	else if ( Page == kHIDPage_GenericDesktop && Usage == kHIDUsage_GD_Rx )
	{
		Meta.mType = Soy::TInputDeviceButtonType::Button;
		Meta.mIndex = UnknownAxisIndex;
	}
	else if ( Page == kHIDPage_GenericDesktop && Usage == kHIDUsage_GD_Ry )
	{
		Meta.mType = Soy::TInputDeviceButtonType::Button;
		Meta.mIndex = UnknownAxisIndex;
	}
	else if ( Page >= kHIDPage_VendorDefinedStart )
	{
		//	ignore vendor specific
		Meta.mType = Soy::TInputDeviceButtonType::Invalid;
	}
	else if ( Type == kIOHIDElementTypeInput_Axis )
	{
		Meta.mType = Soy::TInputDeviceButtonType::Axis;
		throw Soy::AssertException("Todo: handle axis!");
	}
	else if ( Type == kIOHIDElementTypeInput_Misc )
	{
		Meta.mType = Soy::TInputDeviceButtonType::Other;
	}
	else if ( Type == kIOHIDElementTypeCollection )
	{
		//	if this is a collection of two children, hopefully it's an axis
		//	todo: support 3 with Z
		if ( ChildMetas.GetSize() == 2 )
		{
			Meta.mType = Soy::TInputDeviceButtonType::Axis;
			for ( auto c=0;	c<ChildMetas.GetSize();	c++ )
			{
				auto& ChildMeta = ChildMetas[c];
				Meta.mAxisCookies.PushBack( ChildMeta.mCookie );
			}
		}
		else
		{
			Meta.mType = Soy::TInputDeviceButtonType::Other;
		}
	}
	else
	{
		std::stringstream Error;
		Error << "Uncategorised HID input; ";
		Error << "Page=" << Page << ", ";
		Error << "Usage=" << Usage << ", ";
		Error << "Name=" << Name << ", ";
		Error << "Virtual=" << Virtual << ", ";
		Error << "Cookie=" << Cookie << ", ";

		throw Soy::AssertException( Error.str() );
	}
	
	//std::Debug << "Found " << Meta.mName << "(" << GetElementType(Type) << ") ReportId=" << ReportId << " virtual=" << Virtual << " cookie=" << Cookie << " page=" << Page << " usage=" << Usage << std::endl;
	EnumMeta( Meta );
	 */
}


void Hid::TDevice::AddButton(IOHIDElementRef Button)
{
	size_t UnknownAxisIndex = mLastState.mAxis.GetSize();
	auto Append = [&](const Soy::TInputDeviceButtonMeta& Meta)
	{
		AddButton( Meta );
	};
	GetMeta( Button, UnknownAxisIndex, Append );
}


void Hid::TDevice::AddButton(const Soy::TInputDeviceButtonMeta& Meta)
{
	auto SetButton = [&](size_t Index,bool ButtonState)
	{
		while ( Index >= mLastState.mButton.GetSize() )
			mLastState.mButton.PushBack( false );
		mLastState.mButton[Index] = ButtonState;
	};
	
	auto SetAxis = [&](size_t Index,vec2f AxisState)
	{
		while ( Index >= mLastState.mAxis.GetSize() )
			mLastState.mAxis.PushBack( vec2f() );
		mLastState.mAxis[Index] = AxisState;
	};
	
	//	setup state
	if ( Meta.mType == Soy::TInputDeviceButtonType::Button )
	{
		SetButton( Meta.mIndex, false );
	}
	else if ( Meta.mType == Soy::TInputDeviceButtonType::Axis )
	{
		SetAxis( Meta.mIndex, vec2f() );
	}
	else
	{
		//	ignoring others
		std::Debug << "Ignoring " << Meta.mName << std::endl;
		return;
	}
	
	//std::Debug << this->mDevice.mName << " adding button: " << Meta.mName << " #" << Meta.mIndex << " cookie=" << Meta.mCookie << std::endl;
	mStateMetas.PushBack( Meta );
}

void Hid::TDevice::OnStateChanged()
{
	if ( mOnStateChanged )
		mOnStateChanged();
}


void Hid::TDevice::UpdateButton(IOHIDElementRef Button,int64_t Value)
{
	auto Cookie = IOHIDElementGetCookie(Button);
	//auto Meta = GetMeta( Button );
	auto* pMeta = mStateMetas.Find( Cookie );
	if ( !pMeta )
	{
		size_t UnknownAxisIndex = 9999;
		//auto Meta = GetMeta( Button, UnknownAxisIndex );
		//std::Debug << "Igoring button " << Meta.mName << " as not in state meta list" << std::endl;
		//std::Debug << "Igoring button cookie=" << Cookie << " as not in state meta list" << std::endl;
		return;
	}
	
	auto& Meta = *pMeta;
	auto& OutputState = mLastState;
	
	auto SetButton = [&](size_t ButtonIndex,bool ButtonState)
	{
		while ( ButtonIndex >= OutputState.mButton.GetSize() )
			OutputState.mButton.PushBack( false );
		OutputState.mButton[ButtonIndex] = ButtonState;
		
	};
	auto SetAxis = [&](size_t AxisIndex,vec2f AxisState)
	{
		while ( AxisIndex >= OutputState.mAxis.GetSize() )
			OutputState.mAxis.PushBack( vec2f() );
		OutputState.mAxis[AxisIndex] = AxisState;
	};
	
	//	special case
	auto Usage = IOHIDElementGetUsage(Button);
	auto Page = IOHIDElementGetUsagePage(Button);
	if ( Page == kHIDPage_GenericDesktop && Usage == kHIDUsage_GD_Hatswitch )
	{
		vec2f _ValueAxises[] =
		{
			vec2f(0,-1),
			vec2f(1,-1),
			vec2f(1,0),
			vec2f(1,1),
			vec2f(0,1),
			vec2f(-1,1),
			vec2f(-1,0),
			vec2f(-1,-1),
			vec2f(0,0),	//	none!
		};
		auto ValueAxises = GetRemoteArray(_ValueAxises);
		auto Axis = ValueAxises[Value];
		SetAxis( Meta.mIndex, Axis );
	}
	else if ( Meta.mType == Soy::TInputDeviceButtonType::Button )
	{
		SetButton( Meta.mIndex, Value );
	}
	else if ( Meta.mType == Soy::TInputDeviceButtonType::Axis )
	{
		throw Soy::AssertException("Todo: handle axis input");
	}
	else
	{
		throw Soy::AssertException("unhandled input");
	}

	//std::Debug << "Input cookie=" << Cookie << ": " << GetElementType(Type) << " page=" << Page << " usage=" << Usage << " value=" << Value << std::endl;
	OnStateChanged();
}

void Hid::TDevice::InitButtons()
{
	//	gr: watch out for release during call?
	auto DevicePtr = mDevice.GetDevice();
	if ( !DevicePtr )
		throw Soy::AssertException("Hid::TDevice::InitButtons aborted as device released");
	
	CFArrayRef ElementsPtr = IOHIDDeviceCopyMatchingElements( DevicePtr, nullptr, kIOHIDOptionsTypeNone );
	if ( !ElementsPtr )
	{
		std::Debug << "Hid::TDevice::InitButtons returned null input elements" << std::endl;
		return;
	}
	CFPtr<CFArrayRef> Elements( ElementsPtr, false );
	
	auto OnElement = [&](IOHIDElementRef Element)
	{
		try
		{
			AddButton( Element );
		}
		catch(std::exception& e)
		{
			//std::Debug << this->mDevice.mName << " error adding button: " << e.what() << std::endl;
		}
	};

	GetElementChildren( Elements.mObject, OnElement );
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

