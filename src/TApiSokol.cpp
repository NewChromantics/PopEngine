#include "TApiSokol.h"

#include "PopMain.h"
#include "TApiGui.h"
#include "SoyWindow.h"
#include "TApiCommon.h"

#include "sokol/sokol_gfx.h"

#include "SoyOpengl.h"

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
	DEFINE_BIND_FUNCTIONNAME(GetScreenRect);
	DEFINE_BIND_FUNCTIONNAME(CanRenderToPixelFormat);
	DEFINE_BIND_FUNCTIONNAME(GetStats);
	
	int	Stats_ImageCounter = 0;
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
	Template.BindFunction<BindFunction::GetScreenRect>(&TSokolContextWrapper::GetScreenRect);
	Template.BindFunction<BindFunction::CanRenderToPixelFormat>(&TSokolContextWrapper::CanRenderToPixelFormat);
	Template.BindFunction<BindFunction::GetStats>(&TSokolContextWrapper::GetStats);
}

sg_pixel_format GetPixelFormat(SoyPixelsFormat::Type Format,bool ForRenderTarget)
{
	switch(Format)
	{
		case SoyPixelsFormat::Greyscale:	return SG_PIXELFORMAT_R8;
		case SoyPixelsFormat::Depth16mm:	return SG_PIXELFORMAT_R16;
		
		case SoyPixelsFormat::uyvy_8888:
		case SoyPixelsFormat::ChromaUV_88:		
		case SoyPixelsFormat::GreyscaleAlpha:	
			return SG_PIXELFORMAT_RG8;
	
		case SoyPixelsFormat::DepthHalfMetres:
		case SoyPixelsFormat::DepthDisparityHalf:
			return SG_PIXELFORMAT_R16F;
	
		case SoyPixelsFormat::DepthFloatMetres:
		case SoyPixelsFormat::Float1:
			return SG_PIXELFORMAT_R32F;
			
		case SoyPixelsFormat::RGBA:			return SG_PIXELFORMAT_RGBA8;
		case SoyPixelsFormat::BGRA:			return SG_PIXELFORMAT_BGRA8;
		
		case SoyPixelsFormat::Float4:		
		{
			//	gr: for IOS purposes, force half float on render target for float
			//	todo: only if float isn't supported
			if ( ForRenderTarget )
				return SG_PIXELFORMAT_RGBA16F;
				
			return SG_PIXELFORMAT_RGBA32F;
		}		
		
		default:break;
	}
	
	std::stringstream Error;
	Error << "No sokol pixel format for " << Format;
	throw Soy::AssertException(Error);
}


//	for formats that won't represent directly in rendering (eg. multiplane YUV)
//	this returns a striped/single plane set of pixels (usually greyscale)
SoyPixelsMeta GetSinglePlaneImageMeta(const SoyPixelsMeta& Meta)
{
	//	maybe a better approach by finding pixels array size vs w/h
	//	but this approach just "overflows" the luma plane
	//	(which means the following planes may have multiple side-by-side)
	if (Meta.GetFormat() == SoyPixelsFormat::Yuv_8_8_8 ||
		Meta.GetFormat() == SoyPixelsFormat::Yuv_8_88 )
	{
		auto DataSize = Meta.GetDataSize();
		auto Width = Meta.GetWidth();
		auto Height = DataSize / Width;
		auto Format = SoyPixelsFormat::Greyscale;
		return SoyPixelsMeta( Width, Height, Format );
	}
	
	return Meta;
}

//	for formats that won't represent directly in rendering (eg. multiplane YUV)
//	this returns a striped/single plane set of pixels (usually greyscale)
SoyPixelsRemote GetSinglePlanePixels(const SoyPixelsImpl& Pixels)
{
	//	maybe a better approach by finding pixels array size vs w/h
	//	but this approach just "overflows" the luma plane
	//	(which means the following planes may have multiple side-by-side)
	if (Pixels.GetFormat() == SoyPixelsFormat::Yuv_8_8_8 ||
		Pixels.GetFormat() == SoyPixelsFormat::Yuv_8_88 )
	{
		//	try and re-use GetSinglePlaneMeta and then this func should just check alignment
		auto* Data = Pixels.GetPixelsArray().GetArray();
		auto DataSize = Pixels.GetPixelsArray().GetDataSize();
		auto Width = Pixels.GetWidth();
		auto Height = DataSize / Width;
		auto Format = SoyPixelsFormat::Greyscale;
		return SoyPixelsRemote( const_cast<uint8_t*>(Data), Width, Height, DataSize, Format);
	}
	
	return SoyPixelsRemote(Pixels);
}

sg_image_desc GetImageDescription(SoyImageProxy& Image,SoyPixels& TemporaryPixels, bool RenderTarget,bool GetPixelData)
{
	sg_image_desc Description = {0};
	
	//	should probably pass readonly with rendertarget option
	Description.usage = SG_USAGE_STREAM;
	if ( RenderTarget )
	{
		Description.usage = SG_USAGE_IMMUTABLE;
		GetPixelData = false;
	}
	
	//	gr: special case
	//	a bit unsafe! we need to ensure the return isn't held outside stack scope
	auto& ImagePixels = Image.GetPixels();
	auto* pPixels = &ImagePixels;
	if ( ImagePixels.GetFormat() == SoyPixelsFormat::RGB )
	{
		std::string TimerName(Image.mName + " converting RGB image to temporary RGBA for sokol");
		Soy::TScopeTimerPrint Timer(TimerName.c_str(),1);
		TemporaryPixels.Copy(ImagePixels);
		TemporaryPixels.SetFormat(SoyPixelsFormat::RGBA);
		pPixels = &TemporaryPixels;
	}
	if ( ImagePixels.GetFormat() == SoyPixelsFormat::Yuv_8_88 || ImagePixels.GetFormat() == SoyPixelsFormat::Yuv_8_8_8 )
	{
		std::string TimerName(Image.mName + " converting yuv image to temporary greyscale for sokol");
		Soy::TScopeTimerPrint Timer(TimerName.c_str(),1);
		TemporaryPixels.Copy(ImagePixels);
		TemporaryPixels.SetFormat(SoyPixelsFormat::Greyscale);
		pPixels = &TemporaryPixels;
	}

	auto& Pixels = *pPixels;
	auto ImageMeta = Pixels.GetMeta();
	Description.width = ImageMeta.GetWidth();
	Description.height = ImageMeta.GetHeight();
	if ( RenderTarget )
	{
		auto SokolDescription = sg_query_desc();
		Description.render_target = true;
		Description.pixel_format = GetPixelFormat( ImageMeta.GetFormat(), RenderTarget );
		Description.sample_count = SokolDescription.context.sample_count;
		
		//	ignoring pixel content here
	}
	else
	{
		Description.pixel_format = GetPixelFormat( ImageMeta.GetFormat(), RenderTarget );

		//	gr: only set pixel data 
		//	if IncludeData (updating image)
		//	or newimage & immutable
		 
		if ( GetPixelData )
		{
			//	sokol was erroring as we intialised a rendertarget with pixels
			auto& PixelsArray = Pixels.GetPixelsArray();
			auto CubeFace = 0;
			auto Mip = 0;
			auto& SubImage = Description.data.subimage[CubeFace][Mip];
			SubImage.ptr = PixelsArray.GetArray();
			SubImage.size = PixelsArray.GetDataSize();
		}
	}
	
	
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
		Stats_ImageCounter--;
	}
	mPendingDeleteImages.Clear();
}

void ApiSokol::TSokolContextWrapper::OnPaint(sg_context Context,vec2x<size_t> ViewRect)
{
	//	ios(sokol) will fail to setup context if rect is 0x0
	if ( ViewRect.x == 0 || ViewRect.y == 0 )
		return;

	//	skip render if nothing pending
	{
		std::lock_guard<std::mutex> Lock(mPendingFramesLock);
		if ( mPendingFrames.IsEmpty() )
			return;
	}

	sg_activate_context(Context);

	mLastRect = ViewRect;

	//	jobs
	FreeImageDeletes();

	mPendingFramesLock.lock();
	auto RenderFrameList = mPendingFrames;
	mPendingFrames.Clear(true);
	mPendingFramesLock.unlock();

	for ( int i=0;	i<RenderFrameList.GetSize();	i++ )
	{
		auto& RenderCommands = RenderFrameList[i];
		RunRender( RenderCommands, ViewRect );
	}
	
	//	end of "current frame"
	sg_commit();
}

	
void ApiSokol::TSokolContextWrapper::RunRender(Sokol::TRenderCommands& RenderCommands,vec2x<size_t> ViewRect)
{
	sg_reset_state_cache();


	bool InsidePass = false;
	SoyImageProxy* PassRenderTargetImage = nullptr;	//	temp for the pipeline blend mode...
	SoyPixelsMeta RenderTargetPassMeta;		//	last description used for rendertexture pass
	std::string RenderError;			//	if this isnt empty, we reject the promise
	
	//	currently we're just flushing out all pipelines after we render
	Array<sg_pipeline> TempPipelines;
	Array<sg_buffer> TempBuffers;
	Array<sg_pass> TempPasses;

	auto FreePipelines = [&]()
	{
		for ( auto p=0;	p<TempPipelines.GetSize();	p++ )
		{
			auto Pipeline = TempPipelines[p];
			sg_destroy_pipeline(Pipeline);
		}
	};

	auto EndPass = [&]()
	{
		//	gr: implement this when we can sync with the last-read-pixels
		//		we might have a case where we read-back the pixels for this target
		//		TO this image, and calling this will make those pixels out of date
		//PassRenderTargetImage->OnSokolImageChanged();
		if ( InsidePass )
		{
			sg_end_pass();
			InsidePass = false;
			PassRenderTargetImage = nullptr;
			//CurrentTargetPass = {0};
		}
	};
	
	//	run render commands
	auto NewPass = [&](sg_image TargetTexture,SoyImageProxy* TargetImage,SoyPixelsMeta TargetTextureMeta,sg_color rgba)
	{
		EndPass();

		sg_pass_action PassAction = {0};
		PassAction.colors[0].value = rgba;

		//	if colour has zero alpha, we don't clear, just load old contents
		bool ClearColour = rgba.a > 0.0f;
		PassAction.colors[0].action = ClearColour ? SG_ACTION_CLEAR : SG_ACTION_LOAD;

		//	if no image, render to screen with "default"
		if ( TargetTexture.id == 0 )
		{
			sg_begin_default_pass( &PassAction, ViewRect.x, ViewRect.y );
			PassRenderTargetImage = nullptr;
			RenderTargetPassMeta = TargetTextureMeta;
		}
		else
		{
			if ( !TargetImage )
				throw Soy::AssertException("Rendering to texture but null soypixels");
				
			//	make a new pass, add it to our dispose-pass list and use it
			sg_pass_desc RenderTargetPassDesc = { 0 };

			// There are 4 Color and 1 depth slot on for a Render Pass Description
			// TODO: Change header to Image Array for Color textures add slot for depth image
			RenderTargetPassDesc.color_attachments[0].image = TargetTexture;

			auto RenderTargetPass = sg_make_pass(&RenderTargetPassDesc);
			TempPasses.PushBack(RenderTargetPass);

			sg_begin_pass( RenderTargetPass, &PassAction);
			PassRenderTargetImage = TargetImage;
			RenderTargetPassMeta = TargetTextureMeta;
		}
		
		auto test = sg_query_desc();
		InsidePass = true;
		//CurrentTargetPass = NewPass;
	};
	
	try
	{
		for ( auto i=0;	i<RenderCommands.mCommands.GetSize();	i++ )
		{
			auto& NextCommand = RenderCommands.mCommands[i];
			
			//	execute each command
			if ( NextCommand->GetName() == Sokol::TRenderCommand_UpdateImage::Name )
			{
				auto& UpdateImageCommand = dynamic_cast<Sokol::TRenderCommand_UpdateImage&>( *NextCommand );
				if ( !UpdateImageCommand.mImage )
				{
					throw Soy::AssertException("UpdateImage command with null image pointer");
				}
				
				auto& ImageSoy = *UpdateImageCommand.mImage;
				auto IsRenderTarget = UpdateImageCommand.mIsRenderTarget;
				SoyPixels TemporaryImage;
				
				//	if image has no sg_image, create it
				if ( !ImageSoy.HasSokolImage() )
				{
					bool GetPixelData = false;	//	true if we want a readonly image
					auto ImageDescription = GetImageDescription(ImageSoy, TemporaryImage, IsRenderTarget, GetPixelData );
					auto NewImage = sg_make_image(&ImageDescription);
					auto State = sg_query_image_state(NewImage);
					Sokol::IsOkay(State,"sg_make_image");
					Stats_ImageCounter++;
					
					//	gr: guessing this isn't threadsafe
					auto FreeSokolImage = [=]()
					{
						QueueImageDelete(NewImage);
					};
					
					ImageSoy.SetSokolImage( NewImage.id, FreeSokolImage );
					//	gr: as we haven't sent pixel data, the pixels aren't up to date
					if ( ImageDescription.usage == SG_USAGE_IMMUTABLE )
						ImageSoy.OnSokolImageUpdated();
				}
				
				bool LatestVersion = false;
				sg_image ImageSokol = {0};
				ImageSokol.id = ImageSoy.GetSokolImage(LatestVersion);
				
				//	if image sokol version is out of date, update texture
				if ( !LatestVersion )
				{
					bool GetPixelData = true;
					auto ImageDescription = GetImageDescription(ImageSoy,TemporaryImage, IsRenderTarget, GetPixelData );
					sg_update_image( ImageSokol, ImageDescription.data );
					auto State = sg_query_image_state(ImageSokol);
					Sokol::IsOkay(State,"sg_query_image_state");
					ImageSoy.OnSokolImageUpdated();
				}
			}
			else if ( NextCommand->GetName() == Sokol::TRenderCommand_Draw::Name )
			{
				if ( !InsidePass )
					throw Soy::AssertException("Draw command but not inside pass (Haven't SetRenderTarget)");
					
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
				PipelineDescription.cull_mode = SG_CULLMODE_NONE;
				
				//	colour target/attachment config
				PipelineDescription.colors[0].blend.enabled = false;
				
				
				PipelineDescription.depth =
				{
					.compare = SG_COMPAREFUNC_LESS_EQUAL,
					.write_enabled = true
				};
				
				// Some Render Target Settings need to be different here
				// Overwrite them at the end here?
				
				//	gr: we need the pipeline depth format to match the pass's depth setup
				//	in a render texture pass we don't currently have a depth target, so
				//	needs to be none. If rendering to screen (stencil) leave this as default 
				if ( PassRenderTargetImage )
				{
					PipelineDescription.depth.pixel_format = SG_PIXELFORMAT_NONE;
					//	gr: colour also needs configuring to match pass (why??)
					auto ForRenderTarget = true;
					auto PassColourFormat = GetPixelFormat( RenderTargetPassMeta.GetFormat(), ForRenderTarget );
					PipelineDescription.colors[0].pixel_format = PassColourFormat;
				}
				
				sg_pipeline Pipeline = sg_make_pipeline(&PipelineDescription);
				TempPipelines.PushBack(Pipeline);	//	gr: add to list in case state is invalid
				auto PipelineState = sg_query_pipeline_state(Pipeline);
				Sokol::IsOkay(PipelineState,"sg_make_pipeline");
				sg_apply_pipeline(Pipeline);
				
				sg_bindings Bindings = {0};
				for ( auto a=0;	a<Geometry.GetVertexLayoutBufferSlots();	a++ )
					Bindings.vertex_buffers[a] = Geometry.mVertexBuffer;
				Bindings.index_buffer = Geometry.mIndexBuffer;
				
				for ( auto i=0;	i<SG_MAX_SHADERSTAGE_IMAGES;	i++ )
				{
					std::shared_ptr<SoyImageProxy> ImageSoy;

					//	we replace explicit null entries with nulltexture
					//	non-existant entries (which should be imagecount...max) should stay
					//	as sokol_image=0
					auto& ImageUniforms = DrawCommand.mImageUniforms;
					if ( ImageUniforms.find( i ) != ImageUniforms.end() )
					{
						ImageSoy = ImageUniforms[i];
						if ( !ImageSoy )
						{
							auto Name = DrawCommand.mDebug_ImageUniformNames[i];
							std::Debug << "Using null texture for uniform " << Name <<" in draw" << std::endl;
							ImageSoy = mNullTexture;
						}
					}
						
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
				if(DrawCommand.mUniformBlock.GetSize() > 0)
				{
					sg_range UniformData = {0};
					UniformData.ptr = DrawCommand.mUniformBlock.GetArray();
					UniformData.size = DrawCommand.mUniformBlock.GetDataSize();
					sg_apply_uniforms( SG_SHADERSTAGE_VS, 0, UniformData );
					sg_apply_uniforms( SG_SHADERSTAGE_FS, 0, UniformData );
				}
				auto VertexCount = Geometry.GetDrawVertexCount();
				auto VertexFirst = Geometry.GetDrawVertexFirst();
				auto InstanceCount = Geometry.GetDrawInstanceCount();
				sg_draw(VertexFirst,VertexCount,InstanceCount);
				
				FreePipelines();
			}
			else if ( NextCommand->GetName() == Sokol::TRenderCommand_SetRenderTarget::Name )
			{
				auto& SetRenderTargetCommand = dynamic_cast<Sokol::TRenderCommand_SetRenderTarget&>( *NextCommand );
				
				//	no image = screen
				sg_image RenderTargetImage = {0};
				SoyPixelsMeta RenderTargetImageMeta;
				SoyImageProxy* TargetImage = nullptr;
					
				if ( !SetRenderTargetCommand.mTargetTexture ) // js land = Commands.push( [ "SetRenderTarget", null ] )
				{
				}
				else
				{
					bool LatestVersion = true;
					auto& RenderTexture = *SetRenderTargetCommand.mTargetTexture;
					TargetImage = &RenderTexture;
					RenderTargetImage.id = RenderTexture.GetSokolImage(LatestVersion);
					RenderTargetImageMeta = RenderTexture.GetMeta();
					
					//	probe image to throw if the image is invalid
					auto State = sg_query_image_state( RenderTargetImage );
				}
			
				auto rgba = SetRenderTargetCommand.mClearColour;
				NewPass( RenderTargetImage, TargetImage, RenderTargetImageMeta, rgba );
			}
			else if ( NextCommand->GetName() == Sokol::TRenderCommand_ReadPixels::Name )
			{
				auto& Command = dynamic_cast<Sokol::TRenderCommand_ReadPixels&>( *NextCommand );
				auto& Image = *Command.mImage;
				auto ImageMeta = Image.GetMeta();
				
				GLenum ImagePixelType = GL_UNSIGNED_BYTE;
				if ( SoyPixelsFormat::IsFloatChannel(ImageMeta.GetFormat()) )
					ImagePixelType = GL_FLOAT;
				else if ( SoyPixelsFormat::GetBytesPerChannel(ImageMeta.GetFormat()) == 2 )
					ImagePixelType = GL_UNSIGNED_SHORT;
				
				Opengl::TPbo PixelBuffer(ImageMeta);
				PixelBuffer.ReadPixels(ImagePixelType);
				auto* pData = const_cast<uint8_t*>(PixelBuffer.LockBuffer());
				auto PixelsBufferSize = ImageMeta.GetDataSize();
				SoyPixelsRemote PixelsBuffer( pData, PixelsBufferSize, ImageMeta );
				Image.SetPixels( PixelsBuffer );
				PixelBuffer.UnlockBuffer();
				
				//	gr: bit of a bodge. In old renderer, we explicitly did pixel copy
				//		at end of render-to-texture pass so we knew they were in sync
				//		we still need to update at end of pass, but we need to know nothing
				//		rendered to the target after now
				if ( &Image == PassRenderTargetImage )
					PassRenderTargetImage->OnSokolImageUpdated();
			}
			else
			{
				throw Soy::AssertException( std::string(NextCommand->GetName()) + " is unhandled render command"); 
			}
		}
	}
	catch(std::exception& e)
	{
		std::Debug << "Exception in OnPaints RenderCommand Loop: " << e.what() << std::endl;
		RenderError = std::string("RenderCommand exception: ") + e.what();
	}
	
	//	end pass
	EndPass();

	//	cleanup resources only used on the frame
	FreePipelines();
	for ( auto p=0;	p<TempBuffers.GetSize();	p++ )
	{
		auto Buffer = TempBuffers[p];
		sg_destroy_buffer(Buffer);
	}

	for ( auto p=0; p<TempPasses.GetSize(); p++)
	{
		auto Pass = TempPasses[p];
		sg_destroy_pass(Pass);
	}

	//	save last
	//mLastFrame = RenderCommands;
	//mLastFrame.mPromiseRef = std::numeric_limits<size_t>::max();
	
	//	gr: avoid deadlocks by queuing the resolve; Dont want main thread (UI render) to wait on JS
	//		in case JS thread is trying to do something on main thread (UI stuff)
	
	//	we want to resolve the promise NOW, after rendering has been submitted
	//	but we may want here, some callback to block or glFinish or something before resolving
	//	but really we should be using something like glsync?
	if ( RenderCommands.mPromiseRef != std::numeric_limits<size_t>::max() )
	{
		auto Resolve = [RenderError](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
		{
			if ( RenderError.empty() )
				Promise.Resolve(LocalContext,0);
			else
				Promise.Reject(LocalContext,RenderError);
		};
		mPendingFramePromises.QueueFlush(RenderCommands.mPromiseRef,Resolve);
	}
}

void ApiSokol::TSokolContextWrapper::Construct(Bind::TCallback &Params)
{
	auto RenderViewObject = Params.GetArgumentObject(0);
	auto& RenderViewWrapper = RenderViewObject.This<ApiGui::TRenderViewWrapper>();
	
	//	init last-frame for any paints before we get a chance to render
	//	gr: maybe this should be like a Render() call and use js
	InitDebugFrame();
	InitDefaultAssets();
	
	Sokol::TContextParams SokolParams;
	
	SokolParams.mRenderView = RenderViewWrapper.mControl;
	SokolParams.mOnPaint = [this](sg_context Context,vec2x<size_t> Rect)	{	this->OnPaint(Context,Rect);	};
	
	//	create platform-specific context
	mSokolContext = Sokol::Platform_CreateContext(SokolParams);
}

void ApiSokol::TSokolContextWrapper::InitDebugFrame()
{
/*
	auto pClear = std::make_shared<Sokol::TRenderCommand_SetRenderTarget>();
	pClear->mClearColour = {0,1,1,1};
	Commands.mCommands.PushBack(pClear);
	*/
}

void ApiSokol::TSokolContextWrapper::InitDefaultAssets()
{
	SoyPixels NullTexture( SoyPixelsMeta(1,1,SoyPixelsFormat::RGBA) );
	mNullTexture.reset( new SoyImageProxy() );
	mNullTexture->SetPixels(NullTexture);
	
	//	need to make a render command to init the image
	//	which needs to be in a command list
	Sokol::TRenderCommands Commands;
	auto PushCommand = [&](std::shared_ptr<Sokol::TRenderCommandBase> Command)
	{
		Commands.mCommands.PushBack(Command);
	};
		
	{
		auto pUpdateImage = std::make_shared<Sokol::TRenderCommand_UpdateImage>();
		pUpdateImage->mImage = mNullTexture;
		pUpdateImage->mIsRenderTarget = false;
		PushCommand(pUpdateImage);
	}
		
	//	put in queue
	std::lock_guard<std::mutex> Lock(mPendingFramesLock);
	mPendingFrames.PushBack(Commands);
}


void ApiSokol::TSokolContextWrapper::GetScreenRect(Bind::TCallback& Params)
{
	BufferArray<int,4> LastRect;
	LastRect.PushBack( 0 );
	LastRect.PushBack( 0 );
	LastRect.PushBack( mLastRect.x );
	LastRect.PushBack( mLastRect.y );
	Params.Return( GetArrayBridge(LastRect) );
}


void ApiSokol::TSokolContextWrapper::CanRenderToPixelFormat(Bind::TCallback& Params)
{
	auto ForRenderTarget = true;
	auto PixelFormatString = Params.GetArgumentString(0);
	auto PixelFormatSoy = SoyPixelsFormat::ToType(PixelFormatString);
	auto PixelFormat = GetPixelFormat(PixelFormatSoy, ForRenderTarget);
	auto PixelFormatInfo = sg_query_pixelformat(PixelFormat);
	auto CanRender = PixelFormatInfo.render;
	
	Params.Return(CanRender);
}

void ApiSokol::TSokolContextWrapper::GetStats(Bind::TCallback& Params)
{
	auto Stats = Params.mContext.CreateObjectInstance(Params.mLocalContext);
	//auto SokolContext = mContext->GetContext();
	Stats.SetInt("Sokol ImageCount",Stats_ImageCounter);
	Stats.SetInt("Pop.Image ImageCount",TImageWrapper::Debug_ImageCounter);
	Stats.SetInt("SoyImageProxy ImageCount",SoyImageProxy::Pool.GetSize());
	
	std::stringstream ImageNames;
	for ( auto i=0;	i<SoyImageProxy::Pool.GetSize();	i++ )
	{
		auto& Name = SoyImageProxy::Pool[i]->mName;
		ImageNames << Name << ", ";
	}
	Stats.SetString("ImageNames", ImageNames.str() );
	
	Params.Return(Stats);
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
		//	we still want to put a null entry in
		//	gr: this shouldn't come up as this function is only called from iterating keys
		if ( !UniformsObject.HasMember(Uniform.mName) )
			return;
		
		//	we want to explicitly allow null, and in the actual render process, we swap
		//	it for our nulltexture
		if ( UniformsObject.IsMemberNull(Uniform.mName) )
			return;
		
		//	here we don't write to the block, but we need to work out what image is going
		//	to go in the image slot
		auto ImageObject = UniformsObject.GetObject( Uniform.mName );
		auto& ImageWrapper = ImageObject.This<TImageWrapper>();
		auto ImageSoy = ImageWrapper.mImage;
		//	todo: add a "update image pixels" render command before draw
		mImageUniforms[UniformImageIndex] = ImageSoy;
	};
	
	//	we explicitly need keys in the map setup for image uniforms
	//	they can just be initialised to null and will be swapped with
	for ( auto ImageIndex=0;	ImageIndex<Shader.mShaderMeta.mImageUniforms.GetSize();	ImageIndex++ )
	{
		auto& Uniform = Shader.mShaderMeta.mImageUniforms[ImageIndex];
		mImageUniforms[ImageIndex] = nullptr;
		mDebug_ImageUniformNames[ImageIndex] = Uniform.mName;
	}
	
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
			try
			{
				WriteImageUniform( *pImageUniform, UniformImageIndex );
			}
			catch(std::exception& e)
			{
				//	more verbose error
				std::stringstream Error;
				Error << __PRETTY_FUNCTION__ << " Error getting required image uniform " << UniformName << "; " << e.what();
				throw Soy::AssertException(Error);
			}
			continue;
		}

		//	we need to set context or command to put out debug
		bool VerboseDebug = false;
		if ( VerboseDebug )
			std::Debug << "No uniform/image named " << UniformName << std::endl;
	}
	
	//	did we write to all memory?
}


void Sokol::ParseRenderCommand(std::function<void(std::shared_ptr<Sokol::TRenderCommandBase>)> PushCommand,const std::string_view& Name,Bind::TCallback& Params,std::function<Sokol::TShader&(uint32_t)>& GetShader)
{
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
			//	don't queue null entries
			if ( !pImageWrapper )
				continue;
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
			if (!Params.IsArgumentUndefined(3))
			{
				std::stringstream Error;
				Error << "Render Command " << Name << " has read-backformat specified, but with null target, should not be provided.";
				throw Soy::AssertException(Error);
			}
		}
		else
		{
			auto ImageObject = Params.GetArgumentObject(1);
			//	gr: this was auto Image= and may have been causing the leak... but why? why/what was it dangling
			auto& Image = ImageObject.This<TImageWrapper>();
			pSetRenderTarget->mTargetTexture = Image.mImage;

			//	get readback format
			if (!Params.IsArgumentUndefined(3))
			{
				//	gr: is readback better as a command?
				auto FormatString = Params.GetArgumentString(3);
				auto Format = SoyPixelsFormat::ToType(FormatString);
				pSetRenderTarget->mReadBackFormat = Format;
			}

			//	insert a update-image command, so that sg image will get created
			//	although, it doesnt need pixels updating as they will get overriden...
			auto pUpdateImage = std::make_shared<TRenderCommand_UpdateImage>();
			pUpdateImage->mImage = pSetRenderTarget->mTargetTexture;
			pUpdateImage->mIsRenderTarget = true;
			PushCommand(pUpdateImage);
		}
		
		//	argument 2 is clear color, if not provided, no clear
		if ( !Params.IsArgumentUndefined(2) )
		{
			//	allow rgb or rgba (assume alpha=1 if anything provided)
			BufferArray<float,4> ClearColour;
			Params.GetArgumentArray(2, GetArrayBridge(ClearColour) );
			if ( ClearColour.GetSize() < 3 )
			{
				std::stringstream Error;
				Error << Name << " clear colour argument provided only " << ClearColour.GetSize() << " elements. Expected RGB or RGBA";
				throw Soy::AssertException(Error);
			}
			if ( ClearColour.GetSize() < 4 )
				ClearColour.PushBack(1.0f);
			pSetRenderTarget->mClearColour.r = ClearColour[0];
			pSetRenderTarget->mClearColour.g = ClearColour[1];
			pSetRenderTarget->mClearColour.b = ClearColour[2];
			pSetRenderTarget->mClearColour.a = ClearColour[3];
		}

		PushCommand(pSetRenderTarget);
		return;
	}
	
	if (Name == TRenderCommand_ReadPixels::Name)
	{
		auto pSetRenderTarget = std::make_shared<TRenderCommand_ReadPixels>();
		pSetRenderTarget->Init(Params);
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
		
		//	make these errors much clearer
		try
		{
			Sokol::ParseRenderCommand( PushCommand, CommandName, CommandAndParams, GetShader );
		}
		catch(std::exception& e)
		{
			std::stringstream Exception;
			Exception << "Error in ParseRenderCommand(" << CommandName << "): " << e.what();
			throw Soy::AssertException(Exception);
		}
	}
	return Commands;
}

void Sokol::IsOkay(sg_resource_state State,const char* Context)
{
	if ( State == SG_RESOURCESTATE_VALID )
		return;
	
	std::stringstream Error;
	Error << "Sokol resource state error " << magic_enum::enum_name(State) << " in " << Context;
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

	//	sokol doesnt support int or bool
	//	gr: get gl error if we use these
	//if ( TypeName == "int" )	return SG_UNIFORMTYPE_FLOAT;
	//if ( TypeName == "bool" )	return SG_UNIFORMTYPE_FLOAT;
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
	Description.data.ptr = mBufferData.GetArray();
	Description.data.size = mBufferData.GetDataSize();
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
	
	//	gr: this should really be where we setup the uniforms!
	auto MaxUniforms = std::size( Description.uniforms );
	if ( mUniforms.GetSize() > MaxUniforms )
		throw Soy::AssertException("CreateShader: Too many uniforms!");
	
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
		Description.image_type = SG_IMAGETYPE_2D;
		Description.sampler_type = _SG_SAMPLERTYPE_DEFAULT;

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


void Sokol::TRenderCommand_ReadPixels::Init(Bind::TCallback& Params)
{
	auto ImageObject = Params.GetArgumentObject(1);
	//	gr: this was auto Image= and may have been causing the leak... but why? why/what was it dangling
	auto& Image = ImageObject.This<TImageWrapper>();
	mImage = Image.mImage;

	if ( !mImage )
		throw Soy::AssertException("Parsing ReadPixels command, but image's proxy image is null");

	mImage->GetMeta().GetFormat();
	
	if ( !Params.IsArgumentUndefined(2) )
	{
		auto PixelFormatString = Params.GetArgumentString(0);
		auto PixelFormatSoy = SoyPixelsFormat::ToType(PixelFormatString);
		mReadFormat = PixelFormatSoy;
	}
}
