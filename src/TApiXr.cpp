#include "TApiXr.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"
#include "SoyOpengl.h"
#include "SoyOpenglWindow.h"

#include <magic_enum.hpp>
#include "SoyRuntimeLibrary.h"
#include <string_view>
using namespace std::literals;

#if defined(TARGET_IOS)
#include "SoyArkitXr.h"
#endif


#if defined(TARGET_OSX) || defined(TARGET_WINDOWS)
#define ENABLE_OPENXR
#endif

#if defined(ENABLE_OPENXR)
#include "SoyOpenxr.h"
#endif

namespace ApiXr
{
	const char Namespace[] = "Pop.Xr";

	//	gr: like EnumDevices in ApiMedia, maybe should be EnumDevices, but
	//		implementation on web requires some callback, so Create is more appropriate?...
	DEFINE_BIND_FUNCTIONNAME( CreateDevice );

	DEFINE_BIND_TYPENAME(Device);

	void	CreateDevice(Bind::TCallback& Params);
}




void ApiXr::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<BindFunction::CreateDevice>( CreateDevice, Namespace );

	Context.BindObjectType<TDeviceWrapper>(Namespace);
}


void ApiXr::TDeviceWrapper::Construct(Bind::TCallback& Params)
{
}


void ApiXr::TDeviceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
}

void ApiXr::CreateDevice(Bind::TCallback& Params)
{
	//	first param should be a render context
	//	we wanna work out the type here
	//	openxr needs the win32 opengl window info (currently inside platform::topenglwindow)
	//	gr: should we hold onto the js object here? or a sharedptr to the window/context inside...
	auto& WindowWrapper = Params.GetArgumentPointer<ApiOpengl::TWindowWrapper>(0);
	Win32::TOpenglContext* pWin32OpenglContext = WindowWrapper.GetWin32OpenglContext();
	//Directx::TContext* pDirectxContext = WindowWrapper.GetDirectContext();

	auto pPromise = Params.mLocalContext.mGlobalContext.CreatePromisePtr(Params.mLocalContext, __PRETTY_FUNCTION__);
	Params.Return(*pPromise);

	auto Resolve = [=](Bind::TLocalContext& Context) mutable
	{
		try
		{
			//	now try and create a device from differnt systems and with different render contexts
			std::shared_ptr<Xr::TDevice> Device;

			if (pWin32OpenglContext)
				Device = Openxr::CreateDevice(*pWin32OpenglContext);
			//if (pDirectxContext)
			//	Device = Openxr::CreateDevice(*pDirectxContext);

			//	maybe this is backwards and we should get the OpenXr Device 
			//	then make an object wrapper
			auto DeviceObject = Context.mGlobalContext.CreateObjectInstance(Context, TDeviceWrapper::GetTypeName());
			auto& DeviceWrapper = DeviceObject.This<TDeviceWrapper>();
			DeviceWrapper.mDevice = Device;
			pPromise->Resolve( Context, DeviceObject );
		}
		catch (std::exception& e)
		{
			pPromise->Reject(Context, e.what());
		}
	};
	Params.mContext.Queue(Resolve);
}
