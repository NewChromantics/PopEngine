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

extern int glGetError();

void ApiSokol::TSokolContextWrapper::OnPaint(vec2x<size_t> ViewRect)
{
	static int Counter = 0;
	Counter++;
	
	mPassAction.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1.0f, 0.0f, 0.0f, 1.0f}};
	auto Blue = (Counter%60)/60.0f;
	mPassAction.colors[0].val[0] = 1.0f;
	mPassAction.colors[0].val[1] = 0.0f;
	mPassAction.colors[0].val[2] = Blue;

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
	mWindow = Bind::TPersistent( Params.mLocalContext, Window, "Window Object" );

	auto LocalContext = Params.mLocalContext;
	auto WindowObject = this->mWindow.GetObject(LocalContext);
	auto& WindowWrapper = WindowObject.This<ApiGui::TWindowWrapper>();
	mSoyWindow = WindowWrapper.mWindow;
	
	Sokol::TContextParams SokolParams;
	
	// tsdk: If there is a specific view to target, store its name
	if ( !Params.IsArgumentUndefined(1) )
	{
		SokolParams.mViewName = Params.GetArgumentString(1);
	}
	
	SokolParams.mFramesPerSecond = 60;
	SokolParams.mOnPaint = [this](vec2x<size_t> Rect)	{	this->OnPaint(Rect);	};
	
	//	create platform-specific context
	mSokolContext = Sokol::Platform_CreateContext(mSoyWindow,SokolParams);
	
	sg_desc desc =
	{
		.context = mSokolContext->GetSokolContext()
	};

	//	Initialise Sokol
	sg_setup(&desc);
}
