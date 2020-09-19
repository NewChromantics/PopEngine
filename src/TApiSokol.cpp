#include "TApiSokol.h"

#include "PopMain.h"
#include "TApiGui.h"
#include "SoyWindow.h"
#include "TApiCommon.h"

#include "sokol/sokol_gfx.h"


namespace Sokol
{
	void	IsOkay(sg_resource_state State,const char* Context);
}

namespace ApiSokol
{
	const char Namespace[] = "Pop.Sokol";

	DEFINE_BIND_TYPENAME(Context);
	DEFINE_BIND_FUNCTIONNAME(Render);
	DEFINE_BIND_FUNCTIONNAME(CreateShader);
	DEFINE_BIND_FUNCTIONNAME(CreateGeometry);
}

void ApiSokol::Bind(Bind::TContext &Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TSokolContextWrapper>(Namespace);
}

void ApiSokol::TSokolContextWrapper::CreateTemplate(Bind::TTemplate &Template)
{
	Template.BindFunction<BindFunction::Render>(&TSokolContextWrapper::Render);
	Template.BindFunction<BindFunction::CreateShader>(&TSokolContextWrapper::CreateShader);
	Template.BindFunction<BindFunction::CreateGeometry>(&TSokolContextWrapper::CreateGeometry);
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
		mPendingFramePromises.Flush(RenderCommands.mPromiseRef,Resolve);
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
	auto Promise = mPendingFramePromises.CreatePromise( Params.mLocalContext, __PRETTY_FUNCTION__, PromiseRef );
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

void Sokol::IsOkay(sg_resource_state State,const char* Context)
{
	if ( State == SG_RESOURCESTATE_VALID )
		return;
	
	std::stringstream Error;
	Error << "Shader state " << magic_enum::enum_name(State) << " in " << Context;
	throw Soy::AssertException(Error);
}

void ApiSokol::TSokolContextWrapper::CreateShader(Bind::TCallback& Params)
{
	//	parse js call first
	Sokol::TCreateShader NewShader;
	NewShader.mVertSource = Params.GetArgumentString(0);
	NewShader.mFragSource = Params.GetArgumentString(1);

	//	now make a promise and construct
	auto Promise = mPendingShaderPromises.CreatePromise( Params.mLocalContext, __PRETTY_FUNCTION__, NewShader.mPromiseRef );
	Params.Return(Promise);
	
	//	create the object (should be async)
	try
	{
		{
			std::lock_guard<std::mutex> Lock(mPendingShadersLock);
			mPendingShaders.PushBack(NewShader);
		}
		
		//	now create it
		auto PromiseRef = NewShader.mPromiseRef;
		auto Create = [this,PromiseRef,NewShader](sg_context Context)
		{
			try
			{
				sg_activate_context(Context);	//	should be in higher-up code
				sg_reset_state_cache();			//	seems to stop fatal error when shader fails
				sg_shader_desc Description = {};// _sg_shader_desc_defaults();
				Description.vs.source = NewShader.mVertSource.c_str();
				Description.fs.source = NewShader.mFragSource.c_str();
				//	gr: when this errors, we lose the error. need to capture via sokol log
				//	gr: when this errors, it leaves a glGetError I think, need to work out how to flush/reset?
				//	gr: now its not crashing after resetting ipad
				sg_shader Shader = sg_make_shader(&Description);
				
				sg_resource_state State = sg_query_shader_state(Shader);
				Sokol::IsOkay(State,"make_shader/sg_query_shader_state");
				
				/*
				sg_shader bg_shd = sg_make_shader(&(sg_shader_desc){
					.vs.source =
					"#version 330\n"
					"layout(location=0) in vec2 position;\n"
					"void main() {\n"
					"  gl_Position = vec4(position, 0.5, 1.0);\n"
					"}\n",
					.fs = {
						.uniform_blocks[0] = {
							.size = sizeof(fs_params_t),
							.uniforms = {
								[0] = { .name="tick", .type=SG_UNIFORMTYPE_FLOAT }
							}
						},
						.source =
						"#version 330\n"
						"uniform float tick;\n"
						"out vec4 frag_color;\n"
						"void main() {\n"
						"  vec2 xy = fract((gl_FragCoord.xy-vec2(tick)) / 50.0);\n"
						"  frag_color = vec4(vec3(xy.x*xy.y), 1.0);\n"
						"}\n"
					}
				});
				 
				sg_shader_desc desc = {
					.attrs = {
						[0] = { .name="position" },
						[1] = { .name="color1" }
					}
				};
				typedef struct sg_shader_desc {
					uint32_t _start_canary;
					sg_shader_attr_desc attrs[SG_MAX_VERTEX_ATTRIBUTES];
					sg_shader_stage_desc vs;
					sg_shader_stage_desc fs;
					const char* label;
					uint32_t _end_canary;
				} sg_shader_desc;
				 */
				auto ShaderHandle = Shader.id;
				mShaders[ShaderHandle] = Shader;
				
				auto Resolve = [&](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
				{
					Promise.Resolve(LocalContext,ShaderHandle);
				};
				this->mPendingShaderPromises.Flush(PromiseRef,Resolve);
			}
			catch(std::exception& e)
			{
				auto Reject = [&](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
				{
					Promise.Reject(LocalContext,e.what());
				};
				this->mPendingShaderPromises.Flush(PromiseRef,Reject);
			}
		};
		mSokolContext->Run(Create);
	}
	catch(std::exception& e)
	{
		Promise.Reject(Params.mLocalContext, e.what());
	}
}

void ApiSokol::TSokolContextWrapper::CreateGeometry(Bind::TCallback& Params)
{
	Soy_AssertTodo();
}

