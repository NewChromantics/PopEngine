#include "TApiSokol.h"

#include "PopMain.h"
#include "TApiGui.h"
#include "SoyWindow.h"
#include "TApiCommon.h"

#include "sokol/sokol_gfx.h"




namespace ApiSokol
{
	const char Namespace[] = "Pop.Sokol";

	DEFINE_BIND_TYPENAME(Context);
	//DEFINE_BIND_FUNCTIONNAME(StartRender);
}

void ApiSokol::Bind(Bind::TContext &Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TSokolContextWrapper>(Namespace);
}

void ApiSokol::TSokolContextWrapper::CreateTemplate(Bind::TTemplate &Template)
{
	//Template.BindFunction<BindFunction::StartRender>(&TSokolContextWrapper::StartRender);
}


void ApiSokol::TSokolContextWrapper::OnPaint(sg_context Context,vec2x<size_t> ViewRect)
{
	sg_activate_context(Context);

	static int Counter = 0;
	Counter++;
	auto Blue = (Counter%60)/60.0f;

	sg_pass_action mPassAction = {0};
	mPassAction.colors[0] = {.action = SG_ACTION_CLEAR, .val = {0.0f, 1.0f, Blue, 1.0f}};

	//	draw one frame
	sg_begin_default_pass(
		&mPassAction,
		ViewRect.x,
		ViewRect.y
	);

	sg_end_pass();
	sg_commit();
	//auto Error = glGetError();
	//std::Debug << "error=" << Error << std::endl;

}

void ApiSokol::TSokolContextWrapper::Construct(Bind::TCallback &Params)
{
	auto Window = Params.GetArgumentObject(0);

	// Set TPersistent Pointer
	auto mWindow = Bind::TPersistent( Params.mLocalContext, Window, "Window Object" );

	auto LocalContext = Params.mLocalContext;
	auto WindowObject = mWindow.GetObject(LocalContext);
	auto& WindowWrapper = WindowObject.This<ApiGui::TWindowWrapper>();
	auto mSoyWindow = WindowWrapper.mWindow;
	
	Sokol::TContextParams SokolParams;
	
	// tsdk: If there is a specific view to target, store its name
	if ( !Params.IsArgumentUndefined(1) )
	{
		SokolParams.mViewName = Params.GetArgumentString(1);
	}
	
	SokolParams.mFramesPerSecond = 60;
	SokolParams.mOnPaint = [this](sg_context Context,vec2x<size_t> Rect)	{	this->OnPaint(Context,Rect);	};
	

	//	create platform-specific context
	mSokolContext = Sokol::Platform_CreateContext(mSoyWindow,SokolParams);
}
