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
	DEFINE_BIND_FUNCTIONNAME(Render);
}

void ApiSokol::Bind(Bind::TContext &Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TSokolContextWrapper>(Namespace);
}

void ApiSokol::TSokolContextWrapper::CreateTemplate(Bind::TTemplate &Template)
{
	Template.BindFunction<BindFunction::Render>(&TSokolContextWrapper::Render);
}


void ApiSokol::TSokolContextWrapper::OnPaint(sg_context Context,vec2x<size_t> ViewRect)
{
	sg_activate_context(Context);

	//	get last submitted render command
	//	fifo
	Sokol::TRenderCommands RenderCommands = mLastFrame;
	RenderCommands.mPromiseRef = Sokol::TRenderCommands().mPromiseRef;	//	invalidate promise ref so we don't try and resolve it next time
	{
		std::lock_guard<std::mutex> Lock(mPendingFramesLock);
		if ( !mPendingFrames.IsEmpty() )
			RenderCommands = mPendingFrames.PopAt(0);
	}
	
	bool InsidePass = false;
	
	auto NewPass = [&](float r,float g,float b,float a)
	{
		if ( InsidePass )
		{
			sg_end_pass();
			InsidePass = false;
		}
		
		sg_pass_action PassAction = {0};
		PassAction.colors[0] = {.action = SG_ACTION_CLEAR, .val = {r,g,b,a}};
		sg_begin_default_pass( &PassAction, ViewRect.x, ViewRect.y );
		InsidePass = true;
	};
	
	for ( auto i=0;	i<RenderCommands.mCommands.GetSize();	i++ )
	{
		auto& NextCommand = RenderCommands.mCommands[i];
		
		if ( NextCommand->GetName() == Sokol::TRenderCommand_Clear::Name )
		{
			auto& ClearCommand = dynamic_cast<Sokol::TRenderCommand_Clear&>( *NextCommand );
			NewPass( ClearCommand.mColour[0], ClearCommand.mColour[1], ClearCommand.mColour[2], ClearCommand.mColour[3] );
		}
		else if ( !InsidePass )
		{
			//	starting a pass without a clear, so do one
			NewPass(1,0,0,1);
		}
		
		//	execute each
	}
	
	//	end pass
	if ( InsidePass )
	{
		sg_end_pass();
		InsidePass = false;
	}

	//	commit
	sg_commit();
	
	//	save last
	mLastFrame = RenderCommands;
	
	//	we want to resolve the promise NOW, after rendering has been submitted
	//	but we may want here, some callback to block or glFinish or something before resolving
	//	but really we should be using something like glsync?
	if ( RenderCommands.mPromiseRef != std::numeric_limits<size_t>::max() )
	{
		auto Resolve = [](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
		{
			Promise.Resolve(LocalContext,0);
		};
		mPendingFrameRefs.Flush(RenderCommands.mPromiseRef,Resolve);
	}
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

	size_t PromiseRef;
	auto Promise = mPendingFrameRefs.CreatePromise( Params.mLocalContext, __PRETTY_FUNCTION__, PromiseRef );
	Params.Return(Promise);
	
	
	//	parse command
	try
	{
		//	gr: we NEED this to match the promise...
		auto CommandsArray = Params.GetArgumentArray(0);
		auto Commands = Sokol::ParseRenderCommands(Params.mLocalContext,CommandsArray);
		
		Commands.mPromiseRef = PromiseRef;
		std::lock_guard<std::mutex> Lock(mPendingFramesLock);
		mPendingFrames.PushBack(Commands);
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

