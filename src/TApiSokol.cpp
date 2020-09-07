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

static sg_pass_action pass_action;

static void Init(sg_desc desc)
{
	sg_setup(&desc);

	/* setup pass action to clear to red */
	pass_action.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1.0f, 0.0f, 0.0f, 1.0f}};
}

static void Frame()
{
	/* animate clear colors */
	float g = pass_action.colors[0].val[1] + 0.01f;
	if (g > 1.0f)
		g = 0.0f;
	pass_action.colors[0].val[1] = g;

	/* draw one frame */
	sg_begin_default_pass(
		&pass_action,
		300,
		300
	);

	sg_end_pass();
	sg_commit();
}

void ApiSokol::TSokolWrapper::Construct(Bind::TCallback &Params)
{
	// Set TPersistent Pointer
	auto Window = Params.GetArgumentObject(0);
	mWindow = Bind::TPersistent( Params.mLocalContext, Window, "Window Object" );

	//auto WindowObject = Params.GetArgumentPointer<ApiGui::TWindowWrapper>(0);

	// Get Context Desc
	// sg_desc desc = WindowObject.GetSokolContext();
	sg_desc desc = {0};

	// Initialise Sokol
	Init(desc);
}

void ApiSokol::TSokolWrapper::StartRender(Bind::TCallback &Params)
{
	auto LocalContext = Params.mLocalContext;
	auto WindowObject = this->mWindow.GetObject(LocalContext);
	auto &Window = WindowObject.This<ApiGui::TWindowWrapper>();

	// Attach Frame to Draw Function
	std::function<void()> mFrame = Frame;
	Window.StartRender(&mFrame);
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