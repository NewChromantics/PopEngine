#pragma once
#include <string>
#include <functional>
#include "SoyLib/src/HeapArray.hpp"



namespace Bluetooth
{
	class TDeviceMeta;
	class TDevice;
	class TManager;
	class TContext;	//	platform specific
	
	namespace TState
	{
		enum Type
		{
			Invalid,		//	for unsupported things
			Connecting,
			Connected,
			Disconnecting,
			Disconnected,			
		};
	}
}


class Bluetooth::TDeviceMeta
{
public:
	inline bool		operator==(const TDeviceMeta& That) const	{	return this->mUuid == That.mUuid;	}

public:
	std::string		mUuid;
	std::string		mName;
	TState::Type	mState = TState::Disconnected;
};


class Bluetooth::TManager
{
public:
	TManager();

	//	gr: may need a seperate IsSupported(), currently using Invalid
	Bluetooth::TState::Type GetState();
	void					OnFoundDevice(TDeviceMeta DeviceMeta);
	void					OnStateChanged();

	//void	EnumConnectedDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta)> OnDeviceFound);
	//void	EnumDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta)> OnDeviceFound);

public:
	std::function<void(Bluetooth::TState::Type)>	mOnStateChanged;
	std::function<void()>		mOnDevicesChanged;
	Array<TDeviceMeta>			mKnownDevices;
	
private:
	std::shared_ptr<TContext>	mContext;
};

