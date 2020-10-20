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

	DEFINE_BIND_TYPENAME(Sokol_Context);
	DEFINE_BIND_FUNCTIONNAME(Render);
	DEFINE_BIND_FUNCTIONNAME(CreateShader);
	DEFINE_BIND_FUNCTIONNAME(CreateGeometry);
}

void ApiSokol::Bind(Bind::TContext &Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TSokolContextWrapper>(Namespace,"Context");
}

void ApiSokol::TSokolContextWrapper::CreateTemplate(Bind::TTemplate &Template)
{
	Template.BindFunction<BindFunction::Render>(&TSokolContextWrapper::Render);
	Template.BindFunction<BindFunction::CreateShader>(&TSokolContextWrapper::CreateShader);
	Template.BindFunction<BindFunction::CreateGeometry>(&TSokolContextWrapper::CreateGeometry);
}

sg_pixel_format GetPixelFormat(SoyPixelsFormat::Type Format)
{
	switch(Format)
	{
		case SoyPixelsFormat::Greyscale:	return SG_PIXELFORMAT_R8;
		case SoyPixelsFormat::Depth16mm:	return SG_PIXELFORMAT_R16;
		case SoyPixelsFormat::GreyscaleAlpha:	return SG_PIXELFORMAT_RG8;
	
		case SoyPixelsFormat::DepthFloatMetres:	return SG_PIXELFORMAT_R32F;
		case SoyPixelsFormat::Float1:		return SG_PIXELFORMAT_R32F;
		case SoyPixelsFormat::RGBA:			return SG_PIXELFORMAT_RGBA8;
		case SoyPixelsFormat::BGRA:			return SG_PIXELFORMAT_BGRA8;
		case SoyPixelsFormat::Float4:		return SG_PIXELFORMAT_RGBA32F;
	}
	
	std::stringstream Error;
	Error << "No sokol pixel format for " << Format;
	throw Soy::AssertException(Error);
}


sg_image_desc GetImageDescription(SoyImageProxy& Image,SoyPixels& TemporaryPixels)
{
	sg_image_desc Description = {0};
	
	//	gr: special case
	//	a bit unsafe! we need to ensure the return isn't held outside stack scope
	auto& ImagePixels = Image.GetPixels();
	auto* pPixels = &ImagePixels;
	if ( ImagePixels.GetFormat() == SoyPixelsFormat::RGB )
	{
		Soy::TScopeTimerPrint Timer("Converting RGB image to temporary RGBA for sokol",1);
		TemporaryPixels.Copy(ImagePixels);
		TemporaryPixels.SetFormat(SoyPixelsFormat::RGBA);
		pPixels = &TemporaryPixels;
	}
	if ( ImagePixels.GetFormat() == SoyPixelsFormat::Yuv_8_88 || ImagePixels.GetFormat() == SoyPixelsFormat::Yuv_8_8_8 )
	{
		Soy::TScopeTimerPrint Timer("Converting yuv image to temporary greyscale for sokol",1);
		TemporaryPixels.Copy(ImagePixels);
		TemporaryPixels.SetFormat(SoyPixelsFormat::Greyscale);
		pPixels = &TemporaryPixels;
	}

	auto& Pixels = *pPixels;
	auto ImageMeta = Pixels.GetMeta();
	Description.width = ImageMeta.GetWidth();
	Description.height = ImageMeta.GetHeight();
	//Description.render_target = true;
	Description.pixel_format = GetPixelFormat( ImageMeta.GetFormat() );
	
	
	auto& PixelsArray = Pixels.GetPixelsArray();
	auto CubeFace = 0;
	auto Mip = 0;
	sg_subimage_content& SubImage = Description.content.subimage[CubeFace][Mip];
	SubImage.ptr = PixelsArray.GetArray();
	SubImage.size = PixelsArray.GetDataSize();
	
	return Description;
}

void ApiSokol::TSokolContextWrapper::QueueImageDelete(sg_image Image)
{
	std::lock_guard<std::mutex> Lock(mPendingDeleteImagesLock);
	mPendingDeleteImages.PushBack(Image);
}

void ApiSokol::TSokolContextWrapper::FreeImageDeletes()
{
	std::lock_guard<std::mutex> Lock(mPendingDeleteImagesLock);
	for ( auto i=0;	i<mPendingDeleteImages.GetSize();	i++ )
	{
		auto& Image = mPendingDeleteImages[i];
		sg_destroy_image(Image);
	}
	mPendingDeleteImages.Clear();
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
	
	//	jobs
	FreeImageDeletes();
	
	// cache render target pass if 0 render to screen
	sg_pass RenderTargetPass = {0};

	//	run render commands
	auto NewPass = [&](float r,float g,float b,float a)
	{
		if ( InsidePass )
		{
			sg_end_pass();
			InsidePass = false;
		}
		
		sg_pass_action PassAction = {0};
		PassAction.colors[0].action = SG_ACTION_CLEAR;
		PassAction.colors[0].val[0] = r;
		PassAction.colors[0].val[1] = g;
		PassAction.colors[0].val[2] = b;
		PassAction.colors[0].val[3] = a;
		
		// This needs to change to sg_begin_pass if a RenderTarget is set
		if(RenderTargetPass.id != 0)
			sg_begin_pass(RenderTargetPass, &PassAction);
		else
			sg_begin_default_pass( &PassAction, ViewRect.x, ViewRect.y );
	
		auto test = sg_query_desc();
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
		
		//	execute each command
		if ( NextCommand->GetName() == Sokol::TRenderCommand_UpdateImage::Name )
		{
			auto& UpdateImageCommand = dynamic_cast<Sokol::TRenderCommand_UpdateImage&>( *NextCommand );
			if ( !UpdateImageCommand.mImage )
				throw Soy::AssertException("UpdateImage command with null image pointer");

			auto& ImageSoy = *UpdateImageCommand.mImage;
			SoyPixels TemporaryImage;
			
			//	if image has no sg_image, create it
			if ( !ImageSoy.HasSokolImage() )
			{
				auto ImageDescription = GetImageDescription(ImageSoy,TemporaryImage);
				auto NewImage = sg_make_image(&ImageDescription);
				auto State = sg_query_image_state(NewImage);
				Sokol::IsOkay(State,"sg_make_image");
				
				//	gr: guessing this isn't threadsafe
				auto FreeSokolImage = [=]()
				{
					QueueImageDelete(NewImage);
				};
				
				ImageSoy.SetSokolImage( NewImage.id, FreeSokolImage );
				ImageSoy.OnSokolImageUpdated();
			}
			
			bool LatestVersion = false;
			sg_image ImageSokol = {0};
			ImageSokol.id = ImageSoy.GetSokolImage(LatestVersion);
			
			//	if image sokol version is out of date, update texture
			if ( !LatestVersion )
			{
				auto ImageDescription = GetImageDescription(ImageSoy,TemporaryImage);
				sg_update_image( ImageSokol, ImageDescription.content );
				auto State = sg_query_image_state(ImageSokol);
				Sokol::IsOkay(State,"sg_make_image");
				ImageSoy.OnSokolImageUpdated();
			}
		}
		
		if ( NextCommand->GetName() == Sokol::TRenderCommand_Draw::Name )
		{
			auto& DrawCommand = dynamic_cast<Sokol::TRenderCommand_Draw&>( *NextCommand );
			auto& Geometry = mGeometrys[DrawCommand.mGeometryHandle];
			auto& Shader = mShaders[DrawCommand.mShaderHandle];
			
			//	this is where we might bufferup/batch commands
			sg_pipeline_desc PipelineDescription = {0};
			PipelineDescription.layout = Geometry.mVertexLayout;
			
			PipelineDescription.shader = Shader.mShader;
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
			for ( auto a=0;	a<Geometry.GetVertexLayoutBufferSlots();	a++ )
				Bindings.vertex_buffers[a] = Geometry.mVertexBuffer;
			Bindings.index_buffer = Geometry.mIndexBuffer;
			
			for ( auto i=0;	i<SG_MAX_SHADERSTAGE_IMAGES;	i++ )
			{
				//	gr: detect missing slots
				auto ImageSoy = DrawCommand.mImageUniforms[i];
				sg_image ImageSokol = {0};
				if ( ImageSoy )
				{
					bool LatestVersion = false;
					ImageSokol.id = ImageSoy->GetSokolImage(LatestVersion);
					if ( !LatestVersion )
						std::Debug << "Warning, using sokol image as uniform but out of date" << std::endl;
				}
				//sg_query_resource_texture(ImageSokol);
				Bindings.vs_images[i] = ImageSokol;
				Bindings.fs_images[i] = ImageSokol;
			}
			
			sg_apply_bindings(&Bindings);
			sg_apply_uniforms( SG_SHADERSTAGE_VS, 0, DrawCommand.mUniformBlock.GetArray(), DrawCommand.mUniformBlock.GetDataSize() );
			sg_apply_uniforms( SG_SHADERSTAGE_FS, 0, DrawCommand.mUniformBlock.GetArray(), DrawCommand.mUniformBlock.GetDataSize() );
			auto VertexCount = Geometry.GetDrawVertexCount();
			auto VertexFirst = Geometry.GetDrawVertexFirst();
			auto InstanceCount = Geometry.GetDrawInstanceCount();
			sg_draw(VertexFirst,VertexCount,InstanceCount);
		}
	
		if ( NextCommand->GetName() == Sokol::TRenderCommand_SetRenderTarget::Name )
		{
			auto& SetRenderTargetCommand = dynamic_cast<Sokol::TRenderCommand_SetRenderTarget&>( *NextCommand );
			if ( !SetRenderTargetCommand.mTargetTexture ) // js land = Commands.push( [ "SetRenderTarget", null ] )
				RenderTargetPass = {0};
			else
			{
			}
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
		auto Commands = ParseRenderCommands(Params.mLocalContext,CommandsArray);
		
		Commands.mPromiseRef = PromiseRef;
		std::lock_guard<std::mutex> Lock(mPendingFramesLock);
		mPendingFrames.PushBack(Commands);
	}
	catch(std::exception& e)
	{
		Promise.Reject(Params.mLocalContext, e.what());
	}
}

size_t GetFloatCount(sg_uniform_type Type)
{
	switch ( Type )
	{
		case SG_UNIFORMTYPE_FLOAT:	return 1;
		case SG_UNIFORMTYPE_FLOAT2:	return 2;
		case SG_UNIFORMTYPE_FLOAT3: return 3;
		case SG_UNIFORMTYPE_FLOAT4:	return 4;
		case SG_UNIFORMTYPE_MAT4:	return 4*4;
	}
	throw Soy::AssertException("Not a float type");
}

void Sokol::TRenderCommand_Draw::ParseUniforms(Bind::TObject& UniformsObject,Sokol::TShader& Shader)
{
	//	gr; for optimisation, maybe allow a block of bytes!
	
	Array<std::string> Keys;
	UniformsObject.GetMemberNames( GetArrayBridge(Keys) );
	
	//	need to alloc data
	mUniformBlock.SetSize(Shader.mShaderMeta.GetUniformBlockSize());
	mUniformBlock.SetAll(0);
	
	
	auto WriteUniform = [&](const TCreateShader::TUniform& Uniform,size_t UniformDataPosition)
	{
		//	gr: move this into its own func
		switch(Uniform.mType)
		{
			//	our js code should pull arrays of whatever into floats,
			//	may need to check it doesnt go wrong on strings, objects
			case SG_UNIFORMTYPE_FLOAT:
			case SG_UNIFORMTYPE_FLOAT2:
			case SG_UNIFORMTYPE_FLOAT3:
			case SG_UNIFORMTYPE_FLOAT4:
			case SG_UNIFORMTYPE_MAT4:
			{
				auto UniformFloatCount = GetFloatCount(Uniform.mType) * Uniform.mArraySize;
				
				//	get data we're going to write to
				auto UniformData = GetArrayBridge(mUniformBlock).GetSubArray(UniformDataPosition,Uniform.GetDataSize());
				auto UniformDataf = GetArrayBridge(UniformData).GetSubArray<float>(0,UniformFloatCount);
				
				Array<float> Floats;
				//	if !array .GetFloat()
				UniformsObject.GetArray(Uniform.mName,GetArrayBridge(Floats));
				
				//	resize to match target
				Floats.SetSize(UniformFloatCount);
				
				//	copy
				if ( Floats.GetDataSize() != Uniform.GetDataSize() )
				{
					std::stringstream Error;
					Error << "Uniform (" << magic_enum::enum_name(Uniform.mType) << ") floats x" << Floats.GetSize() << " (data size " << Floats.GetDataSize() << ") doesn't match uniform block size " << Uniform.GetDataSize();
					throw Soy::AssertException(Error);
				}
				UniformDataf.Copy( Floats );
				break;
			}
			
			default:
			{
				std::stringstream Error;
				Error << __PRETTY_FUNCTION__ << " Unhandled js uniform->uniform type " << magic_enum::enum_name(Uniform.mType);
				throw Soy::AssertException(Error);
			}
		}
	};
	
	auto WriteImageUniform = [&](const TCreateShader::TImageUniform& Uniform,size_t UniformImageIndex)
	{
		//	here we don't write to the block, but we need to work out what image is going
		//	to go in the image slot
		auto ImageObject = UniformsObject.GetObject( Uniform.mName );
		auto& ImageWrapper = ImageObject.This<TImageWrapper>();
		auto ImageSoy = ImageWrapper.mImage;
		//	todo: add a "update image pixels" render command before draw
		mImageUniforms[UniformImageIndex] = ImageSoy;
	};
	
	for ( auto k=0;	k<Keys.GetSize();	k++ )
	{
		//	get slot (may be optimised out or not exist)
		auto& UniformName = Keys[k];
		size_t UniformDataPosition;
		auto* pUniform = Shader.mShaderMeta.GetUniform( UniformName, UniformDataPosition );
		if ( pUniform )
		{
			WriteUniform( *pUniform, UniformDataPosition );
			continue;
		}

		size_t UniformImageIndex;
		auto* pImageUniform = Shader.mShaderMeta.GetImageUniform( UniformName, UniformImageIndex );
		if ( pImageUniform )
		{
			WriteImageUniform( *pImageUniform, UniformImageIndex );
			continue;
		}

		std::Debug << "No uniform/image named " << UniformName << std::endl;
	}
	
	//	did we write to all memory?
}


void Sokol::ParseRenderCommand(std::function<void(std::shared_ptr<Sokol::TRenderCommandBase>)> PushCommand,const std::string_view& Name,Bind::TCallback& Params,std::function<Sokol::TShader&(uint32_t)>& GetShader)
{
	if ( Name == TRenderCommand_Clear::Name )
	{
		auto pClear = std::make_shared<TRenderCommand_Clear>();
		//	for speed, would a typed array be faster, then we can memcpy?
		pClear->mColour[0] = Params.GetArgumentFloat(1);
		pClear->mColour[1] = Params.GetArgumentFloat(2);
		pClear->mColour[2]= Params.GetArgumentFloat(3);
		pClear->mColour[3] = Params.IsArgumentUndefined(4) ? 1 : Params.GetArgumentFloat(4);
		PushCommand(pClear);
		return;
	}
	
	if ( Name == TRenderCommand_Draw::Name )
	{
		auto pDraw = std::make_shared<TRenderCommand_Draw>();
		//	these objects should exist at this point
		pDraw->mGeometryHandle = Params.GetArgumentInt(1);
		pDraw->mShaderHandle = Params.GetArgumentInt(2);

		auto& Shader = GetShader(pDraw->mShaderHandle);
		
		//	parse uniforms
		if ( !Params.IsArgumentUndefined(3) )
		{
			auto Uniforms = Params.GetArgumentObject(3);
			pDraw->ParseUniforms(Uniforms,Shader);
		}
		
		//	make a update-image command for every image we use
		for ( auto& [ImageUniformIndex,pImageWrapper] : pDraw->mImageUniforms )
		{
			auto pUpdateImage = std::make_shared<TRenderCommand_UpdateImage>();
			pUpdateImage->mImage = pImageWrapper;
			PushCommand(pUpdateImage);
		}
		
		PushCommand(pDraw);
		return;
	}

	if (Name == TRenderCommand_SetRenderTarget::Name)
	{
		auto pSetRenderTarget = std::make_shared<TRenderCommand_SetRenderTarget>();
		
		//	first arg is image, or null to render to screen
		if (Params.IsArgumentNull(1))
		{
			//	must not have readback format
			if (!Params.IsArgumentUndefined(2))
			{
				std::stringstream Error;
				Error << "Render Command " << Name << " has read-backformat specified, but with null target, should not be provided";
				throw Soy::AssertException(Error);
			}
		}
		else
		{
			auto ImageObject = Params.GetArgumentObject(1);
			auto Image = ImageObject.This<TImageWrapper>();
			pSetRenderTarget->mTargetTexture = Image.mImage;

			//	get readback format
			if (!Params.IsArgumentUndefined(2))
			{
				//	gr: is readback better as a command?
				auto FormatString = Params.GetArgumentString(2);
				auto Format = SoyPixelsFormat::ToType(FormatString);
				pSetRenderTarget->mReadBackFormat = Format;
			}

			//	insert a update-image command, so that sg image will get created
			//	although, it doesnt need pixels updating as they will get overriden...
			auto pUpdateImage = std::make_shared<TRenderCommand_UpdateImage>();
			pUpdateImage->mImage = pSetRenderTarget->mTargetTexture;
			PushCommand(pUpdateImage);
		}

		PushCommand(pSetRenderTarget);
		return;
	}
	
	std::stringstream Error;
	Error << "Unknown render command " << Name;
	throw Soy::AssertException(Error);
}

Sokol::TRenderCommands ApiSokol::TSokolContextWrapper::ParseRenderCommands(Bind::TLocalContext& Context,Bind::TArray& CommandArrayArray)
{
	std::function<Sokol::TShader&(uint32_t)> GetShader = [this](uint32_t ShaderHandle) -> Sokol::TShader&
	{
		return mShaders[ShaderHandle];
	};
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
		auto PushCommand = [&](std::shared_ptr<Sokol::TRenderCommandBase> Command)
		{
			Commands.mCommands.PushBack(Command);
		};
		Sokol::ParseRenderCommand( PushCommand, CommandName, CommandAndParams, GetShader );
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
			auto TypeName = UniformDescription.GetString("Type");
			if ( TypeName == "sampler2D" )
			{
				Sokol::TCreateShader::TImageUniform Uniform;
				Uniform.mName = UniformDescription.GetString("Name");
				NewShader.mImageUniforms.PushBack(Uniform);
			}
			else
			{
				Sokol::TCreateShader::TUniform Uniform;
				Uniform.mName = UniformDescription.GetString("Name");
				Uniform.mType = Sokol::GetUniformType(TypeName);
				if ( UniformDescription.HasMember("ArraySize") )
					Uniform.mArraySize = UniformDescription.GetInt("ArraySize");
				NewShader.mUniforms.PushBack(Uniform);
			}
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
				Sokol::TShader Shader;
				Shader.mShaderMeta = NewShader;

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

				auto SetImageDescription = [&](const sg_shader_image_desc& ImageDescription,size_t Index)
				{
					Description.fs.images[Index] = ImageDescription;
					Description.vs.images[Index] = ImageDescription;
				};
				auto SetUniformBlockDescription = [&](const sg_shader_uniform_block_desc& UniformDescription,size_t Index)
				{
					Description.fs.uniform_blocks[Index] = UniformDescription;
					Description.vs.uniform_blocks[Index] = UniformDescription;
				};
				
				NewShader.EnumImageDescriptions(SetImageDescription);
				NewShader.EnumUniformBlockDescription(SetUniformBlockDescription);
				
				//	gr: when this errors, we lose the error. need to capture via sokol log
				//	gr: when this errors, it leaves a glGetError I think, need to work out how to flush/reset?
				//	gr: now its not crashing after resetting ipad
				Shader.mShader = sg_make_shader(&Description);
				sg_reset_state_cache();
				sg_resource_state State = sg_query_shader_state(Shader.mShader);
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
				auto ShaderHandle = Shader.mShader.id;
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

		//	gr: for non-interlaced buffer data, use multiple buffer slots
		//		with an offset (stride auto calculated), but just put the same buffer in the slots
		//	https://github.com/floooh/sokol/issues/382
		Geometry.mVertexLayout.attrs[a].format = Format;
		Geometry.mVertexLayout.attrs[a].buffer_index = a;
		Geometry.mVertexLayout.attrs[a].offset = size_cast<int>(DataStart);
	}
}

size_t Sokol::TCreateGeometry::GetVertexLayoutBufferSlots() const
{
	for ( auto a=0;	a<std::size(mVertexLayout.attrs);	a++ )
	{
		auto& Attr = mVertexLayout.attrs[a];
		if ( Attr.format == SG_VERTEXFORMAT_INVALID )
			return a;
	}
	//	all filled! (unlikely... take out this assert if it happens)
	//	plus, I think in the pipeline we can't have 16 buffers?
	throw Soy::AssertException("Unexpectedly filled all vertex layout buffer slots, remove this assert if genuine case is found");
	return std::size(mVertexLayout.attrs);
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

size_t Sokol::TCreateShader::GetUniformBlockSize() const
{
	//	once this is called, this cannot move because the description is using string pointers!
	auto Size = 0;
	for ( auto u=0;	u<mUniforms.GetSize();	u++ )
	{
		auto& Uniform = mUniforms[u];
		Size += Uniform.GetDataSize();
	}
	return Size;
}


void Sokol::TCreateShader::EnumUniformBlockDescription(std::function<void(const sg_shader_uniform_block_desc&,size_t)> OnDescription) const
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
		//	gr: we're not specifying the position of this data... is it okay in a random order?
		Description.size += Uniform.GetDataSize();
	}
	OnDescription(Description,0);
}


void Sokol::TCreateShader::EnumImageDescriptions(std::function<void(const sg_shader_image_desc&,size_t)> OnDescription) const
{
	//	once this is called, this cannot move because the description is using string pointers!
	for ( auto u=0;	u<mImageUniforms.GetSize();	u++ )
	{
		auto& Uniform = mImageUniforms[u];
		sg_shader_image_desc Description = {0};
		Description.name = Uniform.mName.c_str();
		Description.type = SG_IMAGETYPE_2D;
		OnDescription(Description,u);
	}
}


const Sokol::TCreateShader::TUniform* Sokol::TCreateShader::GetUniform(const std::string& Name,size_t& DataOffset)
{
	DataOffset = 0;
	for ( auto u=0;	u<mUniforms.GetSize();	u++ )
	{
		auto& Uniform = mUniforms[u];
		if ( Uniform.mName == Name )
		return &Uniform;
		DataOffset += Uniform.GetDataSize();
	}
	//	throw here so we can log it?
	return nullptr;
}

const Sokol::TCreateShader::TImageUniform* Sokol::TCreateShader::GetImageUniform(const std::string& Name,size_t& ImageIndex)
{
	for ( auto u=0;	u<mImageUniforms.GetSize();	u++ )
	{
		auto& Uniform = mImageUniforms[u];
		if ( Uniform.mName == Name )
		{
			ImageIndex = u;
			return &Uniform;
		}
	}
	//	throw here so we can log it?
	return nullptr;
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
