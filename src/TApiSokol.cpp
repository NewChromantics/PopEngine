#include "TApiSokol.h"
#include "PopMain.h"


#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "sokol-samples/glfw/flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol/sokol_gfx.h"

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

}

int test()
{
	const int WIDTH = 640;
    const int HEIGHT = 480;
    /* create window and GL context via GLFW */
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Sokol Clear GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit();
    /* setup sokol_gfx */
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());
    /* default pass action, clear to red */
    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };
    /* draw loop */
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
    /* shutdown sokol_gfx and GLFW */
    sg_shutdown();
    glfwTerminate();
	return 0;
}

void ApiSokol::TSokolWrapper::Test(Bind::TCallback& Params)
{
	auto& Thread = *Soy::Platform::gMainThread;
	Thread.PushJob( test );
}
