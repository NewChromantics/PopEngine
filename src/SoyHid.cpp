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
	auto Result = IOHIDDeviceOpen( Device.mDevice, kIOHIDOptionsTypeNone);
	IsOkay(Result, "IOHIDDeviceOpen");
	mDevice = Device;
	
	InitButtons();
	
	auto OnInput = [](void* context,IOReturn result,void* sender,IOHIDValueRef State)
	{
		auto* This = reinterpret_cast<TDevice*>(context);
		auto Value = IOHIDValueGetIntegerValue(State);
		auto Element = IOHIDValueGetElement(State);
		This->UpdateButton( Element, Value );
	};
	
	IOHIDDeviceRegisterInputValueCallback( Device.mDevice, OnInput, this );
	IOHIDDeviceScheduleWithRunLoop( Device.mDevice, CFRunLoopGetCurrent(), kCFRunLoopCommonModes );
}

void Hid::TDevice::Unbind()
{
	auto Result = IOHIDDeviceClose( mDevice.mDevice, kIOHIDOptionsTypeNone);
	IsOkay(Result, "IOHIDDeviceOpen");
}

const char* GetDesktopUsageName(uint32_t Usage)
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
		default:	return "Reserved";
	}
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
	Soy::TInputDeviceButtonMeta Meta;
	IOHIDElementType Type = IOHIDElementGetType(Button);
	
	auto Usage = IOHIDElementGetUsage(Button);
	auto Page = IOHIDElementGetUsagePage(Button);
	auto Name = Platform::GetString( IOHIDElementGetName(Button) );
	auto Virtual = IOHIDElementIsVirtual( Button )!=0;
	auto ReportId = IOHIDElementGetReportID(Button);
	// As a physical element can appear in the device twice (in different collections) and can be
	// represented by different IOHIDElementRef objects, we look at the IOHIDElementCookie which
	// is meant to be unique for each physical element.
	IOHIDElementCookie Cookie = IOHIDElementGetCookie(Button);
	
	if ( Name.length() == 0 )
	{
		std::stringstream NewName;
		if ( Type == kIOHIDElementTypeInput_Button )
		{
			NewName << "Button" << Usage;
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
		if ( Usage == 0 )
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
		throw Soy::AssertException("Todo: categorise this!");
	}
	
	//std::Debug << "Found " << Meta.mName << "(" << GetElementType(Type) << ") ReportId=" << ReportId << " virtual=" << Virtual << " cookie=" << Cookie << " page=" << Page << " usage=" << Usage << std::endl;
	EnumMeta( Meta );
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
	
	std::Debug << "Adding button: " << Meta.mName << " #" << Meta.mIndex << std::endl;
	mStateMetas.PushBack( Meta );
	//std::Debug << "Found " << Meta.mName << "(" << GetElementType(Type) << ") ReportId=" << ReportId << " virtual=" << Virtual << " cookie=" << Cookie << " page=" << Page << " usage=" << Usage << " initialvalue=" << InitialValue << std::endl;
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
		std::Debug << "Igoring button cookie=" << Cookie << " as not in state meta list" << std::endl;
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
}

void Hid::TDevice::InitButtons()
{
	CFArrayRef Elements = IOHIDDeviceCopyMatchingElements( mDevice.mDevice, nullptr, kIOHIDOptionsTypeNone );
	auto OnElement = [&](IOHIDElementRef Element)
	{
		AddButton( Element );
	};
	try
	{
		GetElementChildren( Elements, OnElement );
		CFRelease(Elements);
	}
	catch (...)
	{
		CFRelease(Elements);
		throw;
	}

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

