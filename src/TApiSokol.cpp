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
	
	//	init last-frame for any paints before we get a chance to render
	//	gr: maybe this should be like a Render() call and use js
	InitDebugFrame(mLastFrame);
	
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

void ApiSokol::TSokolContextWrapper::InitDebugFrame(Sokol::TRenderCommands& Commands)
{
	{
		auto pClear = std::make_shared<Sokol::TRenderCommand_Clear>();
		pClear->mColour[0] = 0;
		pClear->mColour[1] = 1;
		pClear->mColour[2] = 1;
		pClear->mColour[3] = 1;
		Commands.mCommands.PushBack(pClear);
	}
}

void ApiSokol::TSokolContextWrapper::Render(Bind::TCallback& Params)
{
	//	request render from context asap
	//	gr: maybe this will end up being an immediate callback before we can queue the new command, and request the prev one
	//		in that case, generate render command then request
	mSokolContext->RequestPaint();

	auto Promise = mPendingFrames.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	//	parse command
	try
	{
		//	gr: we NEED this to match the promise...
		auto CommandsArray = Params.GetArgumentArray(0);
		auto Commands = Sokol::ParseRenderCommands(Params.mLocalContext,CommandsArray);
		mPendingFrames.Push(Commands);
	}
	catch(std::exception& e)
	{
		Promise.Reject(Params.mLocalContext, e.what());
	}
}

std::shared_ptr<Sokol::TRenderCommandBase> Sokol::ParseRenderCommand(const std::string_view& Name,Bind::TCallback& Params)
{
	if ( Name == TRenderCommand_Clear::Name )
	{
		auto pClear = std::make_shared<TRenderCommand_Clear>();
		//	for speed, would a typed array be faster, then we can memcpy?
		pClear->mColour[0] = Params.GetArgumentFloat(1);
		pClear->mColour[1] = Params.GetArgumentFloat(2);
		pClear->mColour[2]= Params.GetArgumentFloat(3);
		pClear->mColour[3] = Params.IsArgumentUndefined(4) ? 1 : Params.GetArgumentFloat(4);
		return pClear;
	}
	
	std::stringstream Error;
	Error << "Unknown render command " << Name;
	throw Soy::AssertException(Error);
}

Sokol::TRenderCommands Sokol::ParseRenderCommands(Bind::TLocalContext& Context,Bind::TArray& CommandArrayArray)
{
	Sokol::TRenderCommands Commands;
	//	go through the array of commands
	//	parse each one. Dont fill gaps here, this system should be generic
	auto CommandArray = CommandArrayArray.GetAsCallback(Context);
	for ( auto c=0;	c<CommandArray.GetArgumentCount();	c++ )
	{
		auto CommandAndParamsArray = CommandArray.GetArgumentArray(c);
		auto CommandAndParams = CommandAndParamsArray.GetAsCallback(Context);
		auto CommandName = CommandAndParams.GetArgumentString(0);
		//	now each command should be able to pull the values it needs
		auto Command = ParseRenderCommand(CommandName,CommandAndParams);
		Commands.mCommands.PushBack(Command);
	}
	return Commands;
}

