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
	DEFINE_BIND_FUNCTIONNAME(StartRender);
}

void ApiSokol::Bind(Bind::TContext &Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TSokolContextWrapper>(Namespace);
}

void ApiSokol::TSokolContextWrapper::CreateTemplate(Bind::TTemplate &Template)
{
	Template.BindFunction<BindFunction::StartRender>(&TSokolContextWrapper::StartRender);
}

void ApiSokol::TSokolContextWrapper::Init(sg_desc desc)
{
	sg_setup(&desc);

	/* setup pass action to clear to red */
	mPassAction.colors[0] = {.action = SG_ACTION_CLEAR, .val = {1.0f, 0.0f, 0.0f, 1.0f}};
}

void ApiSokol::TSokolContextWrapper::RenderFrame()
{
	//tsdk: need to get the views width and height -> hardcoding for now
	auto ScreenRect = mSoyWindow->GetScreenRect();

	/* animate clear colors */
	float g = mPassAction.colors[0].val[1] + 0.01f;
	if (g > 1.0f)
		g = 0.0f;
	mPassAction.colors[0].val[1] = g;

	/* draw one frame */
	sg_begin_default_pass(
		&mPassAction,
		100,
		100
//		ScreenRect.w,
//		ScreenRect.h
	);

	sg_end_pass();
	sg_commit();
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
	
	// tsdk: If there is a specific view to target, store its name
	if ( !Params.IsArgumentUndefined(1) )
	{
		mViewName = Params.GetArgumentString(1);
	}
	
	// tsdk: Make sure to zero initialise the Member Variables
	mPassAction = {0};
	//sg_desc desc = {0};

	// tsdk: Set the sample count
	int SampleCount = 1;
	
	// Create Context
	mSokolContext = Sokol::Platform_CreateContext(mSoyWindow, mViewName, SampleCount );
	
	sg_desc desc =
	{
		.context = mSokolContext->GetSokolContext()
	};

	// Initialise Sokol
	Init(desc);

	std::function<void()> Frame = [this](){ this->RenderFrame(); };
	mSoyWindow->StartRender(Frame, mViewName);
}

void ApiSokol::TSokolContextWrapper::StartRender(Bind::TCallback &Params)
{
	
}
