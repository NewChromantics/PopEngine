#include "TApiXr.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"
#include "SoyOpengl.h"

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
	CreateDevice();
}

void ApiXr::TDeviceWrapper::CreateDevice()
{
	std::stringstream CreateErrors;

#if defined(ENABLE_OPENXR)
	try
	{
		mDevice = Openxr::CreateDevice();
		return;
	}
	catch (std::exception& e)
	{
		CreateErrors << "Error creating openxr device " << e.what();
	}
#endif

#if defined(TARGET_IOS)
	try
	{
		mDevice.reset(new ArkitSession());
		return;
	}
	catch (std::exception& e)
	{
		CreateErrors << "Error creating openxr device " << e.what();
	}
#endif

	std::stringstream Error;
	Error << "Failed to create XR device (may not be supported on this platform) errors; " << CreateErrors.str();
	throw Soy::AssertException(Error);
}



void ApiXr::TDeviceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
}

void ApiXr::CreateDevice(Bind::TCallback& Params)
{
	auto pPromise = Params.mLocalContext.mGlobalContext.CreatePromisePtr(Params.mLocalContext, __PRETTY_FUNCTION__);
	Params.Return(*pPromise);

	auto Resolve = [=](Bind::TLocalContext& Context) mutable
	{
		try
		{
			//	maybe this is backwards and we should get the OpenXr Device 
			//	then make an object wrapper
			auto DeviceObject = Context.mGlobalContext.CreateObjectInstance(Context, TDeviceWrapper::GetTypeName());
			pPromise->Resolve( Context, DeviceObject );
		}
		catch (std::exception& e)
		{
			pPromise->Reject(Context, e.what());
		}
	};
	Params.mContext.Queue(Resolve);
}
