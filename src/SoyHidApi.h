#pragma once

#include <functional>
#include <string>

#include "HidApi/hidapi/hidapi.h"
#include "SoyLib/src/bufferarray.hpp"

namespace Soy
{
	class TInputDeviceMeta;
	class TInputDeviceState;
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

class Soy::TInputDeviceState
{
public:
	BufferArray<uint8_t,32>	mButtons;
	BufferArray<float,32>	mAxises;
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
	~TDevice();
	
public:
	std::function<void()>	mOnDisconnected;
	std::function<void()>	mOnConnected;
	
	//	todo: get all states/updates since last call
	//		or have a callback.
	//	we all know a current-state approach is bad for missfirings
	Soy::TInputDeviceState	GetState() __deprecated;
	
private:
	void			OpenDevice(TContext& Context,const std::string& Reference);
	
	Soy::TInputDeviceState	mLastState;
	hid_device*		mDevice = nullptr;
};
