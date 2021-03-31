#include "TApiSokol.h"
#include "PopMain.h"

#include "SoyGuiApple.h"
#include "SoyWindowApple.h"
#include "SoySokol_Ios_Metal.h"
#include "SoySokol_Ios_Gles.h"
/*
//	metal implementation
#define SOKOL_IMPL
#define SOKOL_METAL
//	wrapping this include in a namespace, means we dont get duplicate symbols
//	but we may need to put using namespace MetalSokol; anywhere we use it here
namespace MetalSokol
{
	#include "sokol/sokol_gfx.h"
}
*/

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


std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(Sokol::TContextParams Params)
{
	auto RenderView = std::dynamic_pointer_cast<Platform::TRenderView>( Params.mRenderView );
	if ( !RenderView )
		throw Soy::AssertException("Found render view but is not a Platform::TRenderView");
	auto& PlatformRenderView = *RenderView;
	
	auto* GlView = PlatformRenderView.GetOpenglView();
	auto* MetalView = PlatformRenderView.GetMetalView();
	
#if defined(ENABLE_OPENGL)
	if ( GlView )
	{
		//	gr: reaplce with PlatformRenderView as it needs to be held onto
		auto* Context = new SokolOpenglContext(GlView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
#endif
	
#if defined(ENABLE_METAL)
	if ( MetalView )
	{
		auto* Context = new SokolMetalContext(MetalView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
#endif
	throw Soy::AssertException("Found view, but no underlaying metal/opengl view");
}







SokolMetalContext::SokolMetalContext(MTKView* View,Sokol::TContextParams Params) :
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

