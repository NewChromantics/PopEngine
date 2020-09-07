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

	DEFINE_BIND_TYPENAME(Initialise);
	DEFINE_BIND_FUNCTIONNAME(Render);
}

void ApiSokol::Bind(Bind::TContext &Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TSokolWrapper>(Namespace);
}

void ApiSokol::TSokolWrapper::CreateTemplate(Bind::TTemplate &Template)
{
	Template.BindFunction<BindFunction::Render>(&TSokolWrapper::Render);
}

static sg_pass_action pass_action;

static void Init(sg_desc desc)
{
	sg_setup(&desc);

	/* setup pass action to clear to red */
	pass_action.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1.0f, 0.0f, 0.0f, 1.0f}};
}

static void Frame(void)
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
	auto mWindow = Params.GetArgumentObject(0);

	sg_desc desc { 0 };
#if defined(TARGET_OSX) || defined(TARGET_IOS)
//	desc = {
//		.metal = {
//			.device = ParentWindow.GetMetalDevice(Context),
//			.renderpass_descriptor_userdata_cb = ParentWindow.GetRenderPassDescriptor(Context),
//			.drawable_userdata_cb = ParentWindow.GetDrawable(Context),
//			.user_data = 0xABCDABCD
//		}
//	}
#endif
	// Initialise Sokol
	Init(desc);
}

void ApiSokol::TSokolWrapper::Render(Bind::TCallback &Params)
{
	auto LocalContext = Params.mLocalContext;
	auto WindowObject = this->mWindow.GetObject(LocalContext);
	auto &Window = WindowObject.This<ApiGui::TWindowWrapper>();
	// Attach Frame to Draw Function
	Window.Render(Frame);
}
