#pragma once

#include <functional>
#include <string>

#include "HidApi/hidapi/hidapi.h"


namespace Soy
{
	class TInputDeviceMeta;
}

namespace HidApi
{
	class TInstance;
	class TDevice;
}


class Soy::TInputDeviceMeta
{
public:
	std::string		mSerial;
	std::string		mName;
	std::string		mVendor;
	std::string		mUsbPath;
};



namespace HidApi
{
	class TContext;
	class TDevice;
}

class HidApi::TContext
{
public:
	TContext();
	~TContext();
	
	void		EnumDevices(std::function<void(Soy::TInputDeviceMeta& Meta)> OnDevice);
};

class HidApi::TDevice
{
public:
	TDevice(TContext& Context,const std::string& DeviceName);
	
public:
	std::function<void()>	mOnDisconnected;
	std::function<void()>	mOnConnected;
	
private:
	void			OpenDevice(TContext& Context,const std::string& Reference);
	
	hid_device*		mDevice = nullptr;
};
