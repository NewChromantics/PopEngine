#include "TApiSokol.h"

#include "PopMain.h"
#include "TApiGui.h"
#include "SoyWindow.h"
#include "TApiCommon.h"

#include "sokol/sokol_gfx.h"


namespace Sokol
{
	void			IsOkay(sg_resource_state State,const char* Context);
	sg_uniform_type	GetUniformType(const std::string& TypeName);
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
	sg_reset_state_cache();

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
	//	currently we're just flushing out all pipelines after we render
	Array<sg_pipeline> TempPipelines;
	Array<sg_buffer> TempBuffers;

	
	sg_reset_state_cache();
	
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
		if ( NextCommand->GetName() == Sokol::TRenderCommand_Draw::Name )
		{
			auto& DrawCommand = dynamic_cast<Sokol::TRenderCommand_Draw&>( *NextCommand );
			auto& Geometry = mGeometrys[DrawCommand.mGeometryHandle];
			auto& Shader = mShaders[DrawCommand.mShaderHandle];
			
			//	this is where we might bufferup/batch commands
			sg_pipeline_desc PipelineDescription = {0};
			PipelineDescription.layout = Geometry.mVertexLayout;
			
			PipelineDescription.shader = Shader;
			PipelineDescription.primitive_type = Geometry.GetPrimitiveType();
			PipelineDescription.index_type = Geometry.GetIndexType();
			//	state stuff
			//PipelineDescription.depth_stencil
			//PipelineDescription.blend
			//PipelineDescription.rasterizer
			PipelineDescription.rasterizer.cull_mode = SG_CULLMODE_NONE;
			PipelineDescription.blend.enabled = false;
			
			sg_pipeline Pipeline = sg_make_pipeline(&PipelineDescription);
			auto PipelineState = sg_query_pipeline_state(Pipeline);
			Sokol::IsOkay(PipelineState,"sg_make_pipeline");
			TempPipelines.PushBack(Pipeline);
			sg_apply_pipeline(Pipeline);
			
			sg_bindings Bindings = {0};
			Bindings.vertex_buffers[0] = Geometry.mVertexBuffer;
			Bindings.index_buffer = Geometry.mIndexBuffer;
			/*	bind image uniforms
			Bindings.vs_images[SG_MAX_SHADERSTAGE_IMAGES];
			Bindings.fs_images[SG_MAX_SHADERSTAGE_IMAGES];
			 */
			
			sg_apply_bindings(&Bindings);
			//sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, const void* data, int num_bytes);
			auto VertexCount = Geometry.GetDrawVertexCount();
			auto VertexFirst = Geometry.GetDrawVertexFirst();
			auto InstanceCount = Geometry.GetDrawInstanceCount();
			sg_draw(VertexFirst,VertexCount,InstanceCount);
		}
	}
	
	//	end pass
	if ( InsidePass )
	{
		sg_end_pass();
		InsidePass = false;
	}

	//	commit
	sg_commit();

	//	cleanup resources only used on the frame
	for ( auto p=0;	p<TempPipelines.GetSize();	p++ )
	{
		auto Pipeline = TempPipelines[p];
		sg_destroy_pipeline(Pipeline);
	}
	for ( auto p=0;	p<TempBuffers.GetSize();	p++ )
	{
		auto Buffer = TempBuffers[p];
		sg_destroy_buffer(Buffer);
	}

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
	
	if ( Name == TRenderCommand_Draw::Name )
	{
		auto pDraw = std::make_shared<TRenderCommand_Draw>();
		pDraw->mGeometryHandle = Params.GetArgumentInt(1);
		pDraw->mShaderHandle = Params.GetArgumentInt(2);
		return pDraw;
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

sg_uniform_type Sokol::GetUniformType(const std::string& TypeName)
{
	if ( TypeName == "float" )	return SG_UNIFORMTYPE_FLOAT;
	if ( TypeName == "float2" )	return SG_UNIFORMTYPE_FLOAT2;
	if ( TypeName == "vec2" )	return SG_UNIFORMTYPE_FLOAT2;
	if ( TypeName == "float3" )	return SG_UNIFORMTYPE_FLOAT3;
	if ( TypeName == "vec3" )	return SG_UNIFORMTYPE_FLOAT3;
	if ( TypeName == "float4" )	return SG_UNIFORMTYPE_FLOAT4;
	if ( TypeName == "vec4" )	return SG_UNIFORMTYPE_FLOAT4;
	if ( TypeName == "float4x4" )	return SG_UNIFORMTYPE_MAT4;
	if ( TypeName == "mat4" )	return SG_UNIFORMTYPE_MAT4;
	throw Soy::AssertException(std::string("Unknown uniform type ") + TypeName);
}

void ApiSokol::TSokolContextWrapper::CreateShader(Bind::TCallback& Params)
{
	//	parse js call first
	Sokol::TCreateShader NewShader;
	NewShader.mVertSource = Params.GetArgumentString(0);
	NewShader.mFragSource = Params.GetArgumentString(1);

	//	arg3 contains uniform descriptions as sokol doesn't automatically resolve these!
	//	we'll try and remove this
	if ( !Params.IsArgumentUndefined(2) )
	{
		Array<Bind::TObject> UniformDescriptions;
		Params.GetArgumentArray(2, GetArrayBridge(UniformDescriptions));
		for ( auto u=0;	u<UniformDescriptions.GetSize();	u++ )
		{
			auto& UniformDescription = UniformDescriptions[u];
			Sokol::TCreateShader::TUniform Uniform;
			Uniform.mName = UniformDescription.GetString("Name");
			auto TypeName = UniformDescription.GetString("Type");
			Uniform.mType = Sokol::GetUniformType(TypeName);
			if ( UniformDescription.HasMember("ArraySize") )
				Uniform.mArraySize = UniformDescription.GetInt("ArraySize");
			NewShader.mUniforms.PushBack(Uniform);
		}
	}
	if ( !Params.IsArgumentUndefined(3) )
	{
		Params.GetArgumentArray(3, GetArrayBridge(NewShader.mAttributes));
	}
	
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
				
				
				for ( auto a=0;	a<NewShader.mAttributes.GetSize();	a++ )
				{
					auto* Name = NewShader.mAttributes[a].c_str();
					Description.attrs[a].name = Name;
				}

				auto UniformBlock = NewShader.GetUniformBlockDescription();
				Description.fs.uniform_blocks[0] = UniformBlock;
				Description.vs.uniform_blocks[0] = UniformBlock;
				/*
				Description.vs.uniform_blocks[0].size = 3*sizeof(float);
				Description.fs.uniform_blocks[0].uniforms[0].name = "Colour";
				Description.fs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT3;
				Description.fs.uniform_blocks[0].uniforms[0].array_count = 0;
				*/

				//	gr: when this errors, we lose the error. need to capture via sokol log
				//	gr: when this errors, it leaves a glGetError I think, need to work out how to flush/reset?
				//	gr: now its not crashing after resetting ipad
				sg_shader Shader = sg_make_shader(&Description);
				sg_reset_state_cache();
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
				{
					std::lock_guard<std::mutex> Lock(mPendingShadersLock);
					mShaders[ShaderHandle] = Shader;
				}
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
		mSokolContext->Queue(Create);
	}
	catch(std::exception& e)
	{
		Promise.Reject(Params.mLocalContext, e.what());
	}
}

void ParseGeometryObject(Sokol::TCreateGeometry& Geometry,Bind::TObject& VertexAttributesObject)
{
	/*
	 // a vertex buffer
	float vertices[] = {
		// positions            colors
		-0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
		0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
		0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
		-0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,
	};
	state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
		.size = sizeof(vertices),
		.content = vertices,
		.label = "quad-vertices"
	});
	
	// an index buffer with 2 triangles
	uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
	state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
		.type = SG_BUFFERTYPE_INDEXBUFFER,
		.size = sizeof(indices),
		.content = indices,
		.label = "quad-indices"
	});
	*/
	//	each key is the name of an attribute
	Array<std::string> AttribNames;
	VertexAttributesObject.GetMemberNames(GetArrayBridge(AttribNames));
	
	auto GetFloatFormat = [](int ElementSize)
	{
		switch (ElementSize)
		{
			case 1:	return SG_VERTEXFORMAT_FLOAT;
			case 2:	return SG_VERTEXFORMAT_FLOAT2;
			case 3:	return SG_VERTEXFORMAT_FLOAT3;
			case 4:	return SG_VERTEXFORMAT_FLOAT4;
		}
		throw Soy::AssertException(std::string("Invalid float format with element size ") + std::to_string(ElementSize) );
	};
	
	for ( auto a=0;	a<AttribNames.GetSize();	a++ )
	{
		auto& AttribName = AttribNames[a];
		auto AttribObject = VertexAttributesObject.GetObject(AttribName);
		Array<float> Dataf;
		AttribObject.GetArray("Data",GetArrayBridge(Dataf));
		auto ElementSize = AttribObject.GetInt("Size");
		auto VertexCount = Dataf.GetSize() / ElementSize;
		if ( a != 0 )
		{
			if ( VertexCount != Geometry.mVertexCount )
				throw Soy::AssertException("Attribute vertex count mis match to previous vertexcount");
		}
		Geometry.mVertexCount = VertexCount;

		auto DataStart = Geometry.mBufferData.GetDataSize();
		//	todo; support non-float and store dumb bytes, but this seems to corrupt data atm
		//auto Data8 = GetArrayBridge(Dataf).GetSubArray<uint8_t>(0,Dataf.GetSize());
		//Geometry.mBufferData.PushBackArray(Data8);
		Geometry.mBufferData.PushBackArray(Dataf);
		
		//	currently float only
		auto Format = GetFloatFormat(ElementSize);

		Geometry.mVertexLayout.attrs[a].format = Format;
		Geometry.mVertexLayout.attrs[a].buffer_index = 0;	//	should be changed on-binding

		//	0 stride and 0 offset means interleaved data
		//	an offset here is the offset into the buffer for the start if this attribute (for vertexAttribPointer)
		Geometry.mVertexLayout.attrs[a].offset = DataStart;
		//Geometry.mVertexLayout.attrs[a].stride = sizeof(float)*ElementSize;
	}
}


sg_buffer_desc Sokol::TCreateGeometry::GetVertexDescription() const
{
	sg_buffer_desc Description = {0};
	Description.size = mBufferData.GetDataSize();
	Description.content = mBufferData.GetArray();
	Description.type = SG_BUFFERTYPE_VERTEXBUFFER;
	Description.usage = SG_USAGE_IMMUTABLE;
	return Description;
}


/* return the byte size of a shader uniform */
int _sg_uniform_size(sg_uniform_type type, int count) {
	switch (type) {
		case SG_UNIFORMTYPE_INVALID:    return 0;
		case SG_UNIFORMTYPE_FLOAT:      return 4 * count;
		case SG_UNIFORMTYPE_FLOAT2:     return 8 * count;
		case SG_UNIFORMTYPE_FLOAT3:     return 12 * count; /* FIXME: std140??? */
		case SG_UNIFORMTYPE_FLOAT4:     return 16 * count;
		case SG_UNIFORMTYPE_MAT4:       return 64 * count;
		default:
			throw Soy::AssertException("_sg_uniform_size unhandled type");
			//SOKOL_UNREACHABLE;
			return -1;
	}
}

size_t Sokol::TCreateShader::TUniform::GetDataSize() const
{
	return _sg_uniform_size(mType,mArraySize);
	//auto ElementSize = GetTypeDataSize(mType);
	//ElementSize *= mArraySize;
	//return ElementSize;
}

sg_shader_uniform_block_desc Sokol::TCreateShader::GetUniformBlockDescription() const
{
	//	once this is called, this cannot move because the description is using string pointers!
	sg_shader_uniform_block_desc Description = {0};
	Description.size = 0;
	for ( auto u=0;	u<mUniforms.GetSize();	u++ )
	{
		auto& Uniform = mUniforms[u];
		//	gr: does this need to be 1 for non-arrays?
		Description.uniforms[u].array_count = Uniform.mArraySize;
		Description.uniforms[u].name = Uniform.mName.c_str();
		Description.uniforms[u].type = Uniform.mType;
		Description.size += Uniform.GetDataSize();
	}
	return Description;
}

void ApiSokol::TSokolContextWrapper::CreateGeometry(Bind::TCallback& Params)
{
	//	parse js call first
	Sokol::TCreateGeometry NewGeometry;
	if ( !Params.IsArgumentUndefined(1) )
		Params.GetArgumentArray(1,GetArrayBridge(NewGeometry.mTriangleIndexes));
	auto VertexAttributes = Params.GetArgumentObject(0);
	ParseGeometryObject( NewGeometry, VertexAttributes );
	
	//	now make a promise and construct
	auto Promise = mPendingGeometryPromises.CreatePromise( Params.mLocalContext, __PRETTY_FUNCTION__, NewGeometry.mPromiseRef );
	Params.Return(Promise);
	
	//	create the object (should be async)
	try
	{
		{
			std::lock_guard<std::mutex> Lock(mPendingGeometrysLock);
			mPendingGeometrys.PushBack(NewGeometry);
		}
		
		//	now create it
		auto PromiseRef = NewGeometry.mPromiseRef;
		auto Create = [this,PromiseRef,NewGeometry](sg_context Context) mutable
		{
			try
			{
				sg_activate_context(Context);	//	should be in higher-up code
				sg_reset_state_cache();			//	seems to stop fatal error when shader fails
				
				auto VertexBufferDescription = NewGeometry.GetVertexDescription();
				sg_buffer VertexBuffer = sg_make_buffer(&VertexBufferDescription);
				
				sg_resource_state State = sg_query_buffer_state(VertexBuffer);
				Sokol::IsOkay(State,"sg_make_buffer/sg_query_buffer_state (vertex)");
				
				NewGeometry.mVertexBuffer = VertexBuffer;
				
				//	need a more unique id maybe?
				auto GeometryHandle = NewGeometry.mVertexBuffer.id;
				{
					std::lock_guard<std::mutex> Lock(mPendingGeometrysLock);
					mGeometrys[GeometryHandle] = NewGeometry;
				}
				auto Resolve = [&](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
				{
					Promise.Resolve(LocalContext,GeometryHandle);
				};
				this->mPendingGeometryPromises.Flush(PromiseRef,Resolve);
			}
			catch(std::exception& e)
			{
				auto Reject = [&](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
				{
					Promise.Reject(LocalContext,e.what());
				};
				this->mPendingGeometryPromises.Flush(PromiseRef,Reject);
			}
		};
		mSokolContext->Queue(Create);
	}
	catch(std::exception& e)
	{
		Promise.Reject(Params.mLocalContext, e.what());
	}
}
