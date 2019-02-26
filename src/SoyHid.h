#pragma once

#include <functional>
#include <string>
#include "SoyLib/src/bufferarray.hpp"
#include "SoyLib/src/HeapArray.hpp"

#include <IOKit/hid/IOHIDManager.h>


namespace Soy
{
	class TInputDeviceMeta;
	class TInputDeviceState;
}

namespace Hid
{
	class TContext;
	class TDevice;
	class TDeviceMeta;
}


class Soy::TInputDeviceMeta
{
public:
	bool		operator==(const TInputDeviceMeta& That) const;
						   
public:
	std::string		mSerial;
	std::string		mName;
	std::string		mVendor;
	std::string		mUsbPath;
	bool			mConnected = true;
};

class Hid::TDeviceMeta : public Soy::TInputDeviceMeta
{
public:
	TDeviceMeta(){}
	TDeviceMeta(IOHIDDeviceRef Device) :
		mDevice	( Device )
	{
	}
	/*
	TDeviceMeta(const TDeviceMeta& That) :
		Soy::TInputDeviceMeta	( That ),
		mDevice					( That.mDevice )
	{
	}
	*/
	IOHIDDeviceRef	mDevice = nullptr;
};


class Soy::TInputDeviceState
{
public:
	BufferArray<uint8_t,32>	mButtons;
	BufferArray<float,32>	mAxises;
};



class Hid::TContext
{
public:
	TContext();
	~TContext();
	
	void				EnumDevices(std::function<void(Soy::TInputDeviceMeta& Meta)> OnDevice);
	
private:
	void				ListenForDevices();
	void				OnDeviceConnected(IOHIDDeviceRef Device,IOReturn Result);
	void				OnDeviceDisconnected(IOHIDDeviceRef Device,IOReturn Result);

	std::mutex			mDeviceMetasLock;
	Array<TDeviceMeta>	mDeviceMetas;	//	known devices
	IOHIDManagerRef		mManager = nullptr;
};



class Hid::TDevice
{
public:
	TDevice(TContext& Context,const std::string& DeviceName);
	~TDevice();
	
public:
	
	//	todo: get all states/updates since last call
	//		or have a callback.
	//	we all know a current-state approach is bad for missfirings
	Soy::TInputDeviceState	GetState() __deprecated;
	
private:
	void			OpenDevice(TContext& Context,const std::string& Reference);
	void			Bind(TDeviceMeta& Device);
	void			Unbind();
	
	void			OnButton(IOHIDElementType Type,uint32_t Page,uint32_t Usage,uint32_t Value);

	Hid::TDeviceMeta		mDevice;
	Soy::TInputDeviceState	mLastState;
};
