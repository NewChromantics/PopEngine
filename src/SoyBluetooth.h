#pragma once
#include <string>
#include <functional>
#include "SoyLib/src/HeapArray.hpp"



namespace Bluetooth
{
	class TDeviceMeta;
	class TDevice;
	class TManager;
	class TContext;			//	platform specific
	class TPlatformDevice;	//	platform specific
	class TPlatformDeviceDelegate;	//	platform specific

	namespace TState
	{
		enum Type
		{
			//	ranked from worst to best
			Unknown,
			Unsupported,
			Connecting,
			Disconnecting,
			Connected,
			Disconnected,
		};
	}
}


class Bluetooth::TDeviceMeta
{
public:
	inline bool		operator==(const TDeviceMeta& That) const	{	return this->mUuid == That.mUuid;	}
	inline bool		operator==(const std::string& That) const	{	return this->mUuid == That;	}

	const std::string&	GetName() const	{	return (mName.length() > 0) ? mName : mUuid;	}
	
public:
	std::string		mUuid;
	std::string		mName;
	BufferArray<std::string,100>	mServices;
	TState::Type	mState = TState::Disconnected;
};


class Bluetooth::TDevice
{
public:
	inline bool		operator==(const TDeviceMeta& That) const	{	return mMeta.mUuid == That.mUuid;	}
	inline bool		operator==(const std::string& That) const	{	return mMeta.mUuid == That;	}

	void			SubscribeToCharacteristics(const std::string& NewChracteristic=std::string());
	
public:
	TDeviceMeta			mMeta;
	TState::Type&		mState = mMeta.mState;
	//	characteristics we haven't yet subscribed to
	Array<std::string>	mPendingCharacteristics;
	
	std::shared_ptr<TPlatformDeviceDelegate>	mPlatformDeviceDelegate;
};

class Bluetooth::TManager
{
public:
	TManager();

	//	gr: may need a seperate IsSupported(), currently using Invalid
	Bluetooth::TState::Type GetState();
	void					OnStateChanged();

	void					Scan(const std::string& SpecificService);
	//void	EnumConnectedDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta)> OnDeviceFound);
	//void	EnumDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta)> OnDeviceFound);

	void					ConnectDevice(const std::string& Uuid);
	void					DisconnectDevice(const std::string& Uuid);
	TDevice&				GetDevice(const std::string& Uuid);
	void					SetDeviceState(TDevice& Device,TState::Type NewState);
	void					SetDeviceState(TPlatformDevice* Device,TState::Type NewState);
	void					UpdateDeviceMeta(TDevice& Device);

	void					DeviceRecv(const std::string& DeviceUuid,const std::string& Char);

private:
	void					OnDeviceChanged(TDevice& Device);

public:
	std::function<void(Bluetooth::TState::Type)>	mOnStateChanged;
	std::function<void()>			mOnDevicesChanged;
	std::function<void(TDevice&)>	mOnDeviceChanged;
	std::function<void(TDevice&)>	mOnDeviceRecv;
	
	Array<std::shared_ptr<TDevice>>	mDevices;
	
	//	service we're currently scanning, or when we wake up, will scan
	std::string						mScanService;
	
private:
	std::shared_ptr<TContext>	mContext;
};

