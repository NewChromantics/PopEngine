#include "TApiSokol.h"
#include "PopMain.h"

#include "SoyWindowApple.h"
#include "SoySokol_Ios_Metal.h"
#include "SoySokol_Ios_Gles.h"

//	metal implementation
#define SOKOL_IMPL
#define SOKOL_METAL
//	wrapping this include in a namespace, means we dont get duplicate symbols
//	but we may need to put using namespace MetalSokol; anywhere we use it here
namespace MetalSokol
{
	#include "sokol/sokol_gfx.h"
}


//	this could do metal & gl
@interface SokolViewDelegate_Metal : UIResponder<MTKViewDelegate>
	
@property std::function<void(CGRect)>	mOnPaint;
@property CGSize						mSize;
- (instancetype)init:(std::function<void(CGRect)> )OnPaint;

@end



@implementation SokolViewDelegate_Metal
	
- (instancetype)init:(std::function<void(CGRect)> )OnPaint
{
	self = [super init];
	self.mOnPaint = OnPaint;
	self.mSize = {123,456};
	return self;
}
	
- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
}
	
- (void)drawInMTKView:(nonnull MTKView *)view
{
	CGRect Rect;
	Rect.origin = {0,0};
	Rect.size = self.mSize;
	//	read old rect here
	self.mOnPaint(Rect);
}
	
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
	self.mSize = size;
}

@end


std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params)
{
	auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);
	
	//	get the view with matching name, if it's a metal view, make a metal context
	//	if its gl, make a gl context
	auto* View = PlatformWindow.GetChild(Params.mViewName);
	if ( !View )
	{
		std::stringstream Error;
		Error << "Failed to find child view " << Params.mViewName << " (required on ios)";
		throw Soy::AssertException(Error);
	}
	
	auto ClassName = Soy::NSStringToString(NSStringFromClass([View class]));
	std::Debug << "View " << Params.mViewName << " class name " << ClassName << std::endl;
	
	if ( ClassName == "MTKView" )
	{
		MTKView* MetalView = (MTKView*)View;
		auto* Context = new SokolMetalContext(Window,MetalView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
	
#if defined(ENABLE_OPENGL)
	if ( ClassName == "GLKView" )
	{
		GLKView* GlView = (GLKView*)View;
		auto* Context = new SokolOpenglContext(Window,GlView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
#endif
	
	std::stringstream Error;
	Error << "Class of view " << Params.mViewName << " is not MTKView or GLKView; " << ClassName;
	throw Soy::AssertException(Error);
}






SokolMetalContext::SokolMetalContext(std::shared_ptr<SoyWindow> Window,MTKView* View,Sokol::TContextParams Params) :
	mView	( View ),
	mParams	( Params )
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

	sg_context_desc mContextDesc = (sg_context_desc)
	{
		.sample_count = 1,//mParams.mSampleCount,
		.metal =
		{
			.device= (__bridge const void*) mMetalDevice,
			.renderpass_descriptor_userdata_cb = GetRenderPassDescriptor,
			.drawable_userdata_cb = GetDrawable,
			.user_data = this
		}
	};

}

void SokolMetalContext::Queue(std::function<void(sg_context)> Exec)
{
	Soy_AssertTodo();
}

