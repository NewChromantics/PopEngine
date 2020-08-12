#pragma once

#include <functional>

namespace Xr
{
	class TDevice;

	//void	EnumDevices(std::function<void(const std::string& DeviceName)> OnDeviceName);
}

namespace Openxr
{
	//	this will probably be renamed device once working, to seperate internal session & rendering/inputs
	class TSession;

	std::shared_ptr<Xr::TDevice>	CreateDevice();
}
