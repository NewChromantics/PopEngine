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
	std::string		mName;
	TState::TYPE	mState = TState::Disconnected;
};


class Bluetooth::TManager
{
public:
	TManager();
	
	void		EnumDevicesWithService(const std::string& ServiceUuid,std::function<void(TDeviceMeta&)> FoundDevice);
	
private:
	std::shared_ptr<TContext>	mContext;
};

