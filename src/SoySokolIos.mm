#include "TApiSokol.h"
#include "SoyWindowIos.h"
#include "PopMain.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import <GLKit/GLKit.h>

#define SOKOL_IMPL
#define SOKOL_GLES2
//#define SOKOL_METAL
#include "sokol/sokol_gfx.h"


class SokolMetalContext : public Sokol::TContext
{
public:
	SokolMetalContext(std::shared_ptr<SoyWindow> Window,MTKView* View,int SampleCount);

	sg_context_desc					GetSokolContext() override	{	return mContextDesc;	}

public:
	sg_context_desc         		mContextDesc;
	MTKView*             			mView = nullptr;
	id<MTLDevice>         			mMetalDevice;
};

class SokolOpenglContext : public Sokol::TContext
{
public:
	SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLKView* View,int SampleCount);
	
	sg_context_desc					GetSokolContext() override	{	return mContextDesc;	}
	
public:
	sg_context_desc         		mContextDesc;
	GLKView*             			mView = nullptr;
};



std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(std::shared_ptr<SoyWindow> Window,const std::string& ViewName,int SampleCount)
{
	auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);
	
	//	get the view with matching name, if it's a metal view, make a metal context
	//	if its gl, make a gl context
	auto* View = PlatformWindow.GetChild(ViewName);
	if ( !View )
	{
		std::stringstream Error;
		Error << "Failed to find child view " << ViewName << " (required on ios)";
		throw Soy::AssertException(Error);
	}
	
	auto ClassName = Soy::NSStringToString(NSStringFromClass([View class]));
	std::Debug << "View " << ViewName << " class name " << ClassName << std::endl;
	
	if ( ClassName == "MTKView" )
	{
		//	todo: proper obj-c cast
		MTKView* MetalView = View;
		auto* Context = new SokolMetalContext(Window,MetalView,SampleCount);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
	
	if ( ClassName == "GLKView" )
	{
		//	todo: proper obj-c cast
		GLKView* GlView = View;
		auto* Context = new SokolOpenglContext(Window,GlView,SampleCount);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
	
	std::stringstream Error;
	Error << "Class of view " << ViewName << " is not MTKView or GLKView; " << ClassName;
	throw Soy::AssertException(Error);
}






SokolMetalContext::SokolMetalContext(std::shared_ptr<SoyWindow> Window,MTKView* View,int SampleCount) :
	mView	( View )
{
	mMetalDevice = MTLCreateSystemDefaultDevice();
 	[mView setDevice: mMetalDevice];
	 
	auto GetRenderPassDescriptor = [](void* user_data)
	{
		auto* This = reinterpret_cast<SokolMetalContext*>(user_data);
		auto* MetalView = This->mView;
		auto* Descriptor = (__bridge const void*) [MetalView currentRenderPassDescriptor];
		return Descriptor;
	};
	
	auto GetDrawable = [](void* user_data)
	{
		auto* This = reinterpret_cast<SokolMetalContext*>(user_data);
		auto* MetalView = This->mView;
		auto* Drawable = (__bridge const void*) [MetalView currentDrawable];
		return Drawable;
	};

	mContextDesc = (sg_context_desc)
	{
		.sample_count = SampleCount,
		.metal =
		{
			.device= (__bridge const void*) mMetalDevice,
			.renderpass_descriptor_userdata_cb = GetRenderPassDescriptor,
			.drawable_userdata_cb = GetDrawable,
			.user_data = this
		}
	};

}


SokolOpenglContext::SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLKView* View,int SampleCount) :
	mView	( View )
{
	Soy_AssertTodo();
}
