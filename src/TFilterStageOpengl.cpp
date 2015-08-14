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
	return TFilterFrame::SetTextureUniform( Shader, Uniform, mTexture, StageName );
}


TFilterStage_ReadPixels::TFilterStage_ReadPixels(const std::string& Name,const std::string& SourceStage,TFilter& Filter) :
	TFilterStage	( Name, Filter ),
	mSourceStage	( SourceStage )
{
	
}
	
bool TFilterStage_ReadPixels::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& pData)
{
	std::Debug << "reading pixels stage " << mName << std::endl;

	//	get source texture
	auto SourceDatait = Frame.mStageData.find( mSourceStage );
	if ( SourceDatait == Frame.mStageData.end() )
		return false;
	auto& SourceData = SourceDatait->second;
	if ( !SourceData )
		return false;
	
	auto SourceTexture = SourceData->GetTexture();
	bool ReadSuccess = false;

	auto ReadPixels = [&SourceTexture,&pData,&ReadSuccess]
	{
		if ( !SourceTexture.IsValid() )
		{
			ReadSuccess = false;
			return true;
		}

		//	alloc pixels if we need to
		if ( !pData )
			pData.reset( new TFilterStageRuntimeData_ReadPixels );
		auto& Data = *dynamic_cast<TFilterStageRuntimeData_ReadPixels*>( pData.get() );

		try
		{
			SourceTexture.Read( Data.mPixels );
			ReadSuccess = true;
		}
		catch (std::exception& e)
		{
			std::Debug << "Exception reading texture: " << e.what() << std::endl;
			ReadSuccess = false;
		}
		return true;
	};
	
	Soy::TSemaphore Semaphore;
	mFilter.GetOpenglContext().PushJob(ReadPixels,Semaphore);
	Semaphore.Wait();
	
	return ReadSuccess;
}

bool TFilterStageRuntimeData_ReadPixels::SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter)
{
	return false;
}



bool TFilterStage_ShaderBlit::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data)
{
	static int DebugColourOffset = 0;
	DebugColourOffset++;

	
	auto BlitToTexture = [&Data,&Frame,this]
	{
		Soy::TRgb DebugClearColours[] =
		{
			Soy::TRgb(1,0,0),
			Soy::TRgb(1,1,0),
			Soy::TRgb(0,1,0),
			Soy::TRgb(0,1,1),
			Soy::TRgb(0,0,1),
			Soy::TRgb(1,0,1),
		};
		
		if ( !mFilter.mBlitQuad )
			throw Soy::AssertException("Filter blit quad missing");
		
		if ( !mShader )
			throw Soy::AssertException("Filter blit shader missing");
		
		if ( !Data )
		{
			auto* pData = new TFilterStageRuntimeData_ShaderBlit;
			Data.reset( pData );
			auto& StageTarget = pData->mTexture;
			auto& FrameTexture = Frame.mFrameTexture;

			if ( !StageTarget.IsValid() )
			{
				auto Format = SoyPixelsFormat::RGBA;
				SoyPixelsMeta Meta( FrameTexture.GetWidth(), FrameTexture.GetHeight(), Format );
				StageTarget = Opengl::TTexture( Meta, GL_TEXTURE_2D );
			}
		}
		auto& StageTarget = dynamic_cast<TFilterStageRuntimeData_ShaderBlit*>( Data.get() )->mTexture;
		auto& StageName = mName;
		
		//	gr: cache/pool these rendertargets?
		Opengl::TFboMeta FboMeta( StageName, StageTarget.GetWidth(), StageTarget.GetHeight() );
		std::shared_ptr<Opengl::TRenderTarget> pRenderTarget( new Opengl::TRenderTargetFbo( FboMeta, StageTarget ) );
		auto& RenderTarget = *pRenderTarget;
		
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
	
	Soy::TSemaphore Semaphore;
	mFilter.GetOpenglContext().PushJob( BlitToTexture, Semaphore );
	static bool ShowBlitTime = false;
	try
	{
		Semaphore.Wait( ShowBlitTime ? (mName + " blit").c_str() : nullptr );
		return true;
	}
	catch ( std::exception& e)
	{
		std::Debug << "Filter stage failed: " << e.what() << std::endl;
		return false;
	}
}


