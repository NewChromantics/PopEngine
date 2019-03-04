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
	class TInputDeviceButtonMeta;
	
	namespace TInputDeviceButtonType
	{
		enum TYPE
		{
			Button,
			Axis,	//	+dpad/Hatswitch
			Other,
			Invalid,
		};
	}
}

namespace Hid
{
	class TContext;
	class TDevice;
	class TDeviceMeta;
}

class Soy::TInputDeviceButtonMeta
{
public:
	bool		operator==(const uint32_t& Cookie) const	{	return mCookie == Cookie;	}

public:
	std::string						mName;
	size_t							mIndex = 0;		//	axis index, or button index
	uint32_t						mCookie = 0;	//	unique identifier... I like the term cookie
	BufferArray<uint32_t,3>			mAxisCookies;	//	if this is an axis, these buttons are the axis'
	TInputDeviceButtonType::TYPE	mType = TInputDeviceButtonType::Invalid;
};

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
	BufferArray<bool,32>	mButton;
	BufferArray<vec2f,32>	mAxis;
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
	void			InitButtons();

	void			Bind(TDeviceMeta& Device);
	void			Unbind();
	
	void			AddButton(IOHIDElementRef Button);
	void			AddButton(const Soy::TInputDeviceButtonMeta& Meta);
	void			UpdateButton(IOHIDElementRef Button,int64_t Value);

	Hid::TDeviceMeta		mDevice;
	Soy::TInputDeviceState	mLastState;
	Array<Soy::TInputDeviceButtonMeta>	mStateMetas;
};
