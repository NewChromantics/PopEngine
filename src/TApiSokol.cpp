#include "TApiSokol.h"

#include "PopMain.h"
#include "TApiGui.h"
#include "SoyWindow.h"
#include "TApiCommon.h"

#if defined(TARGET_LINUX)
#include <stdlib.h>
#include "LinuxDRM/esUtil.h"
#define SOKOL_IMPL
#define SOKOL_GLES2
#endif

#include "sokol/sokol_gfx.h"

namespace ApiSokol
{
	const char Namespace[] = "Pop.Sokol";

	DEFINE_BIND_TYPENAME(RenderPipeline);
	DEFINE_BIND_FUNCTIONNAME(StartRender);
}

void ApiSokol::Bind(Bind::TContext &Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TSokolWrapper>(Namespace);
}

void ApiSokol::TSokolWrapper::CreateTemplate(Bind::TTemplate &Template)
{
	Template.BindFunction<BindFunction::StartRender>(&TSokolWrapper::StartRender);
}

void ApiSokol::TSokolWrapper::Init(sg_desc desc)
{
	sg_setup(&desc);

	/* setup pass action to clear to red */
	mPassAction.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1.0f, 0.0f, 0.0f, 1.0f}};
}

void ApiSokol::TSokolWrapper::RenderFrame()
{
	auto ScreenRect = mSoyWindow->GetScreenRect();

	/* animate clear colors */
	float g = mPassAction.colors[0].val[1] + 0.01f;
	if (g > 1.0f)
		g = 0.0f;
	mPassAction.colors[0].val[1] = g;

	/* draw one frame */
	sg_begin_default_pass(
		&mPassAction,
		ScreenRect.w,
		ScreenRect.h
	);

	sg_end_pass();
	sg_commit();
}

void ApiSokol::TSokolWrapper::Construct(Bind::TCallback &Params)
{
	// Set TPersistent Pointer
	auto Window = Params.GetArgumentObject(0);
	mWindow = Bind::TPersistent( Params.mLocalContext, Window, "Window Object" );

	auto LocalContext = Params.mLocalContext;
	auto WindowObject = this->mWindow.GetObject(LocalContext);
	auto& WindowWrapper = WindowObject.This<ApiGui::TWindowWrapper>();
	mSoyWindow = WindowWrapper.mWindow;

	// Make sure to zero initialise the Member Variables
	mPassAction = {0};
	sg_desc desc = {0};

	// Create Context
	auto* Context = new ApiSokol::TSokolContext(mSoyWindow);

	desc = 
	{
		.context = Context->GetSokolContext()
	};

	// Initialise Sokol
	Init(desc);

	std::function<void()> Frame = [this](){ this->RenderFrame(); };
	mSoyWindow->StartRender(Frame);
}

void ApiSokol::TSokolWrapper::StartRender(Bind::TCallback &Params)
{
	
}

//#if defined(TARGET_OSX) || defined(TARGET_IOS)
//	desc = {
//		.metal = {
//			.device = ParentWindow.GetMetalDevice(Context),
//			.renderpass_descriptor_userdata_cb = ParentWindow.GetRenderPassDescriptor(Context),
//			.drawable_userdata_cb = ParentWindow.GetDrawable(Context),
//			.user_data = 0xABCDABCD
//		}
//	}
//#endif