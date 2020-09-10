#include "TApiSokol.h"
#include "PopMain.h"
#include "sokol/sokol_gfx.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

class SokolMetalContext : public ApiSokol::TSokolContext
{
public:
	SokolMetalContext( std::shared_ptr<SoyWindow> mSoyWindow, int SampleCount );

	sg_context_desc					GetSokolContext() override;

public:
	sg_context_desc         		mContextDesc;
	MTKView*             			mMetalView;
	id<MTLDevice>         			mMetalDevice;
	void*							mUserData;
};

sg_context_desc SokolMetalContext::GetSokolContext()
{
	return mContextDesc;
}


SokolMetalContext::SokolMetalContext(std::shared_ptr<SoyWindow> mSoyWindow, int SampleCount ) : ApiSokol::TSokolContext(mSoyWindow, SampleCount)
{
	// Needs to be written
	mMetalView* = mSoyWindow->GetMetalView;
	
	// tsdk: Leaving this as a stub for now
	mUserData = (void*)0xABCDABCD;

	mMetalDevice = MTLCreateSystemDefaultDevice();
  
	[mMetalView setDevice: mMetalDevice];
	 
	auto GetRenderPassDescriptor = [](void* user_data)
	{
		auto* This = reinterpret_cast<SokolMetalContext*>(user_data);
		auto* MetalView = This->mMetalView;
			
		assert(This->mUserData == (void*)0xABCDABCD);
		return (__bridge const void*) [MetalView currentRenderPassDescriptor];
	};
	
	auto GetDrawable = [](void* user_data)
	{
		auto* This = reinterpret_cast<SokolMetalContext*>(user_data);
		auto* MetalView = This->mMetalView;
		
		assert(This->mUserData == (void*)0xABCDABCD);
		return (__bridge const void*) [MetalView currentDrawable];
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
