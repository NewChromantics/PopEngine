#include "TApiSokol.h"
#include "PopMain.h"
#include "SoyOpenglWindow.h"
#include "SoyOpenglView.h"
#include "TApiGui.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

#define SOKOL_IMPL
#define SOKOL_METAL
#include "sokol/sokol_gfx.h"

//static sg_pass_action pass_action;
//
//static void init(void) {
//
//	sg_desc desc = {
//		.context = osx_get_context()
//	};
//	sg_setup(&desc);
//
//    /* setup pass action to clear to red */
//    pass_action = (sg_pass_action) {
//        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
//    };
//}
//
//static void frame(void) {
//    /* animate clear colors */
//    float g = pass_action.colors[0].val[1] + 0.01f;
//    if (g > 1.0f) g = 0.0f;
//    pass_action.colors[0].val[1] = g;
//
//    /* draw one frame */
//	/* Need to write width and height callbacks ! */
////    sg_begin_default_pass(&pass_action, osx_width(), osx_height());
//    sg_end_pass();
//    sg_commit();
//}
//
//static void shutdown(void) {
//    sg_shutdown();
//}
//
//int MetalTest(sg_context_desc) {
////    osx_start(640, 480, 1, "Sokol Clear (Metal)", init, frame, shutdown);
//    return 0;
//}

namespace ApiSokol
{
    const char Namespace[] = "Pop.Sokol";

    DEFINE_BIND_TYPENAME(Sokol);
    DEFINE_BIND_FUNCTIONNAME(Test);
}

void ApiSokol::Bind(Bind::TContext& Context)
{
    Context.CreateGlobalObjectInstance("", Namespace);
    Context.BindObjectType<TSokolWrapper>(Namespace);
}

class SoySokol
{
	
};

void ApiSokol::TSokolWrapper::CreateTemplate(Bind::TTemplate& Template)
{
    Template.BindFunction<BindFunction::Test>( &TSokolWrapper::Test );
}

void ApiSokol::TSokolWrapper::Construct(Bind::TCallback& Params)
{
	auto& ParentWindow = Params.GetArgumentPointer<ApiGui::TWindowWrapper>(0);
	auto mLabel = Platform::GetLabel( *ParentWindow.mWindow, "*" );
//	std::cout << mLabel << "\n";
//	Platform::GetLabel(Window, 'test');
//	auto label = Platform::GetLabel(Window, '
//	sg_context_desc Platform::GetSoyContext(*ParentWindow.mWindow);
	
//	MetalTest(sg_context_desc);
//	MetalView Window = ParentWindow.GetMetalView();
//	Createsokol(MetalView)
}

//int GLFWTest()
//{
//	const int WIDTH = 640;
//    const int HEIGHT = 480;
//    /* create window and GL context via GLFW */
//    glfwInit();
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//	GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Clear GLFW", 0, 0);
//    glfwMakeContextCurrent(w);
//    glfwSwapInterval(1);
//    flextInit();
//    /* setup sokol_gfx */
//    sg_desc desc = {0};
//    sg_setup(&desc);
//    assert(sg_isvalid());
//    /* default pass action, clear to red */
//    sg_pass_action pass_action = {
//        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
//    };
//    /* draw loop */
//    while (!glfwWindowShouldClose(w)) {
//        float g = (float)(pass_action.colors[0].val[1] + 0.01);
//        if (g > 1.0f) g = 0.0f;
//        pass_action.colors[0].val[1] = g;
//        int cur_width, cur_height;
//        glfwGetFramebufferSize(w, &cur_width, &cur_height);
//        sg_begin_default_pass(&pass_action, cur_width, cur_height);
//        sg_end_pass();
//        sg_commit();
//        glfwSwapBuffers(w);
//        glfwPollEvents();
//    }
//    /* shutdown sokol_gfx and GLFW */
//    sg_shutdown();
//    glfwTerminate();
//	return 0;
//}

void ApiSokol::TSokolWrapper::Test(Bind::TCallback& Params)
{
	Soy::Rectx<int32_t> Rect(0, 1, 500, 500);
	auto window = Platform::CreateWindow("test", Rect, false);
//	window.renderview;
//	auto& Thread = *Soy::Platform::gMainThread;
//	Thread.PushJob( MetalTest );
//	MetalTest();
}
