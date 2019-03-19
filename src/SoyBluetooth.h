#pragma once
#include <string>
#include <functional>

namespace Bluetooth
{
	class TDeviceMeta;
	class TDevice;
	class TManager;
	class TContext;	//	platform specific
	
	namespace TState
	{
		enum TYPE
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
	std::string		mUuid;
	std::string		mName;
	TState::TYPE	mState = TState::Disconnected;
};


class Bluetooth::TManager
{
public:
	TManager();

	//	gr: may need a seperate IsSupported(), or use Invalid
	Bluetooth::TState::TYPE GetState();
	
	//	gr: assume these are both blocking. Maybe have as jobs on a thread... or something cancellable
	void	EnumConnectedDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta)> OnDeviceFound);
	void	EnumDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta)> OnDeviceFound);

private:
	std::shared_ptr<TContext>	mContext;
};

