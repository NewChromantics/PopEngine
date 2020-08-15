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
	void	GetRenderContext(std::shared_ptr<Win32::TOpenglContext>& Win32OpenglContext, std::shared_ptr<Directx::TContext>& DirectX11Context, Bind::TCallback& Params, size_t ArgumentIndex);
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

void ApiXr::GetRenderContext(std::shared_ptr<Win32::TOpenglContext>& Win32OpenglContext, std::shared_ptr<Directx::TContext>& DirectX11Context, Bind::TCallback& Params, size_t ArgumentIndex)
{
	//	try and get opengl context
	try
	{
		auto& WindowWrapper = Params.GetArgumentPointer<ApiOpengl::TWindowWrapper>(ArgumentIndex);
		auto Win32OpenglContext = WindowWrapper.GetWin32OpenglContext();
		if (Win32OpenglContext)
			return;
	}
	catch (std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " failed to get opengl context " << e.what() << std::endl;
	}
		//Directx::TContext* pDirectxContext = WindowWrapper.GetDirectContext();
	try
	{
		auto& WindowWrapper = Params.GetArgumentPointer<ApiOpengl::TWindowWrapper>(ArgumentIndex);
		auto Win32OpenglContext = WindowWrapper.GetWin32OpenglContext();
		if (Win32OpenglContext)
			return;
	}
	catch (std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " failed to get opengl context " << e.what() << std::endl;
	}

	throw Soy::AssertException("Failed to get opengl or directx context from object");
}

void ApiXr::CreateDevice(Bind::TCallback& Params)
{
	//	first param should be a render context
	//	we wanna work out the type here
	//	openxr needs the win32 opengl window info (currently inside platform::topenglwindow)
	//	gr: should we hold onto the js object here? or a sharedptr to the window/context inside...
	std::shared_ptr<Win32::TOpenglContext> Win32OpenglContext;
	std::shared_ptr<Directx::TContext> Directx11Context;
	GetRenderContext(Win32OpenglContext, Directx11Context, Params, 0);

	auto pPromise = Params.mLocalContext.mGlobalContext.CreatePromisePtr(Params.mLocalContext, __PRETTY_FUNCTION__);
	Params.Return(*pPromise);

	auto Resolve = [=](Bind::TLocalContext& Context) mutable
	{
		try
		{
			//	now try and create a device from differnt systems and with different render contexts
			std::shared_ptr<Xr::TDevice> Device;

			if (Win32OpenglContext )
				Device = Openxr::CreateDevice(*Win32OpenglContext);
			if (Directx11Context)
				Device = Openxr::CreateDevice(*Directx11Context);

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
