#pragma once

#include <functional>

namespace Xr
{
	class TDevice;

	void	EnumDevices(std::function<void(const std::string& DeviceName)> OnDeviceName);
}

