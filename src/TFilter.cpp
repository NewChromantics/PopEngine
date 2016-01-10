#include "TFilter.h"
#include "TFilterWindow.h"
#include <SoyMath.h>
#include <SoyOpencl.h>

#include "TFilterStageOpencl.h"
#include "TFilterStageOpengl.h"
#include "TFilterStageGatherRects.h"
#include "TFilterStageHoughTransform.h"
#include "TFilterStageWritePng.h"
#include "TFilterStagePrevFrameData.h"
#include "TFilterStageWriteGif.h"
#include "TFilterStageWriteAtlasMeta.h"


const char* TFilter::FrameSourceName = "Frame";


namespace Opencl
{
	std::ostream& operator<<(std::ostream &out,const Opencl::TDeviceMeta& in);
}
std::ostream& Opencl::operator<<(std::ostream &out,const Opencl::TDeviceMeta& in)
{
	//(out << in);
	return out;
}



bool SetUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,const TJobParams& Params)
{
	auto Param = Params.GetParam( Uniform.mName );
	if ( !Param.IsValid() )
		return false;
	
	if ( Uniform.mType == Soy::GetTypeName<int>() )
	{
		int v;
		if ( Param.Decode(v) )
			if ( Shader.SetUniform( Uniform.mName, v ) )
				return true;
	}
	
	if ( Uniform.mType == Soy::GetTypeName<float>() )
	{
		float v;
		if ( Param.Decode(v) )
			if ( Shader.SetUniform( Uniform.mName, v ) )
				return true;
	}
	
	if ( Uniform.mType == Soy::GetTypeName<std::string>() )
	{
		std::string v;
		if ( Param.Decode(v) )
			if ( Shader.SetUniform( Uniform.mName, v ) )
				return true;
	}
	
	std::Debug << "Warning: Found uniform " << Uniform.mName << "(" << Uniform.mType << ") in stage params, but type not handled. (Param is " << Param.GetFormat() << ")" << std::endl;
	
	return false;
}


std::shared_ptr<SoyPixelsImpl> TFilterStageRuntimeData_Frame::GetPixels(Opengl::TContext& Context,bool Blocking)
{
	if ( mPixels )
		return mPixels;
	
	//	read from texture
	if ( mTexture )
	{
		auto ReadPixels = [this]
		{
			mPixels.reset( new SoyPixels() );
			mTexture->Read( *mPixels );
		};
		
		if ( Blocking )
		{
			Soy::TSemaphore Semaphore;
			Context.PushJob( ReadPixels, Semaphore );
			
			//	gr: as we may well be inside an opengl job, this will block.
			//		need some auto-dependancy system, without tieing jobs, contexts and semaphores so tightly together
			//		instead, for now caller can fail
			//	if ( !Context inside job right now )
			Semaphore.Wait();
		}
		else
		{
			Context.PushJob( ReadPixels );
		}
	}
	
	return mPixels;
}

Opengl::TTexture TFilterStageRuntimeData_Frame::GetTexture(Opengl::TContext& Context,bool Blocking)
{
	if ( mTexture )
		return *mTexture;
	
	//	make texture from pixels
	if ( mPixels )
	{
		auto WritePixels = [this]
		{
			if ( !mTexture )
				mTexture.reset( new Opengl::TTexture( mPixels->GetMeta(), GL_TEXTURE_2D ) );
		
			Opengl::TTextureUploadParams Params;
			
			//	gr: this works, and is fast... but we exhaust memory quickly (apple storage seems to need huge amounts of memory)
			Params.mAllowClientStorage = false;
			//	Params.mAllowOpenglConversion = false;
			//	Params.mAllowCpuConversion = false;
			
			//	grab already-allocated pixels data to skip a copy
			if ( Params.mAllowClientStorage )
				mTexture->mClientBuffer = mPixels;
			
			mTexture->Write( *mPixels, Params );
		};
		
		if ( Blocking )
		{
			Soy::TSemaphore Semaphore;
			Context.PushJob( WritePixels, Semaphore );
		
			//	gr: as we may well be inside an opengl job, this will block.
			//		need some auto-dependancy system, without tieing jobs, contexts and semaphores so tightly together
			//		instead, for now caller can fail
			//	if ( !Context inside job right now )
			Semaphore.Wait();
		}
		else
		{
			Context.PushJob( WritePixels );
		}
	}
	
	if ( !mTexture )
		return Opengl::TTexture();
	
	return *mTexture;
}


void TFilterStageRuntimeData_Frame::Shutdown(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	shutdown our data
	auto DefferedDelete = [this]
	{
		this->mTexture.reset();
	};
	Soy::TSemaphore Semaphore;
	ContextGl.PushJob( DefferedDelete, Semaphore );
	Semaphore.Wait();
}

	
TFilterStage::TFilterStage(const std::string& Name,TFilter& Filter,const TJobParams& StageParams) :
	mName			( Name ),
	mFilter			( Filter ),
	mUniforms		( StageParams )
{
}


bool TFilterStage::SetUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform)
{
	return ::SetUniform( Shader, Uniform, mUniforms );
}

TFilterFrame::~TFilterFrame()
{
	mRunLock.lock("~TFilterFrame");
	mStageDataLock.lock("~TFilterFrame");
	//std::Debug << "Frame destruction " << this->mFrameTime << std::endl;

	//	shutdown all the datas
	for ( auto it=mStageData.begin();	it!=mStageData.end();	)
	{
		auto& pData = it->second;
		if ( pData )
			pData->Shutdown( *mContextGl, *mContextCl );

		pData.reset();
		it = mStageData.erase( it );
	}
	
	mContextCl.reset();
	mContextGl.reset();
}


Opengl::TTexture TFilterFrame::GetFrameTexture(TFilter& Filter,bool Blocking)
{
	//	this should have been allocated before run
	auto& Data = GetData<TFilterStageRuntimeData_Frame>( TFilter::FrameSourceName );
	auto& Context = Filter.GetOpenglContext();
	return Data.GetTexture( Context, Blocking );
}

std::shared_ptr<SoyPixelsImpl> TFilterFrame::GetFramePixels(TFilter& Filter,bool Blocking)
{
	//	this should have been allocated before run
	auto& Data = GetData<TFilterStageRuntimeData_Frame>( TFilter::FrameSourceName );
	auto& Context = Filter.GetOpenglContext();
	return Data.GetPixels( Context, Blocking );
}


bool TFilterFrame::Run(TFilter& Filter,const std::string& Description,std::shared_ptr<Opengl::TContext>& ContextGl,std::shared_ptr<Opencl::TContext>& ContextCl)
{
	mContextCl = ContextCl;
	mContextGl = ContextGl;
	
	auto& Frame = *this;
	bool AllSuccess = true;

	
	Array<SoyTime> StageTimings;
	
	//	run through the shader chain
	for ( int s=0;	s<Filter.mStages.GetSize();	s++ )
	{
		auto pStage = Filter.mStages[s];
		std::stringstream StageDesc;
		if ( pStage )
			StageDesc << pStage->mName;
		else
			StageDesc << "#" << s;

		auto& TimerTime = StageTimings.PushBack(SoyTime());
		std::stringstream TimerName;
		TimerName << Description << " stage " << StageDesc.str();
		Soy::TScopeTimer Timer( TimerName.str().c_str(), 0, nullptr, true );
	
		if ( !pStage )
		{
			std::Debug << "Warning: Filter " << Filter.mName << " " << StageDesc.str() << " is null" << std::endl;
			continue;
		}
		
		//	get data pointer for this stage, if it's null the stage should allocate what it needs
		//	gr: need a better lock...
		auto& StageName = pStage->mName;
		mStageDataLock.lock( std::string("pre-Run get Stage Data ") + StageName );
		auto& pData = Frame.mStageData[StageName];
		mStageDataLock.unlock();
		
		bool Success = true;
		try
		{
			pStage->Execute( *this, pData, *mContextGl, *mContextCl );

			//	make dev snapshots :)
			Filter.PushDevSnapshot(pData,*pStage);
			
			mStageDataLock.lock( std::string("post-Run place stageData ") + StageName );
			Frame.mStageData[StageName] = pData;
			mStageDataLock.unlock();
			
		}
		catch (std::exception& e)
		{
			Success = false;
			std::Debug << "Stage " << StageName << " failed: " << e.what() << std::endl;
		}

		TimerTime = Timer.Stop(false);
		AllSuccess = AllSuccess && Success;
	}
	
	//	report timing
	auto& TimingDebug = std::Debug;
	TimingDebug << Description << " ";
	for ( int s=0;	s<Filter.mStages.GetSize();	s++ )
	{
		auto pStage = Filter.mStages[s];
		std::stringstream StageDesc;
		if ( pStage )
			StageDesc << pStage->mName;
		else
			StageDesc << "#" << s;
		TimingDebug << StageDesc.str() << " " << StageTimings[s].mTime << "ms ";
	}
	TimingDebug << std::endl;
	
	
	//	gr: this sync seems to be good to keep work OUT of the render window's flush
	static bool DoSync = false;
	if ( DoSync )
	{
		auto WaitForSync = []
		{
			Opengl::TSync SyncCommand;
			SyncCommand.Wait();
		};
		Soy::TSemaphore Semaphore;
		Filter.GetOpenglContext().PushJob( WaitForSync, Semaphore );
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
		};
		Filter.GetOpenglContext().PushJob( Finish, Semaphore );
		Semaphore.Wait("glfinish");
	}
	
	return AllSuccess;
}


bool TFilterFrame::SetTextureUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,const SoyPixelsMeta& Meta,const std::string& TextureName,TFilter& Filter,const TJobParams& StageUniforms)
{
	bool TextureIsThis = false;
	TextureIsThis |= Soy::StringBeginsWith( Uniform.mName, TextureName, true );
	
	if ( !TextureIsThis )
		return false;
	
	//	is there a suffix?
	std::string Suffix;
	Suffix = Uniform.mName.substr( TextureName.length(), std::string::npos );
	
	if ( Suffix == "_TexelWidthHeight" )
	{
		vec2f Size( 1.0f / static_cast<float>( Meta.GetWidth()), 1.0f / static_cast<float>(Meta.GetHeight()) );
		Shader.SetUniform_s( Uniform.mName, Size );
		return true;
	}
	
	if ( Suffix == "_PixelWidthHeight" )
	{
		vec2f Size( Meta.GetWidth(), Meta.GetHeight() );
		Shader.SetUniform_s( Uniform.mName, Size );
		return true;
	}
	
	return false;
}



bool TFilterFrame::SetUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,TFilter& Filter,TFilterStage& Stage)
{
	//	have a priority/overload order;
	//		stage-frame data
	//		frame data
	//		stage data
	//		filter data
	for ( auto it=mStageData.begin();	it!=mStageData.end();	it++ )
	{
		auto& StageData = it->second;
		auto& StageName = it->first;
		
		if ( !StageData )
			continue;
		
		if ( StageData->SetUniform( StageName, Shader, Uniform, Filter, Stage.mUniforms, Filter.GetOpenglContext() ) )
			return true;
	}
	
	if ( Stage.SetUniform( Shader, Uniform ) )
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
	mOddJobThread		( Name + " odd job thread" ),
	mDevSnapshotThread		( Name + " dev snapshots" ),
	//mDevSnapshotsDir	( "../DevSnapshots" ),
	mCurrentOpenclContext	( 0 )
{
	mWindow.reset( new TFilterWindow( Name, *this ) );
	
	static bool CreateSharedContext = false;
	if ( CreateSharedContext )
		mOpenglContext = mWindow->GetContext()->CreateSharedContext();

	CreateBlitGeo(false);
	
	mOddJobThread.Start();
	mDevSnapshotThread.Start();
}

void TFilter::CreateBlitGeo(bool Blocking)
{
	
	auto DoCreateBlitGeo = [this]
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
		Array<size_t> Indexes;
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
		UvAttrib.SetType(GL_FLOAT);
		UvAttrib.mIndex = 0;	//	gr: does this matter?
		UvAttrib.mArraySize = 2;
		UvAttrib.mElementDataSize = sizeof( Mesh.mVertexes[0].uv );
		
		Array<uint8> MeshData;
		MeshData.PushBackReinterpret( Mesh );
		mBlitQuad.reset( new Opengl::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex ) );
	};
	
	//	create blit geometry
	if ( Blocking )
	{
		Soy::TSemaphore Semaphore;
		GetOpenglContext().PushJob( DoCreateBlitGeo, Semaphore );
		Semaphore.Wait();
	}
	else
	{
		GetOpenglContext().PushJob( DoCreateBlitGeo );
	}
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
	//	gr: needs a better factory system
	std::shared_ptr<TFilterStage> Stage;
	

	//	gr: start of "factory"
	auto StageType = Params.GetParamAs<std::string>("StageType");
	
	//	construct an opencl context [early for debugging]
	CreateOpenclContexts();

	if ( StageType == "GetTruthLines" )
	{
		auto VertUniform = Params.GetParamAs<std::string>("VertLinesUniform");
		auto HorzUniform = Params.GetParamAs<std::string>("HorzLinesUniform");
		Stage.reset( new TFilterStage_GetTruthLines( Name, VertUniform, HorzUniform, *this, Params ) );
	}
	else if ( StageType == "WritePng" )
	{
		auto Filename = Params.GetParamAs<std::string>("Filename");
		auto ImageStage = Params.GetParamAs<std::string>("ExportStage");
		Stage.reset( new TFilterStage_WritePng( Name, Filename, ImageStage, *this, Params ) );
	}
	else if ( StageType == "MakeRectAtlas" )
	{
		auto RectsSource = Params.GetParamAs<std::string>("Rects");
		auto ImageSource = Params.GetParamAs<std::string>("Image");
		auto MaskStage = Params.GetParamAs<std::string>("Mask");
		
		Stage.reset( new TFilterStage_MakeRectAtlas( Name, RectsSource, ImageSource, MaskStage, *this, Params ) );
	}
	else if ( StageType == "WriteRectAtlasStream" )
	{
		auto AtlasStage = Params.GetParamAs<std::string>("AtlasStage");
		auto FilenameParam = Params.GetParam("Filename");
		if ( !FilenameParam.IsValid() )
			throw Soy::AssertException("No filename specified for atlas output");
		auto Filename = FilenameParam.Decode<std::string>();
		
		Stage.reset( new TFilterStage_WriteRectAtlasStream( Name, AtlasStage, Filename, *this, Params ) );
	}
	else if ( StageType == "WriteGif" )
	{
		Stage.reset( new TFilterStage_WriteGif( Name, *this, Params ) );
	}
	else if ( StageType == "WriteAtlasMeta" )
	{
		Stage.reset( new TFilterStage_WriteAtlasMeta( Name, *this, Params ) );
	}
	else if ( StageType == "DistortRects" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto MinMaxDataStage = Params.GetParamAs<std::string>("MinMaxDataStage");
		
		Stage.reset( new TFilterStage_DistortRects( Name, ProgramFilename, KernelName, MinMaxDataStage, *this, Params ) );
	}
	else if ( StageType == "DrawHoughLines" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto HoughLineDataStageName = Params.GetParamAs<std::string>("HoughLineData");
		
		Stage.reset( new TFilterStage_DrawHoughLines( Name, ProgramFilename, KernelName, HoughLineDataStageName, *this, Params ) );
	}
	else if ( StageType == "DrawHoughCorners" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto HoughCornerDataStageName = Params.GetParamAs<std::string>("HoughCornerData");
		
		Stage.reset( new TFilterStage_DrawHoughCorners( Name, ProgramFilename, KernelName, HoughCornerDataStageName, *this, Params ) );
	}
	else if ( StageType == "DrawHoughLinesDynamic" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto HoughDataStageName = Params.GetParamAs<std::string>("HoughData");
		
		Stage.reset( new TFilterStage_DrawHoughLinesDynamic( Name, ProgramFilename, KernelName, HoughDataStageName, *this, Params ) );
	}
	else if ( StageType == "ExtractHoughLines" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto HoughDataStageName = Params.GetParamAs<std::string>("HoughData");
		
		Stage.reset( new TFilterStage_ExtractHoughLines( Name, ProgramFilename, KernelName, HoughDataStageName, *this, Params ) );
	}
	else if ( StageType == "ExtractHoughCorners" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto HoughLinesStageName = Params.GetParamAs<std::string>("HoughLineData");
		
		Stage.reset( new TFilterStage_ExtractHoughCorners( Name, ProgramFilename, KernelName, HoughLinesStageName, *this, Params ) );
	}
	else if ( StageType == "GetHoughCornerHomographys" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto HoughCornerData = Params.GetParamAs<std::string>("HoughCornerData");
		
		Stage.reset( new TFilterStage_GetHoughCornerHomographys( Name, ProgramFilename, KernelName, HoughCornerData, *this, Params ) );
	}
	else if ( StageType == "GetHoughLineHomographys" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto HoughLineData = Params.GetParamAs<std::string>("HoughLineData");
		auto TruthLineData = Params.GetParamAs<std::string>("TruthLineData");
		
		Stage.reset( new TFilterStage_GetHoughLineHomographys( Name, ProgramFilename, KernelName, HoughLineData, TruthLineData, *this, Params ) );
	}
	else if ( StageType == "ScoreHomographys" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto HomographyData = Params.GetParamAs<std::string>("HomographyData");
		auto HoughCornerData = Params.GetParamAs<std::string>("CornerData");
		auto TruthCornerData = Params.GetParamAs<std::string>("TruthCornerData");
		
		Stage.reset( new TFilterStage_ScoreHoughCornerHomographys( Name, ProgramFilename, KernelName, HomographyData, HoughCornerData, TruthCornerData, *this, Params ) );
	}
	else if ( StageType == "DrawHomographyCorners" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto CornerData = Params.GetParamAs<std::string>("CornerData");
		auto TruthCornerData = Params.GetParamAs<std::string>("TruthCornerData");
		auto HomographyData = Params.GetParamAs<std::string>("HomographyData");
		
		Stage.reset( new TFilterStage_DrawHomographyCorners( Name, ProgramFilename, KernelName, CornerData, TruthCornerData, HomographyData, *this, Params ) );
	}
	else if ( StageType == "DrawMaskOnFrame" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		auto MaskStage = Params.GetParamAs<std::string>("MaskStage");
		auto HomographyData = Params.GetParamAs<std::string>("HomographyData");
		
		Stage.reset( new TFilterStage_DrawMaskOnFrame( Name, ProgramFilename, KernelName, MaskStage, HomographyData, *this, Params ) );
	}
	else if ( StageType == "GatherHoughLines" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		
		Stage.reset( new TFilterStage_GatherHoughTransforms( Name, ProgramFilename, KernelName, *this, Params ) );
	}
	else if ( StageType == "GatherRects" )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		
		Stage.reset( new TFilterStage_GatherRects( Name, ProgramFilename, KernelName, *this, Params ) );
	}
	else if ( StageType == "PrevFrameData" )
	{
		Stage.reset( new TFilterStage_PrevFrameData( Name, *this, Params ) );
	}
	else if ( Params.HasParam("kernel") && Params.HasParam("cl") )
	{
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		
		Stage.reset( new TFilterStage_OpenclBlit( Name, ProgramFilename, KernelName, *this, Params ) );
	}
	else if ( Params.HasParam("vert") && Params.HasParam("frag") )
	{
		CreateBlitGeo(true);
		auto VertFilename = Params.GetParamAs<std::string>("vert");
		auto FragFilename = Params.GetParamAs<std::string>("frag");
		Stage.reset( new TFilterStage_ShaderBlit( Name, VertFilename, FragFilename, mBlitQuad->mVertexDescription, *this, Params ) );
	}
	else if ( Params.HasParam("Filename") )
	{
		auto Filename = Params.GetParamAs<std::string>("Filename");
		Stage.reset( new TFilterStage_ReadPng( Name, Filename, *this, Params ) );
	}

	if ( !Stage )
		throw Soy::AssertException("Could not deduce type of stage");

	mStages.PushBack( Stage );
	OnStagesChanged();
	Stage->mOnChanged.AddListener( [this](TFilterStage&){OnStagesChanged();} );
}

void TFilter::LoadFrame(std::shared_ptr<SoyPixelsImpl>& Pixels,SoyTime Time)
{
	Soy::Assert( Time.IsValid(), "invalid frame time" );
	bool IsNewFrame = false;
	
	//	grab the frame (create if nececssary)
	std::shared_ptr<TFilterFrame> Frame;
	std::stringstream LoadLockName;
	LoadLockName << "Load lock " << Time;
	mFramesLock.lock( LoadLockName.str() );
	{
		auto FrameIt = mFrames.find( Time );
		if ( FrameIt == mFrames.end() )
		{
			Frame.reset( new TFilterFrame(Time) );
			mFrames[Time] = Frame;
			IsNewFrame = true;
		}
		else
		{
			Frame = FrameIt->second;
		}
	}
	mFramesLock.unlock();
	
	//	gr: here we may have a problem where the original pixel buffer is getting overriden by the movie reader?
	//	make source stage data and assign pixels. texture will be generated when first requested
	
	{
		std::shared_ptr<TFilterStageRuntimeData_Frame> SourceData = Frame->AllocData<TFilterStageRuntimeData_Frame>( TFilter::FrameSourceName );
		//	gr: getting null ptr here... dynamic cast fail?
		if ( !SourceData )
		{
			//	gr: nothing catching throw!
			//throw Soy::AssertException("Failed to alloc frame data");
			return;
		}
		
		SourceData->mPixels = Pixels;
		
		OnFrameChanged( Time );

		if ( IsNewFrame )
			mOnFrameAdded.OnTriggered( Time );
	}
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

TFilterFrame& TFilter::GetPrevFrame(SoyTime CurrentFrame)
{
	static bool OldestFrame = true;
	
	SoyTime ClosestPrevFrame;
	for ( auto it=mFrames.begin();	it!=mFrames.end();	it++ )
	{
		auto& FrameTime = it->first;
		if ( FrameTime >= CurrentFrame )
			continue;
		
		if ( OldestFrame )
		{
			if ( FrameTime > ClosestPrevFrame && ClosestPrevFrame.IsValid() )
				continue;
		}
		else
		{
			if ( FrameTime < ClosestPrevFrame )
				continue;
		}
		ClosestPrevFrame = FrameTime;
	}
	
	auto pFrame = GetFrame( ClosestPrevFrame );
	{
		std::stringstream Error;
		Error << "No previous frame to " << CurrentFrame;
		Soy::Assert( pFrame!=nullptr, Error.str() );
	}
	
	return *pFrame;
}

bool TFilter::DeleteFrame(SoyTime FrameTime)
{
	mFramesLock.lock("Waiting for frame lock before pop");

	//	pop from list
	auto FrameIt = mFrames.find( FrameTime );
	if ( FrameIt == mFrames.end() )
	{
		mFramesLock.unlock();
		std::stringstream Error;
		Error << "Frame " << FrameTime << " doesn't exist";
		throw Soy::AssertException( Error.str() );
	}
	auto pFrame = FrameIt->second;

	//	still being run!
	if ( !pFrame->mRunLock.try_lock() )
	{
		std::Debug << "Tried to delete frame " << FrameTime << " which was still running" << std::endl;
		mFramesLock.unlock();
		return false;
	}
	//	unlock the successfull try
	pFrame->mRunLock.unlock();

	//	delete
	mFrames.erase( FrameIt );
	mFramesLock.unlock();
	
	pFrame.reset();
	//std::Debug << "Deleted frame " << FrameTime << std::endl;
	return true;
}


bool TFilter::Run(SoyTime Time)
{
	//	grab the frame
	auto Frame = GetFrame(Time);
	if ( !Frame )
		return false;
	
	bool Completed = false;

	std::stringstream RunLockName;
	RunLockName << "Run lock " << Time;
	Frame->mRunLock.lock(RunLockName.str());
	try
	{
		auto ContextCl = PickNextOpenclContext();
		GetOpenglContext();
		auto ContextGl = mOpenglContext;

		//std::Debug << "Started run: " << Time << " on " << ContextCl->GetDevice().mName << std::endl;
		std::stringstream TimerName;
		TimerName << "filter run " << Time << " on " << ContextCl->GetDevice().mName;
		//ofScopeTimerWarning Timer(TimerName.str().c_str(),10);
		
		Completed = Frame->Run( *this, TimerName.str(), ContextGl, ContextCl );
		Frame->mRunLock.unlock();
	}
	catch(...)
	{
		Frame->mRunLock.unlock();
		throw;
	}
	if ( Completed )
		mOnRunCompleted.OnTriggered( Time );
	
	return Completed;
}


void TFilter::QueueJob(std::function<void(void)> Function)
{
	mOddJobThread.PushJob( Function );
}

void TFilter::PushDevSnapshot(std::shared_ptr<TFilterStageRuntimeData> StageData,const TFilterStage& Stage)
{
	if ( mDevSnapshotsDir.empty() )
		return;
	if ( !StageData )
		return;

	std::string StageName = Stage.mName;
	if ( Stage.mUniforms.HasParam("NoSnapshot" ) )
		return;
	
	//	copy var
	auto SavePixels = [this,StageData,StageName]
	{
		auto& ContextGl = GetOpenglContext();
		//	dev snapshots :)
		auto Pixels = StageData->GetPixels( ContextGl );
		if ( !Pixels )
			return;

		std::stringstream Filename;
		Filename << mDevSnapshotsDir << "/" << SoyTime(true) << "_" << StageName << ".png";
		Array<char> PngData;
		Pixels->GetPng( GetArrayBridge(PngData) );
		Soy::ArrayToFile( GetArrayBridge(PngData), Filename.str() );

	};
	
	//	to reduce load, just save shots when we're idle
	static int MaxSnapShotQueue = 1;
	if ( mDevSnapshotThread.GetJobCount() > MaxSnapShotQueue )
		return;
	
	mDevSnapshotThread.PushJob( SavePixels );
}


void TFilter::OnStagesChanged()
{
	static int ApplyToFrameCount = 5;
	
	int Applications = 0;

	//	grab frames to re-run
	Array<SoyTime> ReRunFrames;
	
	mFramesLock.lock("OnStagesChanged gather frames");
	for ( auto it=mFrames.rbegin();	it!=mFrames.rend();	it++ )
	{
		auto& FrameTime = it->first;
		ReRunFrames.PushBack( FrameTime );
		if ( ++Applications > ApplyToFrameCount )
			break;
	}
	mFramesLock.unlock();

	//	re-run each
	for ( int f=0;	f<ReRunFrames.GetSize();	f++ )
	{
		auto& FrameTime = ReRunFrames[f];
		OnFrameChanged( FrameTime );
	}
}

void TFilter::OnUniformChanged(const std::string &Name)
{
	OnStagesChanged();
}

bool TFilter::SetUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform)
{
	return ::SetUniform( Shader, Uniform, mUniforms );
}

bool TFilter::SetUniform(TJobParam& Param,bool TriggerRerun)
{
	mUniforms.AddParam( Param );
	return true;
}

TJobParam TFilter::GetUniform(const std::string& Name)
{
	return mUniforms.GetParam( Name );
}

Opengl::TContext& TFilter::GetOpenglContext()
{
	auto Context = GetOpenglContextPtr();
	Soy::Assert( Context != nullptr, "Expected opengl window to have a context");
	return *Context;
}


std::shared_ptr<Opengl::TContext> TFilter::GetOpenglContextPtr()
{
	//	use shared context
	if ( mOpenglContext )
		return mOpenglContext;
	
	Soy::Assert( mWindow!=nullptr, "window not yet allocated" );
	auto Context = mWindow->GetContext();
	Soy::Assert( Context != nullptr, "Expected opengl window to have a context");
	mOpenglContext = Context;
	return mOpenglContext;
}



void TFilter::CreateOpenclContexts()
{
	auto& OpenglContext = GetOpenglContext();
	
	std::lock_guard<std::mutex> Lock(mOpenclContextLock);
	
	//	make a device
	if ( mOpenclDevices.IsEmpty() )
	{
		static OpenclDevice::Type Filter = OpenclDevice::GPU;
		//OpenclDevice::Type Filter = OpenclDevice::CPU;
		
		//	get a list of all the devices
		Array<Opencl::TDeviceMeta> Devices;
		Opencl::GetDevices( GetArrayBridge(Devices), Filter );
		
		for ( int i=0;	i<Devices.GetSize();	i++ )
		{
			static bool InterpolateWithOpengl = true;
			auto& Device = Devices[i];
			
			try
			{
				BufferArray<Opencl::TDeviceMeta,1> ThisContextDevices;
				ThisContextDevices.PushBack( Device );
				std::shared_ptr<Opencl::TDevice> pDevice( new Opencl::TDevice( GetArrayBridge(ThisContextDevices), InterpolateWithOpengl ? &OpenglContext : nullptr ) );
			
				//	now make a context
				auto Context = pDevice->CreateContextThread("Filter opencl thread");
				if ( !Context )
					continue;

				std::Debug << "Created Opencl context for device #" << i << " " << Device << std::endl;
				
				mOpenclDevices.PushBack( pDevice );
				mOpenclContexts.PushBack( Context );
			}
			catch ( std::exception& e)
			{
				std::Debug << "Failed to create opencl context context for device #" << i << " " << Device << ": " << e.what() << std::endl;
				continue;
			}
		}
	}
}


void TFilter::GetOpenclContexts(ArrayBridge<std::shared_ptr<Opencl::TContext>>&& Contexts)
{
	CreateOpenclContexts();
	Contexts.Copy( mOpenclContexts );
}

std::shared_ptr<Opencl::TContext> TFilter::PickNextOpenclContext()
{
	mCurrentOpenclContext++;
	CreateOpenclContexts();
	return mOpenclContexts[ mCurrentOpenclContext % mOpenclContexts.GetSize() ];
}

bool TFilterStageRuntimeData::CanHandleUniform(const Soy::TUniform& Uniform,const std::string& ParentStageName,const TJobParams& CurrentStageUniforms)
{
	std::string TargetName = Uniform.mName;
	bool SetFromStageUniformValue = false;
	
	//	special case, uniform is an image, and value is us!
	if ( Uniform.mType == "image2d_t" || Uniform.mType == "GL_SAMPLER_2D" )
	{
		auto StageParam = CurrentStageUniforms.GetParam( Uniform.mName );
		if ( StageParam.IsValid() )
		{
			auto StageParamValue = StageParam.Decode<std::string>();
			if ( Soy::StringBeginsWith( StageParamValue, ParentStageName, true ) )
			{
				TargetName = StageParamValue;
				SetFromStageUniformValue = true;
			}
		}
	}
	
	//if ( !Soy::StringBeginsWith( TargetName, ParentStageName, true ) )
	if ( TargetName != ParentStageName )
		return false;
	/*
	//	is there a suffix?
	std::string Suffix;
	Suffix = TargetName.substr( TextureName.length(), std::string::npos );
	*/
	return true;
}


bool TFilterStageRuntimeData::SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,TFilter& Filter,const TJobParams& StageUniforms,Opengl::TContext& ContextGl)
{
	if ( CanHandleUniform( Uniform, StageName, StageUniforms  ) )
	{
		auto Texture = GetTexture();
		if ( Texture.IsValid(false) )
		{
			if ( Shader.SetUniform_s( Uniform.mName, Opengl::TTextureAndContext( Texture, Filter.GetOpenglContext() ) ) )
				return true;
		}

		auto Pixels = GetPixels(ContextGl);
		if ( Pixels )
		{
			if ( Shader.SetUniform_s( Uniform.mName, *Pixels ) )
				return true;
		}
	}
	
	return false;
}

std::shared_ptr<SoyPixelsImpl> TFilterStageRuntimeData::GetPixels(Opengl::TContext& ContextGl)
{
	auto Texture = GetTexture();
	if ( !Texture.IsValid(false) )
		return nullptr;
	
	std::shared_ptr<SoyPixelsImpl> Pixels;
	auto ReadTexture = [&Pixels,&Texture]
	{
		Pixels.reset( new SoyPixels );
		Texture.Read( *Pixels );
	};
	Soy::TSemaphore Semaphore;
	ContextGl.PushJob( ReadTexture, Semaphore );
	Semaphore.Wait();
	Pixels->Flip();

	return Pixels;
}



