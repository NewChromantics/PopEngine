#include "TApiSokol.h"
#include "PopMain.h"
#include "sokol/sokol_gfx.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

class SokolMetalContext : public ApiSokol::TSokolContext
{
public:
  SokolMetalContext( std::shared_ptr<SoyWindow> 	mSoyWindow );

  sg_context_desc					GetSokolContext() override;

public:
  sg_context_desc         mContextDesc;
  int                     sample_count;
  MTKView                 *mMetalView;
  id<MTLDevice>           mMetalDevice;

  const void*                   GetRenderPassDescriptor(void* user_data);
  const void*                   GetDrawable(void* user_data);
};

sg_context_desc SokolMetalContext::GetSokolContext()
{
	return mContextDesc;
}

const void* SokolMetalContext::GetRenderPassDescriptor(void* user_data)
{
  assert(user_data == (void*)0xABCDABCD);
  return (__bridge const void*) [mMetalView currentRenderPassDescriptor];
}

const void* SokolMetalContext::GetDrawable(void* user_data)
{
  assert(user_data == (void*)0xABCDABCD);
  return (__bridge const void*) [mMetalView currentDrawable];
}

SokolMetalContext::SokolMetalContext(std::shared_ptr<SoyWindow> mSoyWindow ) : ApiSokol::TSokolContext(mSoyWindow)
{
  // Needs to be written
  mMetalView* = mSoyWindow->GetMetalView;

  mMetalDevice = MTLCreateSystemDefaultDevice();
  
  [mMetalView setDevice: mMetalDevice];

  mContextDesc = (sg_context_desc)
  {
    .sample_count = 1,
    .metal =
    {
      .device= (__bridge const void*) mMetalDevice,
	  .renderpass_descriptor_userdata_cb = [this](){ this->GetRenderPassDescriptor(); },
      .drawable_userdata_cb = [this](){ this->GetDrawable(); },
      .user_data = 0xABCDABCD
    }
  };

}
