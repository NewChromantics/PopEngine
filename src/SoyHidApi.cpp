#include "SoyHidApi.h"

#include "SoyLib/src/SoyDebug.h"



HidApi::TContext::TContext()
{
	hid_init();
}

HidApi::TContext::~TContext()
{
	hid_exit();
}

std::string GetStringFromWString(wchar_t* WString)
{
	std::wstring ws(WString);
	std::string str(ws.begin(), ws.end());
	return str;
}

void HidApi::TContext::EnumDevices(std::function<void(Soy::TInputDeviceMeta&)> Enum)
{
	auto VendorFilter = 0;
	auto ProductFilter = 0;
	auto* Devices = hid_enumerate( VendorFilter, ProductFilter );

	try
	{
		auto* Device = Devices;
		while ( Device )
		{
			Soy::TInputDeviceMeta Meta;
			Meta.mVendor = GetStringFromWString( Device->manufacturer_string );
			Meta.mName = GetStringFromWString( Device->product_string );
			Meta.mSerial = GetStringFromWString( Device->serial_number );
			Meta.mUsbPath = std::string( Device->path );
			Enum( Meta );
			Device = Device->next;
		}
	}
	catch(std::exception& e)
	{
		hid_free_enumeration( Devices );
		throw;
	}
	hid_free_enumeration( Devices );
}

HidApi::TDevice::TDevice(TContext& Context,const std::string& DeviceName)
{
	OpenDevice( Context, DeviceName );
}

void HidApi::TDevice::OpenDevice(TContext& Context,const std::string& Reference)
{
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
}
