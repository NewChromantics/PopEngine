#include "TFilter.h"
#include "TFilterWindow.h"
#include <SoyMath.h>

#include "TFilterStageOpencl.h"
#include "TFilterStageOpengl.h"


const char* TFilter::FrameSourceName = "Frame";





class TOpenglJob_UploadPixels : public PopWorker::TJob
{
public:
	TOpenglJob_UploadPixels(std::shared_ptr<SoyPixelsImpl>& Pixels,std::shared_ptr<TFilterFrame>& Frame) :
	mPixels	( Pixels ),
	mFrame	( Frame )
	{
	}
	
	virtual bool		Run(std::ostream& Error);
	
	std::shared_ptr<SoyPixelsImpl>	mPixels;
	std::shared_ptr<TFilterFrame>	mFrame;
};



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
	
	Opengl::TTextureUploadParams Params;
	
	//	gr: this works, and is fast... but we exhaust memory quickly (apple storage seems to need huge amounts of memory)
	Params.mAllowClientStorage = false;
	//	Params.mAllowOpenglConversion = false;
	//	Params.mAllowCpuConversion = false;
	
	//	grab already-allocated pixels data to skip a copy
	if ( Params.mAllowClientStorage )
		Frame.mFrame.mClientBuffer = mPixels;
	
	Frame.mFrame.Copy( Pixels, Params );
	return true;
}


TFilterStage::TFilterStage(const std::string& Name,TFilter& Filter) :
	mName			( Name ),
	mFilter			( Filter )
{
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
			return true;
		};
		Filter.GetOpenglContext().PushJob( Finish, Semaphore );
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
	
	static bool CreateSharedContext = false;
	if ( CreateSharedContext )
		mOpenglContext = mWindow->GetContext()->CreateSharedContext();
	
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
		mBlitQuad = Opengl::CreateGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex, GetOpenglContext() );
		
		return true;
	};
	
	//	create blit geometry
	GetOpenglContext().PushJob( CreateBlitGeo );
	
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
	
	if ( Params.HasParam("kernel") && Params.HasParam("cl") )
	{
		auto& Context = GetOpenclContext();
		auto ProgramFilename = Params.GetParamAs<std::string>("cl");
		auto KernelName = Params.GetParamAs<std::string>("kernel");
		Stage.reset( new TFilterStage_OpenclKernel( Name, ProgramFilename, KernelName, *this ) );
	}
	else if ( Params.HasParam("vert") && Params.HasParam("frag") )
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

void TFilter::LoadFrame(std::shared_ptr<SoyPixelsImpl>& Pixels,SoyTime Time)
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
	auto& Context = GetOpenglContext();
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

Opengl::TContext& TFilter::GetOpenglContext()
{
	//	use shared context
	if ( mOpenglContext )
		return *mOpenglContext;
	
	Soy::Assert( mWindow!=nullptr, "window not yet allocated" );
	
	auto* Context = mWindow->GetContext();
	Soy::Assert( Context != nullptr, "Expected opengl window to have a context");
	return *Context;
}


Opencl::TContext& TFilter::GetOpenclContext()
{
	//	make a device
	if ( !mOpenclDevice )
	{
		OpenclDevice::Type Filter = OpenclDevice::GPU;
		
		//	get a list of all the devices
		Array<Opencl::TDeviceMeta> Devices;
		Opencl::GetDevices( GetArrayBridge(Devices), Filter );
		
		for ( int i=0;	i<Devices.GetSize();	i++ )
			std::Debug << "Opencl device #" << i << " " << Devices[i] << std::endl;
		
		mOpenclDevice.reset( new Opencl::TDevice( GetArrayBridge(Devices) ) );
		
		//	now make a context
		mOpenclContext = mOpenclDevice->CreateContext();
	}
		
	return *mOpenclContext;
}


