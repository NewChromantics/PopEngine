#pragma once

#include "sokol/sokol_gfx.h"

DECLARE_NONCOMPLEX_TYPE(sg_color);
DECLARE_NONCOMPLEX_TYPE(sg_buffer);
DECLARE_NONCOMPLEX_TYPE(sg_image);
DECLARE_NONCOMPLEX_TYPE(sg_shader);
DECLARE_NONCOMPLEX_TYPE(sg_pipeline);
DECLARE_NONCOMPLEX_TYPE(sg_pass);

//#include "SoyWindow.h"
#include <memory>
#include <functional>
#include "SoyVector.h"


namespace Gui
{
	class TRenderView;
}

namespace Sokol
{
	class TContext;			//	platform context
	class TContextParams;	//	or ViewParams?

	std::shared_ptr<TContext>	Platform_CreateContext(TContextParams Params);
}

class Sokol::TContextParams
{
public:
	std::function<void(sg_context,vec2x<size_t>)>	mOnPaint;		//	render callback
	std::shared_ptr<Gui::TRenderView>				mRenderView;
	size_t											mFramesPerSecond = 120;
};
