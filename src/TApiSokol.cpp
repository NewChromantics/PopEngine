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

void ApiSokol::Bind(Bind::TContext& Context)
{
    Context.CreateGlobalObjectInstance("", Namespace);
    Context.BindObjectType<TSokolWrapper>(Namespace);
}

void ApiSokol::TSokolWrapper::CreateTemplate(Bind::TTemplate& Template)
{
    Template.BindFunction<BindFunction::Render>( &TSokolWrapper::Render );
}

static sg_pass_action pass_action;

static void Init(sg_desc desc) {

	sg_setup(&desc);

	/* setup pass action to clear to red */
	pass_action.colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } };

}

static void Frame(void) {
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


void ApiSokol::TSokolWrapper::Construct(Bind::TCallback& Params)
{
	auto mWindow = Params.GetArgumentObject(0);
	
	sg_desc desc;
#if defined(TARGET_OSX) || defined(TARGET_IOS)
//    desc = {
//		.metal = {
//			.device = ParentWindow.GetMetalDevice(Context),
//			.renderpass_descriptor_userdata_cb = ParentWindow.GetRenderPassDescriptor(Context),
//			.drawable_userdata_cb = ParentWindow.GetDrawable(Context),
//			.user_data = 0xABCDABCD
//		}
//	}
#elif defined(TARGET_LINUX)
	desc = { 0 };
#endif
	
	// Initialise Sokol
	Init(desc);
}

void ApiSokol::TSokolWrapper::Render(Bind::TCallback& Params)
{
	auto LocalContext = Params.mLocalContext;
	auto WindowObject = this->mWindow.GetObject(LocalContext);
	auto& Window = WindowObject.This<ApiGui::TWindowWrapper>();
	// Attach Frame to Draw Function
	Window.Render( Frame );
}






/*
//Function to get the necessary metal context for ios/osx
sg_context_desc GetContext(int sample_count)
{
	return (sg_context_desc) {
       .sample_count = sample_count,
#if defined(TARGET_OSX) || defined(TARGET_IOS)
       .metal = {
           .device = (__bridge const void*) mtl_device,
           .renderpass_descriptor_userdata_cb = osx_mtk_get_render_pass_descriptor,
           .drawable_userdata_cb = osx_mtk_get_drawable,
           .user_data = 0xABCDABCD
       }
#endif
   };
}
*/

/* GLFW SETUP */
/*
void AttachRenderToMainThread()
{
	auto& Thread = *Soy::Platform::gMainThread;
	Thread.PushJob( GLFWTest );
}


//int GLFWTest()
{
	const int WIDTH = 640;
    const int HEIGHT = 480;
    // create window and GL context via GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Clear GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit();
    // setup sokol_gfx
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());
    // default pass action, clear to red
    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };
    // draw loop
    while (!glfwWindowShouldClose(w)) {
        float g = (float)(pass_action.colors[0].val[1] + 0.01);
        if (g > 1.0f) g = 0.0f;
        pass_action.colors[0].val[1] = g;
        int cur_width, cur_height;
        glfwGetFramebufferSize(w, &cur_width, &cur_height);
        sg_begin_default_pass(&pass_action, cur_width, cur_height);
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }
    // shutdown sokol_gfx and GLFW
    sg_shutdown();
    glfwTerminate();
	return 0;
}

*/
