#include "TFilterStageOpengl.h"






TFilterStage_ShaderBlit::TFilterStage_ShaderBlit(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename,const Opengl::TGeometryVertex& BlitVertexDescription,TFilter& Filter) :
	TFilterStage	( Name, Filter ),
	mVertFilename	( VertFilename ),
	mFragFilename	( FragFilename ),
	mBlitVertexDescription	( BlitVertexDescription ),
	mVertFileWatch	( VertFilename ),
	mFragFileWatch	( FragFilename )
{
	auto OnFileChanged = [this,&Filter](const std::string& Filename)
	{
		//	this is triggered from the main thread. But Reload() waits on opengl (also main thread...) so we deadlock...
		//	to fix this, we put it on a todo list on the filter
		auto DoReload = [this]
		{
			Reload();
			return true;
		};
		Filter.QueueJob( DoReload );
	};
	
	mVertFileWatch.mOnChanged.AddListener( OnFileChanged );
	mFragFileWatch.mOnChanged.AddListener( OnFileChanged );
	
	Reload();
}


void TFilterStage_ShaderBlit::Reload()
{
	Opengl::TContext& Context = mFilter.GetOpenglContext();
	
	std::Debug << "Loading shader files for " << this->mName << std::endl;

	//	load files
	std::string VertSrc;
	if ( !Soy::FileToString( mVertFilename, VertSrc ) )
		return;
	
	std::string FragSrc;
	if ( !Soy::FileToString( mFragFilename, FragSrc ) )
		return;

	auto BuildShader = [this,&VertSrc,&FragSrc,&Context]
	{
		try
		{
			//	don't override the shader until it succeeds
			std::shared_ptr<Opengl::TShader> NewShader( new Opengl::TShader( VertSrc, FragSrc, mBlitVertexDescription, mName, Context ) );

			//	gr; may need std::move here
			mShader = NewShader;
		}
		catch (std::exception& e)
		{
			std::Debug << "Failed to compile shader: " << e.what() << std::endl;
		}
		return true;
	};
	
	Soy::TSemaphore Semaphore;
	Context.PushJob( BuildShader, Semaphore );
	Semaphore.Wait("build shader");

	if ( !mShader )
		return;

	std::Debug << "Loaded shader (" << mShader->mProgram.mName << ") okay for " << this->mName << std::endl;
	this->mOnChanged.OnTriggered(*this);
}

bool TFilterStageRuntimeData_ShaderBlit::SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter)
{
	//	pre-emptive for debugging
	auto& TextureName = StageName;
	if ( !Soy::StringBeginsWith( Uniform.mName, TextureName, true ) )
		return false;

	//	use opencl buffer directly if it exists
	if ( Uniform.mType == "image2d_t" && mImageBuffer )
	{
		if ( TFilterFrame::SetTextureUniform( Shader, Uniform, mImageBuffer->GetMeta(), StageName, Filter ) )
			return true;
		
		auto& Kernel = dynamic_cast<Opencl::TKernelState&>( Shader );
		return Kernel.SetUniform( Uniform.mName.c_str(), *mImageBuffer );
	}
	
	//auto Texture = GetTexture( Filter.GetOpenglContext(), Filter.GetOpenclContext(), true );
	auto Texture = GetTexture();
	
	return TFilterFrame::SetTextureUniform( Shader, Uniform, Texture, StageName, Filter );
}

void TFilterStageRuntimeData_ShaderBlit::Shutdown(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	auto DefferedDelete = [this]
	{
		mTexture.Delete();
	};
	
	Soy::TSemaphore Semaphore;
	ContextGl.PushJob( DefferedDelete, Semaphore );
	Semaphore.Wait();
}


Opengl::TTexture TFilterStageRuntimeData_ShaderBlit::GetTexture(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl,bool Blocking)
{
	if ( mTexture.IsValid(false) )
		return mTexture;

	//	convert image buffer if it's there
	if ( mImageBuffer )
	{
		auto Read = [this,&ContextGl,Blocking]
		{
			Opengl::TTextureAndContext TextureAndContext( mTexture, ContextGl );

			if ( Blocking )
			{
				Opencl::TSync Semaphore;
				mImageBuffer->Read( TextureAndContext, &Semaphore );
				Semaphore.Wait();
			}
			else
			{
				mImageBuffer->Read( TextureAndContext, nullptr );
			}
		};
		
		//	make a texture
		auto MakeTexture = [this]
		{
			mTexture = std::move( Opengl::TTexture( mImageBuffer->GetMeta(), GL_TEXTURE_2D ) );
		};
		
		if ( Blocking )
		{
			Soy::TSemaphore SemaphoreGl;
			ContextGl.PushJob( MakeTexture, SemaphoreGl );
			SemaphoreGl.Wait();

			Soy::TSemaphore SemaphoreCl;
			ContextCl.PushJob( Read, SemaphoreCl );
			SemaphoreCl.Wait();
		}
		else
		{
			ContextGl.PushJob( MakeTexture );
			ContextCl.PushJob( Read );
		}
	}

	return mTexture;
}




void TFilterStage_ShaderBlit::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	static int DebugColourOffset = 0;
	DebugColourOffset++;

	
	auto BlitToTexture = [&Data,&Frame,this]
	{
		static float Sat = 0.7f;
		static float Lum = 0.5f;
		Soy::TRgb DebugClearColours[] =
		{
			Soy::THsl(0.00f,Sat,Lum),
			Soy::THsl(0.10f,Sat,Lum),
			Soy::THsl(0.20f,Sat,Lum),
			Soy::THsl(0.30f,Sat,Lum),
			Soy::THsl(0.40f,Sat,Lum),
			Soy::THsl(0.50f,Sat,Lum),
			Soy::THsl(0.60f,Sat,Lum),
			Soy::THsl(0.70f,Sat,Lum),
			Soy::THsl(0.80f,Sat,Lum),
			Soy::THsl(0.90f,Sat,Lum),
		};
		
		if ( !mFilter.mBlitQuad )
			throw Soy::AssertException("Filter blit quad missing");
		
		if ( !mShader )
			throw Soy::AssertException("Filter blit shader missing");
		
		if ( !Data )
		{
			auto* pData = new TFilterStageRuntimeData_ShaderBlit;
			Data.reset( pData );
		}
		
		auto& StageTarget = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data.get() ).mTexture;

		//	gr: warning, can get stuck when this waits in opengl, as this is already opengl...
		auto FrameTexture = Frame.GetFrameTexture( mFilter, false );
		if ( !StageTarget.IsValid() )
		{
			auto Format = SoyPixelsFormat::RGBA;
			SoyPixelsMeta Meta( FrameTexture.GetWidth(), FrameTexture.GetHeight(), Format );
			StageTarget = Opengl::TTexture( Meta, GL_TEXTURE_2D );
		}
		auto& StageName = mName;
		
		//	gr: cache/pool these rendertargets?
		Opengl::TFboMeta FboMeta( StageName, StageTarget.GetWidth(), StageTarget.GetHeight() );
		Opengl::TRenderTargetFbo RenderTarget( FboMeta, StageTarget );
		RenderTarget.mGenerateMipMaps = false;
		
		//	render this stage to the stage target fbo
		RenderTarget.Bind();
		
		try
		{
			auto& StageShader = *mShader;
			auto& BlitQuad = *mFilter.mBlitQuad;
			
			glDisable(GL_BLEND);
			Opengl::ClearColour( DebugClearColours[(DebugColourOffset)%sizeofarray(DebugClearColours)], 1 );
			Opengl::ClearDepth();
			
			//	write blend RGB but write alpha directly
			glEnable(GL_BLEND);
			glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
			Opengl_IsOkay();
			
			auto Shader = StageShader.Bind();

			//	set uniforms
			for ( int u=0;	u<StageShader.mUniforms.GetSize();	u++ )
			{
				auto& Uniform = StageShader.mUniforms[u];
				if ( Frame.SetUniform( Shader, Uniform, mFilter ) )
					continue;
					
				//	maybe surpress this until we need it... or only warn once
				static bool DebugUnsetUniforms = false;
				if ( DebugUnsetUniforms )
					std::Debug << "Warning; unset uniform " << Uniform.mName << std::endl;
			}
			BlitQuad.Draw();
		}
		catch(...)
		{
			RenderTarget.Unbind();
			throw;
		}
		RenderTarget.Unbind();
	};

	
	//	prefetch frame texture
	{
		//Soy::TScopeTimerPrint Timer("Frame.GetFrameTexture",50);
		auto FrameTexture = Frame.GetFrameTexture( mFilter, true );
	}
	
	Soy::TSemaphore Semaphore;
	mFilter.GetOpenglContext().PushJob( BlitToTexture, Semaphore );
	static bool ShowBlitTime = false;

	Semaphore.Wait( ShowBlitTime ? (mName + " blit").c_str() : nullptr );
}

