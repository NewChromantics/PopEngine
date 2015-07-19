#include "TFilter.h"
#include "TFilterWindow.h"

const char* TFilter::FrameSourceName = "Frame";


class TOpenglJob_UploadPixels : public Opengl::TJob
{
public:
	TOpenglJob_UploadPixels(std::shared_ptr<SoyPixels>& Pixels,std::shared_ptr<TFilterFrame>& Frame) :
		mPixels	( Pixels ),
		mFrame	( Frame )
	{
	}
	
	virtual bool		Run(std::ostream& Error);

	std::shared_ptr<SoyPixels>		mPixels;
	std::shared_ptr<TFilterFrame>	mFrame;
};


class TFilterJobRun : public Opengl::TJob
{
public:
	TFilterJobRun(TFilter& Filter,std::shared_ptr<TFilterFrame>& Frame) :
		mFrame		( Frame ),
		mFilter		( &Filter )
	{
	}

	virtual bool		Run(std::ostream& Error);
	
	//	eek, no safe pointer here!
	TFilter*						mFilter;
	std::shared_ptr<TFilterFrame>	mFrame;
};





TFilterStage::TFilterStage(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename,const Opengl::TGeometryVertex& BlitVertexDescription,TFilter& Filter) :
	mName			( Name ),
	mVertFilename	( VertFilename ),
	mFragFilename	( FragFilename ),
	mBlitVertexDescription	( BlitVertexDescription ),
	mFilter			( Filter ),
	mVertFileWatch	( VertFilename ),
	mFragFileWatch	( FragFilename )
{
	auto OnFileChanged = [this](const std::string& Filename)
	{
		Reload();
	};
	
	mVertFileWatch.mOnChanged.AddListener( OnFileChanged );
	mFragFileWatch.mOnChanged.AddListener( OnFileChanged );
}

void TFilterStage::Reload()
{
	Opengl::TContext& Context = mFilter.GetContext();
	
	auto BuildShader = [this]
	{
		std::Debug << "Loading shader files for " << this->mName << std::endl;
		//	load files
		std::string VertSrc;
		if ( !Soy::FileToString( mVertFilename, VertSrc ) )
			return true;
		std::string FragSrc;
		if ( !Soy::FileToString( mFragFilename, FragSrc ) )
			return true;
		
		try
		{
			//	don't override the shader until it succeeds
			auto NewShader = Opengl::BuildProgram( VertSrc, FragSrc, mBlitVertexDescription, mName );

			if ( !NewShader.IsValid() )
				return true;
		
			//	gr; may need std::move here
			mShader = NewShader;
		}
		catch (std::exception& e)
		{
			std::Debug << "Failed to compile shader: " << e.what() << std::endl;
			return true;
		}
		std::Debug << "Loaded shader (" << mShader.program.mName << ") okay for " << this->mName << std::endl;
		this->mOnChanged.OnTriggered(*this);
		return true;
	};
	
	Context.PushJob( BuildShader );
}



bool TOpenglJob_UploadPixels::Run(std::ostream& Error)
{
	auto& Frame = *mFrame;
	auto& Pixels = *mPixels;
	
	//	make texture if it doesn't exist
	if ( !Frame.mFrame.IsValid() )
	{
		SoyPixelsMetaFull Meta( Pixels.GetWidth(), Pixels.GetHeight(), Pixels.GetFormat() );
		Frame.mFrame = Opengl::TTexture( Meta, GL_TEXTURE_2D );
	}
	
	Frame.mFrame.Copy( Pixels, true, true, true );
	return true;
}


bool TFilterJobRun::Run(std::ostream& Error)
{
	return mFrame->Run( Error, *mFilter );
}
	

bool TFilterFrame::Run(std::ostream& Error,TFilter& Filter)
{
	std::Debug << __func__ << std::endl;
	
	auto& Frame = *this;
	auto& FrameTexture = Frame.mFrame;
	
	static int DebugColourOffset = 0;
	DebugColourOffset++;
	Soy::TRgb DebugClearColours[] =
	{
		Soy::TRgb(1,0,0),
		Soy::TRgb(1,1,0),
		Soy::TRgb(0,1,0),
		Soy::TRgb(0,1,1),
		Soy::TRgb(0,0,1),
		Soy::TRgb(1,0,1),
	};
	
	//	run through the shader chain
	for ( int s=0;	s<Filter.mStages.GetSize();	s++ )
	{
		auto pStage = Filter.mStages[s];
		auto& StageName = pStage->mName;
		if ( !pStage )
		{
			std::Debug << "Warning: Filter " << Filter.mName << " stage #" << s << " is null" << std::endl;
			continue;
		}
		
		//	get texture for this stage
		auto& StageTarget = Frame.mShaderTextures[StageName];
		if ( !StageTarget.IsValid() )
		{
			//	SoyPixelsMetaFull Meta( FrameTexture.GetWidth(), FrameTexture.GetHeight(), FrameTexture.GetFormat() );
			SoyPixelsMetaFull Meta( FrameTexture.GetWidth(), FrameTexture.GetHeight(), SoyPixelsFormat::RGBA );
			StageTarget = Opengl::TTexture( Meta, GL_TEXTURE_2D );
		}
		
		//	gr: cache/pool these rendertargets?
		Opengl::TFboMeta FboMeta( StageName, StageTarget.GetWidth(), StageTarget.GetHeight() );
		std::shared_ptr<Opengl::TRenderTarget> pRenderTarget( new Opengl::TRenderTargetFbo( FboMeta, StageTarget ) );
		auto& RenderTarget = *pRenderTarget;
		
		//	render this stage to the stage target fbo
		RenderTarget.Bind();
		{
			auto& StageShader = pStage->mShader;
			glDisable(GL_BLEND);
			Opengl::ClearColour( DebugClearColours[(s+DebugColourOffset)%sizeofarray(DebugClearColours)], 1 );
			Opengl::ClearDepth();
			
			//	write blend RGB but write alpha directly
			glEnable(GL_BLEND);
			//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			//glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
			
			//	no rgb, all alpha
			//glBlendFuncSeparate( GL_ZERO, GL_ONE, GL_ONE, GL_ZERO);
			
			glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
			Opengl_IsOkay();
			
			auto Shader = StageShader.Bind();
			if ( Shader.IsValid() )
			{
				std::Debug << "drawing stage " << StageName << std::endl;
				//	gr: go through uniforms, find any named same as a shader and bind that shaders output
				for ( int u=0;	u<StageShader.mUniforms.GetSize();	u++ )
				{
					auto& Uniform = StageShader.mUniforms[u];
					
					if ( SetUniform( Shader, Uniform, Filter ) )
						continue;

					//	maybe surpress this until we need it... or only warn once
					static bool DebugUnsetUniforms = false;
					if ( DebugUnsetUniforms )
						std::Debug << "Warning; unset uniform " << Uniform.mName << std::endl;
				}
				Filter.mBlitQuad.Draw();
			}
			else
			{
				std::Debug << __func__ << " stage has no valid shader" << std::endl;
			}
		}
		RenderTarget.Unbind();
	}
	
	return true;
}


bool TFilterFrame::SetTextureUniform(Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,Opengl::TTexture& Texture,const std::string& TextureName)
{
	if ( !Soy::StringBeginsWith( Uniform.mName, TextureName, true ) )
		return false;
	
	//	is there a suffix?
	std::string Suffix;
	Suffix = Uniform.mName.substr( TextureName.length(), std::string::npos );
	
	if ( Suffix.empty() )
	{
		Shader.SetUniform( Uniform.mName, Texture );
		return true;
	}
	
	if ( Suffix == "_TexelWidthHeight" )
	{
		vec2f Size( 1.0f / static_cast<float>(Texture.GetWidth()), 1.0f / static_cast<float>(Texture.GetHeight()) );
		Shader.SetUniform( Uniform.mName, Size );
		return true;
	}
	
	if ( Suffix == "_PixelWidthHeight" )
	{
		vec2f Size( Texture.GetWidth(), Texture.GetHeight() );
		Shader.SetUniform( Uniform.mName, Size );
		return true;
	}
	
	return false;
}

bool TFilterFrame::SetUniform(Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,TFilter& Filter)
{
	//	do texture bindings
	if ( SetTextureUniform( Shader, Uniform, mFrame, TFilter::FrameSourceName ) )
		return true;
	
	for ( auto it=mShaderTextures.begin();	it!=mShaderTextures.end();	it++ )
	{
		auto& Texture = it->second;
		auto& TextureName = it->first;
		
		if ( SetTextureUniform( Shader, Uniform, Texture, TextureName ) )
			return true;
	}
	
	if ( Filter.SetUniform( Shader, Uniform ) )
		return true;
	
	return false;
}


TFilter::TFilter(const std::string& Name) :
	TFilterMeta		( Name )
{
	//	create window
	vec2f WindowPosition( 0, 0 );
	vec2f WindowSize( 600, 200 );
	
	mWindow.reset( new TFilterWindow( Name, WindowPosition, WindowSize, *this ) );
	
	auto CreateBlitGeo = [this]
	{
		//	make mesh
		struct TVertex
		{
			vec2f	uv;
		};
		class TMesh
		{
		public:
			TVertex	mVertexes[4];
		};
		TMesh Mesh;
		Mesh.mVertexes[0].uv = vec2f( 0, 0);
		Mesh.mVertexes[1].uv = vec2f( 1, 0);
		Mesh.mVertexes[2].uv = vec2f( 1, 1);
		Mesh.mVertexes[3].uv = vec2f( 0, 1);
		Array<GLshort> Indexes;
		Indexes.PushBack( 0 );
		Indexes.PushBack( 1 );
		Indexes.PushBack( 2 );
		
		Indexes.PushBack( 2 );
		Indexes.PushBack( 3 );
		Indexes.PushBack( 0 );
		
		//	for each part of the vertex, add an attribute to describe the overall vertex
		Opengl::TGeometryVertex Vertex;
		auto& UvAttrib = Vertex.mElements.PushBack();
		UvAttrib.mName = "TexCoord";
		UvAttrib.mType = GL_FLOAT;
		UvAttrib.mIndex = 0;	//	gr: does this matter?
		UvAttrib.mArraySize = 2;
		UvAttrib.mElementDataSize = sizeof( Mesh.mVertexes[0].uv );
		
		Array<uint8> MeshData;
		MeshData.PushBackReinterpret( Mesh );
		mBlitQuad = Opengl::CreateGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex, GetContext() );
		
		return true;
	};
	
	//	create blit geometry
	mWindow->GetContext()->PushJob( CreateBlitGeo );
}

void TFilter::AddStage(const std::string& Name,const std::string& VertShader,const std::string& FragShader)
{
	//	make sure stage doesn't exist
	for ( int s=0;	s<mStages.GetSize();	s++ )
	{
		auto& Stage = *mStages[s];
		Soy::Assert( !(Stage == Name), "Stage already exists" );
	}

	std::shared_ptr<TFilterStage> Stage( new TFilterStage(Name,VertShader,FragShader,mBlitQuad.mVertexDescription,*this) );
	mStages.PushBack( Stage );
	OnStagesChanged();
	Stage->mOnChanged.AddListener( [this](TFilterStage&){OnStagesChanged();} );
	Stage->Reload();
}

void TFilter::LoadFrame(std::shared_ptr<SoyPixels>& Pixels,SoyTime Time)
{
	Soy::Assert( Time.IsValid(), "invalid frame time" );
	
	//	grab the frame (create if nececssary)
	std::shared_ptr<TFilterFrame> Frame;
	{
		auto FrameIt = mFrames.find( Time );
		if ( FrameIt == mFrames.end() )
		{
			Frame.reset( new TFilterFrame() );
			mFrames[Time] = Frame;
		}
		else
		{
			Frame = FrameIt->second;
		}
	}
	
	//	make up a job that holds the pixels to put it into a texture, then run to refresh everything
	auto& Context = GetContext();
	std::shared_ptr<Opengl::TJob> Job( new TOpenglJob_UploadPixels( Pixels, Frame ) );
	Context.PushJob( Job );
	
	//	trigger a run (maybe this will be automatic in future after success?)
	Run( Time );
}

void TFilter::Run(SoyTime Time)
{
	//	grab the frame
	auto FrameIt = mFrames.find( Time );
	Soy::Assert( FrameIt != mFrames.end(), "Frame does not exist" );
	std::shared_ptr<TFilterFrame> Frame = FrameIt->second;
	
	//	make up a job that holds the pixels to put it into a texture, then run to refresh everything
	auto& Context = GetContext();
	std::shared_ptr<Opengl::TJob> Job( new TFilterJobRun( *this, Frame ) );
	Context.PushJob( Job );
}

void TFilter::OnStagesChanged()
{
	//	re-run each
	for ( auto it=mFrames.begin();	it!=mFrames.end();	it++ )
	{
		auto& FrameTime = it->first;
		OnFrameChanged( FrameTime );
	}
}


Opengl::TContext& TFilter::GetContext()
{
	auto* Context = mWindow->GetContext();
	Soy::Assert( Context != nullptr, "Expected opengl window to have a context");
	return *Context;
}



TPlayerFilter::TPlayerFilter(const std::string& Name) :
	TFilter		( Name )
{
	mPitchCorners.PushBack( vec2f(0.0f,0.0f) );
	mPitchCorners.PushBack( vec2f(0.5f,0.0f) );
	mPitchCorners.PushBack( vec2f(0.5f,0.8f) );
	mPitchCorners.PushBack( vec2f(0.0f,0.8f) );
}
	
bool TPlayerFilter::SetUniform(Opengl::TShaderState& Shader,Opengl::TUniform& Uniform)
{
	if ( Uniform.mName == "MaskTopLeft" )
	{
		Shader.SetUniform( Uniform.mName, mPitchCorners[0] );
		return true;
	}
	
	if ( Uniform.mName == "MaskTopRight" )
	{
		Shader.SetUniform( Uniform.mName, mPitchCorners[1] );
		return true;
	}
	
	if ( Uniform.mName == "MaskBottomRight" )
	{
		Shader.SetUniform( Uniform.mName, mPitchCorners[2] );
		return true;
	}
	
	if ( Uniform.mName == "MaskBottomLeft" )
	{
		Shader.SetUniform( Uniform.mName, mPitchCorners[3] );
		return true;
	}
	
	return false;
}


bool TPlayerFilter::SetUniform(TJobParam& Param)
{
	if ( Param.GetKey() == "MaskTopLeft" )
	{
		Soy::Assert( Param.Decode( mPitchCorners[0] ), "Failed to decode" );
		return true;
	}
	
	if ( Param.GetKey() == "MaskTopRight" )
	{
		Soy::Assert( Param.Decode( mPitchCorners[1] ), "Failed to decode" );
		return true;
	}
	
	if ( Param.GetKey() == "MaskBottomRight" )
	{
		Soy::Assert( Param.Decode( mPitchCorners[2] ), "Failed to decode" );
		return true;
	}
	
	if ( Param.GetKey() == "MaskBottomLeft" )
	{
		Soy::Assert( Param.Decode( mPitchCorners[3] ), "Failed to decode" );
		return true;
	}

	return TFilter::SetUniform( Param );
}
