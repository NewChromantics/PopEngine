#include "SoyInput.h"

#include "HidApi/hidapi/hidapi.h"

class THidApi
{
public:
	THidApi();
	~THidApi();
};
THidApi HidApi;

THidApi::THidApi()
{
	hid_init();
}

THidApi::~THidApi()
{
	hid_exit();
}

std::string GetStringFromWString(wchar_t* WString)
{
	std::wstring ws(WString);
	std::string str(ws.begin(), ws.end());
	return str;
}

void Soy::EnumInputDevices(std::function<void(const TInputDeviceMeta&)> Enum)
{
	auto VendorFilter = 0;
	auto ProductFilter = 0;
	auto* Devices = hid_enumerate( VendorFilter, ProductFilter );

	try
	{
		auto* Device = Devices;
		while ( Device )
		{
			TInputDeviceMeta Meta;
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
}


Soy::TInputDevice::TInputDevice(const std::string& DeviceName)
{
	
}
