#pragma once

#include <functional>

namespace Win32
{
	class TOpenglContext;
}

namespace Xr
{
	class TDevice;

	//void	EnumDevices(std::function<void(const std::string& DeviceName)> OnDeviceName);
}

namespace Openxr
{
	//	this will probably be renamed device once working, to seperate internal session & rendering/inputs
	class TSession;

	//	gr: Platform::TOpenglContext contains HLGRC DC etc for windows
	//		adapt as we go!
	std::shared_ptr<Xr::TDevice>	CreateDevice(Win32::TOpenglContext& Context);
}
