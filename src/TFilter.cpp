#include "TFilter.h"
#include "TFilterWindow.h"
#include <SoyMath.h>

const char* TFilter::FrameSourceName = "Frame";


class TOpenglJob_UploadPixels : public PopWorker::TJob
{
public:
	TOpenglJob_UploadPixels(const SoyPixelsImpl& Pixels,std::shared_ptr<TFilterFrame>& Frame) :
		mPixels	( Pixels ),
		mFrame	( Frame )
	{
	}
	
	virtual bool		Run(std::ostream& Error);

	const SoyPixelsImpl&			mPixels;
	std::shared_ptr<TFilterFrame>	mFrame;
};




TFilterStage::TFilterStage(const std::string& Name,TFilter& Filter) :
	mName			( Name ),
	mFilter			( Filter )
{
}




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
	Opengl::TContext& Context = mFilter.GetContext();
	
	std::Debug << "Loading shader files for " << this->mName << std::endl;

	//	load files
	std::string VertSrc;
	if ( !Soy::FileToString( mVertFilename, VertSrc ) )
		return;
	
	std::string FragSrc;
	if ( !Soy::FileToString( mFragFilename, FragSrc ) )
		return;

	auto BuildShader = [this,&VertSrc,&FragSrc]
	{
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
		}
		return true;
	};
	
	Soy::TSemaphore Semaphore;
	Context.PushJob( BuildShader, Semaphore );
	Semaphore.Wait("build shader");

	if ( !mShader.IsValid() )
		return;

	std::Debug << "Loaded shader (" << mShader.program.mName << ") okay for " << this->mName << std::endl;
	this->mOnChanged.OnTriggered(*this);
}

bool TFilterStageRuntimeData_ShaderBlit::SetUniform(const std::string& StageName,Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,TFilter& Filter)
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
	mFilter.GetContext().PushJob(ReadPixels,Semaphore);
	Semaphore.Wait();
	
	return ReadSuccess;
}

bool TFilterStageRuntimeData_ReadPixels::SetUniform(const std::string& StageName,Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,TFilter& Filter)
{
	return false;
}


bool TOpenglJob_UploadPixels::Run(std::ostream& Error)
{
	auto& Frame = *mFrame;
	auto& Pixels = mPixels;
	
	//	make texture if it doesn't exist
	if ( !Frame.mFrame.IsValid() )
	{
		SoyPixelsMetaFull Meta( Pixels.GetWidth(), Pixels.GetHeight(), Pixels.GetFormat() );
		Frame.mFrame = Opengl::TTexture( Meta, GL_TEXTURE_2D );
	}
	
	Opengl::TTextureUploadParams Params;
//	Params.mAllowClientStorage = true;
//	Params.mAllowOpenglConversion = false;
//	Params.mAllowCpuConversion = false;
	Frame.mFrame.Copy( Pixels, Params );
	return true;
}


bool TFilterStage_ShaderBlit::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data)
{
	static int DebugColourOffset = 0;
	DebugColourOffset++;
	bool Success = false;
	
	
	auto BlitToTexture = [&Data,&Success,&Frame,this]
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
		
		if ( !mFilter.mBlitQuad.IsValid() )
		{
			std::Debug << "Filter blit quad invalid " << std::endl;
			Success = false;
			return true;
		}
		
		if ( !Data )
		{
			auto* pData = new TFilterStageRuntimeData_ShaderBlit;
			Data.reset( pData );
			auto& StageTarget = pData->mTexture;
			auto& FrameTexture = Frame.mFrame;

			if ( !StageTarget.IsValid() )
			{
				auto Format = SoyPixelsFormat::RGBA;
				SoyPixelsMetaFull Meta( FrameTexture.GetWidth(), FrameTexture.GetHeight(), Format );
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
		{
			auto& StageShader = mShader;
			glDisable(GL_BLEND);
			Opengl::ClearColour( DebugClearColours[(DebugColourOffset)%sizeofarray(DebugClearColours)], 1 );
			Opengl::ClearDepth();
			
			//	write blend RGB but write alpha directly
			glEnable(GL_BLEND);
			glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
			Opengl_IsOkay();
			
			auto Shader = StageShader.Bind();
			if ( Shader.IsValid() )
			{
				//std::Debug << "drawing stage " << StageName << std::endl;
				//	gr: go through uniforms, find any named same as a shader and bind that shaders output
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
				mFilter.mBlitQuad.Draw();
				Success = true;
			}
			else
			{
				std::Debug << __func__ << " stage " << StageName << " has no valid shader" << std::endl;
			}
		}
		RenderTarget.Unbind();
		return true;
	};
	
	Soy::TSemaphore Semaphore;
	mFilter.GetContext().PushJob( BlitToTexture, Semaphore );
	static bool ShowBlitTime = false;
	Semaphore.Wait( ShowBlitTime ? (mName + " blit").c_str() : nullptr );
	
	return Success;
}

bool TFilterFrame::Run(TFilter& Filter)
{
	auto& Frame = *this;
	bool AllSuccess = true;

	
	
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
		
		//	get data pointer for this stage, if it's null the stage should allocate what it needs
		//	gr: need a better lock...
		mStageDataLock.lock();
		auto& pData = Frame.mStageData[StageName];
		mStageDataLock.unlock();
		bool Success = pStage->Execute( *this, pData );
		Frame.mStageData[StageName] = pData;
		
		AllSuccess = AllSuccess && Success;
	}

	//	gr: this sync seems to be good to keep work OUT of the render window's flush
	static bool DoSync = true;
	if ( DoSync )
	{
		auto WaitForSync = []
		{
			Opengl::TSync SyncCommand;
			SyncCommand.Wait();
			return true;
		};
		Soy::TSemaphore Semaphore;
		Filter.GetContext().PushJob( WaitForSync, Semaphore );
		Semaphore.Wait();
	}
	
	//	finish is superceeded by sync
	static bool DoFinish = false;
	if ( DoFinish )
	{
		Soy::TSemaphore Semaphore;
		auto Finish = []
		{
			glFinish();
			return true;
		};
		Filter.GetContext().PushJob( Finish, Semaphore );
		Semaphore.Wait("glfinish");
	}
	
	return AllSuccess;
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
	//	have a priority/overload order;
	//		stage-frame data
	//		frame data
	//		stage data	[done by filter]
	//		filter data
	for ( auto it=mStageData.begin();	it!=mStageData.end();	it++ )
	{
		auto& StageData = it->second;
		auto& StageName = it->first;
		
		if ( !StageData )
			continue;
		
		if ( StageData->SetUniform( StageName, Shader, Uniform, Filter ) )
			return true;
	}

	//	do texture bindings
	if ( SetTextureUniform( Shader, Uniform, mFrame, TFilter::FrameSourceName ) )
		return true;
	
	if ( Filter.SetUniform( Shader, Uniform ) )
		return true;
	
	return false;
}

std::shared_ptr<TFilterStageRuntimeData> TFilterFrame::GetData(const std::string& StageName)
{
	auto it = mStageData.find( StageName );
	if ( it == mStageData.end() )
		return nullptr;
	
	return it->second;
}


TFilter::TFilter(const std::string& Name) :
	TFilterMeta		( Name ),
	mJobThread		( Name + " odd job thread" )
{
	//	create window
	vec2f WindowPosition( 0, 0 );
	vec2f WindowSize( 600, 200 );
	
	mWindow.reset( new TFilterWindow( Name, WindowPosition, WindowSize, *this ) );
	
	static bool CreateSharedContext = true;
	if ( CreateSharedContext )
		mContext = mWindow->GetContext()->CreateSharedContext();
	
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
	GetContext().PushJob( CreateBlitGeo );
	
	//	start odd job thread
	mJobThread.Start();
}

void TFilter::AddStage(const std::string& Name,const TJobParams& Params)
{
	//	make sure stage doesn't exist
	for ( int s=0;	s<mStages.GetSize();	s++ )
	{
		auto& Stage = *mStages[s];
		Soy::Assert( !(Stage == Name), "Stage already exists" );
	}

	//	work out which type it is
	std::shared_ptr<TFilterStage> Stage;
	if ( Params.HasParam("vert") && Params.HasParam("frag") )
	{
		auto VertFilename = Params.GetParamAs<std::string>("vert");
		auto FragFilename = Params.GetParamAs<std::string>("frag");
		Stage.reset( new TFilterStage_ShaderBlit(Name,VertFilename,FragFilename,mBlitQuad.mVertexDescription,*this) );
	}
	else if ( Params.HasParam("readtexture") )
	{
		auto SourceTexture = Params.GetParamAs<std::string>("readtexture");
		Stage.reset( new TFilterStage_ReadPixels(Name,SourceTexture,*this) );
	}
	
	if ( !Stage )
		throw Soy::AssertException("Could not deduce type of stage");

	mStages.PushBack( Stage );
	OnStagesChanged();
	Stage->mOnChanged.AddListener( [this](TFilterStage&){OnStagesChanged();} );
}

void TFilter::LoadFrame(const SoyPixelsImpl& Pixels,SoyTime Time)
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
	std::shared_ptr<PopWorker::TJob> Job( new TOpenglJob_UploadPixels( Pixels, Frame ) );
	
	Soy::TSemaphore Semaphore;
	Context.PushJob( Job, Semaphore );
	Semaphore.Wait("filter load frame");
	OnFrameChanged( Time );
}

std::shared_ptr<TFilterFrame> TFilter::GetFrame(SoyTime Time)
{
	//	grab the frame
	auto FrameIt = mFrames.find( Time );
	if ( FrameIt == mFrames.end() )
		return nullptr;

	std::shared_ptr<TFilterFrame> Frame = FrameIt->second;
	return Frame;
}

bool TFilter::Run(SoyTime Time)
{
	//	grab the frame
	auto Frame = GetFrame(Time);
	if ( !Frame )
		return false;
	
	bool Completed = false;
	
	{
		std::stringstream TimerName;
		TimerName << "filter run " << Time;
		ofScopeTimerWarning Timer(TimerName.str().c_str(),10);
		Completed = Frame->Run( *this );
	}
	
	if ( Completed )
		mOnRunCompleted.OnTriggered( Time );
	
	return Completed;
}


void TFilter::QueueJob(std::function<bool(void)> Function)
{
	mJobThread.PushJob( Function );
}


void TFilter::OnStagesChanged()
{
	static int ApplyToFrameCount = 2;
	
	int Applications = 0;
	
	//	re-run each
	for ( auto it=mFrames.rbegin();	it!=mFrames.rend();	it++ )
	{
		auto& FrameTime = it->first;
		OnFrameChanged( FrameTime );

		if ( ++Applications > ApplyToFrameCount )
			break;
	}
}

void TFilter::OnUniformChanged(const std::string &Name)
{
	OnStagesChanged();
}

TJobParam TFilter::GetUniform(const std::string& Name)
{
	return TJobParam();
}

Opengl::TContext& TFilter::GetContext()
{
	//	use shared context
	if ( mContext )
		return *mContext;
	
	Soy::Assert( mWindow!=nullptr, "window not yet allocated" );
		
	auto* Context = mWindow->GetContext();
	Soy::Assert( Context != nullptr, "Expected opengl window to have a context");
	return *Context;
}



TPlayerFilter::TPlayerFilter(const std::string& Name) :
	TFilter		( Name )
{
	mCylinderPixelWidth = 10;
	mCylinderPixelHeight = 19;
	mPitchCorners.PushBack( vec2f(0.0f,0.0f) );
	mPitchCorners.PushBack( vec2f(0.5f,0.0f) );
	mPitchCorners.PushBack( vec2f(0.5f,0.8f) );
	mPitchCorners.PushBack( vec2f(0.0f,0.8f) );
	
	//	debug extraction
	auto DebugExtractedPlayers = [this](const SoyTime& Time)
	{
		auto Frame = GetFrame( Time );
		if ( !Frame )
			return;
		
		TExtractedFrame ExtractedFrame;
		ExtractPlayers( Time, *Frame, ExtractedFrame );

		std::Debug << "Run extracted " << ExtractedFrame.mPlayers.GetSize() << " players" << std::endl;
	};
	mOnRunCompleted.AddListener( DebugExtractedPlayers );
}

TJobParam TPlayerFilter::GetUniform(const std::string& Name)
{
	if ( Name == "CylinderPixelWidth" )
		return TJobParam( Name, mCylinderPixelWidth );
	
	if ( Name == "CylinderPixelHeight" )
		return TJobParam( Name, mCylinderPixelHeight );
	
	return TFilter::GetUniform(Name);
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
		OnUniformChanged( Param.GetKey() );
		return true;
	}
	
	if ( Param.GetKey() == "MaskTopRight" )
	{
		Soy::Assert( Param.Decode( mPitchCorners[1] ), "Failed to decode" );
		OnUniformChanged( Param.GetKey() );
		return true;
	}
	
	if ( Param.GetKey() == "MaskBottomRight" )
	{
		Soy::Assert( Param.Decode( mPitchCorners[2] ), "Failed to decode" );
		OnUniformChanged( Param.GetKey() );
		return true;
	}
	
	if ( Param.GetKey() == "MaskBottomLeft" )
	{
		Soy::Assert( Param.Decode( mPitchCorners[3] ), "Failed to decode" );
		OnUniformChanged( Param.GetKey() );
		return true;
	}

	return TFilter::SetUniform( Param );
}


void TPlayerFilter::ExtractPlayers(SoyTime FrameTime,TFilterFrame& FilterFrame,TExtractedFrame& ExtractedFrame)
{
	//	grab pixel data
	std::string PlayerDataStage = "foundplayers";
	auto PlayerStageData = FilterFrame.GetData(PlayerDataStage);
	if ( !PlayerStageData )
	{
		std::Debug << "Missing stage data for " << PlayerDataStage << std::endl;
		return;
	}

	auto& FoundPlayerData = *dynamic_cast<TFilterStageRuntimeData_ReadPixels*>( PlayerStageData.get() );
	auto& FoundPlayerPixels = FoundPlayerData.mPixels;
	auto& FoundPlayerPixelsArray = FoundPlayerPixels.GetPixelsArray();
	
	float PlayerWidthNorm = GetUniform("CylinderPixelWidth").Decode<float>();
	float PlayerHeightNorm = GetUniform("CylinderPixelHeight").Decode<float>();
	PlayerWidthNorm /= static_cast<float>(FoundPlayerPixels.GetWidth());
	PlayerHeightNorm /= static_cast<float>(FoundPlayerPixels.GetHeight());
	
	//	get all valid entries
	ExtractedFrame.mTime = FrameTime;
	
	static int MaxPlayerExtractions = 1000;
	auto PixelChannelCount = SoyPixelsFormat::GetChannelCount( FoundPlayerPixels.GetFormat() );
	for ( int i=0;	i<FoundPlayerPixelsArray.GetSize();	i+=PixelChannelCount )
	{
		int ValidityIndex = size_cast<int>( PixelChannelCount-1 );
		auto RedIndex = std::clamped( 0, 0, ValidityIndex-1 );
		int GreenIndex = std::clamped( 0, 1, ValidityIndex-1 );
		int BlueIndex = std::clamped( 0, 2, ValidityIndex-1 );
		
		auto Validity = FoundPlayerPixelsArray[i+ValidityIndex];
		if ( Validity <= 0 )
			continue;
		
		TExtractedPlayer Player;
		Player.mRgb = vec3f( FoundPlayerPixelsArray[i+RedIndex], FoundPlayerPixelsArray[i+GreenIndex], FoundPlayerPixelsArray[i+BlueIndex] );
		Player.mRgb *= vec3f( 1.0f/255.f, 1.0f/255.f, 1.0f/255.f );
		
		vec2f BottomMiddle = FoundPlayerPixels.GetUv( i/PixelChannelCount );
		float x = BottomMiddle.x - PlayerWidthNorm / 2.0f;
		float y = BottomMiddle.y - PlayerHeightNorm;
		Player.mRect = Soy::Rectf( x, y, PlayerWidthNorm, PlayerHeightNorm );

		ExtractedFrame.mPlayers.PushBack( Player );
		
		if ( ExtractedFrame.mPlayers.GetSize() > MaxPlayerExtractions )
		{
			std::Debug << "Stopped player extraction at " << ExtractedFrame.mPlayers.GetSize() << std::endl;
			break;
		}
	}
	
}

void TPlayerFilter::Run(SoyTime FrameTime,TJobParams& ResultParams)
{
	bool AllCompleted = TFilter::Run( FrameTime );
	if ( !Soy::Assert( AllCompleted, "Filter run failed") )
		throw Soy::AssertException("Filter run failed");

	auto FilterFrame = GetFrame(FrameTime);
	if ( !Soy::Assert( FilterFrame!=nullptr, "Missing filter frame") )
		throw Soy::AssertException("Missing filter frame");

	TExtractedFrame ExtractedFrame;
	ExtractPlayers( FrameTime, *FilterFrame, ExtractedFrame );
	
	std::stringstream Output;
	Output << "Extracted " << ExtractedFrame.mPlayers.GetSize() << " players\n";
	Output << Soy::StringJoin( GetArrayBridge(ExtractedFrame.mPlayers), "," );
	ResultParams.AddDefaultParam( Output.str() );
}


std::ostream& operator<<(std::ostream &out,const TExtractedPlayer& in)
{
	out << in.mRect.x << 'x' << in.mRect.y << 'x' << in.mRect.w << 'x' << in.mRect.h;
	return out;
}
