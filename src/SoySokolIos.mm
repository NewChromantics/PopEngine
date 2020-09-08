#include "TApiSokol.h"
#include "PopMain.h"
import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

class SokolMetalContext : ApiSokol::TSokolContext::TSokolContext(	std::shared_ptr<SoyWindow> 	mSoyWindow )
{
public:
  SokolMetalContext( std::shared_ptr<SoyWindow> 	mSoyWindow );

  sg_context_desc					GetSokolContext() override;

public:
  sg_context_desc         mContextDesc;
  int                     sample_count;
  MTKView                 mMetalView;
  id<MTLDevice>           mMetalDevice;

  void*                   osx_mtk_get_render_pass_descriptor(void* user_data);
  void*                   osx_mtk_get_drawable(void* user_data);
}

SokolMetalContext::SokolMetalContext( std::shared_ptr<SoyWindow> 	mSoyWindow )
{
  // Needs to be written
  mMetalView = mSoyWindow->GetMetalView;

  mMetalDevice = MTLCreateSystemDefaultDevice();
  
  [mMetalView setDevice: mtl_device];

  mContextDesc = (sg_context_desc)
  {
    .sample_count = 1
    .metal = 
    {
      .device= (__bridge const void*) mMetalDevice,
      renderpass_descriptor_userdata_cb = osx_mtk_get_render_pass_descriptor,
      .drawable_userdata_cb = osx_mtk_get_drawable,
      .user_data = 0xABCDABCD
    }
  };

}

sg_context_desc SokolMetalContext::GetSokolContext()
{
	return mContextDesc;
}

SokolMetalContext::osx_mtk_get_render_pass_descriptor(void* user_data) 
{
  assert(user_data == (void*)0xABCDABCD);
  return (__bridge const void*) [mMetalView currentRenderPassDescriptor];
}

SokolMetalContext::osx_mtk_get_drawable(void* user_data) 
{
  assert(user_data == (void*)0xABCDABCD);
  return (__bridge const void*) [mMetalView currentDrawable];
}
