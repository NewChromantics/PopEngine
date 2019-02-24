#pragma once

#include <functional>
#include <string>

namespace Soy
{
	class TInputDevice;
	class TInputDeviceMeta;
	
	void	EnumInputDevices(std::function<void(const TInputDeviceMeta&)> Enum);
}


class Soy::TInputDeviceMeta
{
public:
	std::string		mSerial;
	std::string		mName;
	std::string		mVendor;
	std::string		mUsbPath;
};


class Soy::TInputDevice
{
public:
	TInputDevice(const std::string& DeviceName);
};
