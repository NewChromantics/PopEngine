#include "TFilterStageGatherRects.h"
#include "TFilterStageOpengl.h"
#include <fstream>


/*
static bool RectMatch(float4 a,float4 b)
{
	float NearEdgeDist = 50;
	
	float4 Diff = a-b;
	bool x1 = ( fabs(Diff.x) < NearEdgeDist );
	bool y1 = ( fabs(Diff.y) < NearEdgeDist );
	bool x2 = ( fabs(Diff.z) < NearEdgeDist );
	bool y2 = ( fabs(Diff.w) < NearEdgeDist );
	
	return x1 && y1 && x2 && y2;
}
*/
bool MergeRects(cl_float4& a_cl,cl_float4& b_cl,float NearEdgeDist)
{
	auto a = Soy::ClToVector( a_cl );
	auto b = Soy::ClToVector( b_cl );
	
	vec4f Diff( a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w );
	bool x1 = ( fabs(Diff.x) < NearEdgeDist );
	bool y1 = ( fabs(Diff.y) < NearEdgeDist );
	bool x2 = ( fabs(Diff.z) < NearEdgeDist );
	bool y2 = ( fabs(Diff.w) < NearEdgeDist );
	
	if ( x1 && y1 && x2 && y2 )
	{
		//	expand
		a.x = std::min( a.x, b.x );
		a.y = std::min( a.y, b.y );
		a.z = std::min( a.z, b.z );
		a.w = std::min( a.w, b.w );
		a_cl = Soy::VectorToCl( a );
		return true;
	}
	else
	{
		return false;
	}
}

void TFilterStage_GatherRects::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "GatherRects missing kernel" ) )
		return;
	auto FramePixels = Frame.GetFramePixels(mFilter);
	if ( !Soy::Assert( FramePixels != nullptr, "Frame missing frame pixels" ) )
		return;

	auto FrameWidth = FramePixels->GetWidth();
	auto FrameHeight = FramePixels->GetHeight();

	//	allocate data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_GatherRects() );

	auto& StageData = dynamic_cast<TFilterStageRuntimeData_GatherRects&>( *Data.get() );
	
	StageData.mRects.SetSize( 1000 );
	int RectBufferCount[] = {0};
	auto RectBufferCountArray = GetRemoteArray( RectBufferCount );
	Opencl::TBufferArray<cl_float4> RectBuffer( GetArrayBridge(StageData.mRects), ContextCl, "RectBuffer" );
	Opencl::TBufferArray<cl_int> RectBufferCounter( GetArrayBridge(RectBufferCountArray), ContextCl, "RectBufferCounter" );
	
	auto Init = [this,&Frame,&RectBuffer,&RectBufferCounter,&FrameWidth,&FrameHeight](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
	
		Kernel.SetUniform("Matches", RectBuffer );
		Kernel.SetUniform("MatchesCount", RectBufferCounter );
		Kernel.SetUniform("MatchesMax", size_cast<cl_int>(RectBuffer.GetSize()) );
	
		Iterations.PushBack( vec2x<size_t>(ImageCropLeft,FrameWidth-ImageCropRight) );
		Iterations.PushBack( vec2x<size_t>(ImageCropTop,FrameHeight-ImageCropBottom) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetX", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetY", size_cast<cl_int>(Iteration.mFirst[1]) );
	};
	
	auto Finished = [&StageData,&RectBuffer,&RectBufferCounter](Opencl::TKernelState& Kernel)
	{
		cl_int RectCount = 0;
		Opencl::TSync Semaphore;
		RectBufferCounter.Read( RectCount, Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
		
		StageData.mRects.SetSize( std::min( RectCount, size_cast<cl_int>(RectBuffer.GetSize()) ) );
		RectBuffer.Read( GetArrayBridge(StageData.mRects), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();

	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait();
	
	//	the kernel does some merging, but due to parrallism... it doesn't get them all
	//	the remainder should be smallish, so do it ourselves
	static bool PostMerge = true;
	if ( PostMerge )
	{
		float RectMergeMax = mFilter.GetUniform("RectMergeMax").Decode<float>();
												  
		Soy::TScopeTimer Timer("Gather rects post merge",0,nullptr,true);
		auto& Rects = StageData.mRects;
		auto PreMergeCount = Rects.GetSize();
		for ( ssize_t r=Rects.GetSize()-1;	r>=0;	r-- )
		{
			bool Delete = false;
			for ( int m=0;	m<r;	m++ )
			{
				auto& Rect = Rects[r];
				auto& MatchRect = Rects[m];
				if ( MergeRects( MatchRect, Rect, RectMergeMax ) )
				{
					Delete = true;
					break;
				}
			}
			if ( !Delete )
				continue;
			Rects.RemoveBlock( r, 1 );
		}
		
		static bool DebugMerge = false;
		if ( DebugMerge )
		{
			auto Time = Timer.Stop(false);
			auto RemoveCount = PreMergeCount - Rects.GetSize();
			std::Debug << mName << " post merge removed " << RemoveCount << "/" << PreMergeCount << " rects in " << Time << "ms" << std::endl;
		}
	}
	
	
	static bool DebugAllRects = false;
	if ( DebugAllRects )
	{
		std::Debug << "Read " << StageData.mRects.GetSize() << " rects; ";
		for ( int i=0;	i<StageData.mRects.GetSize();	i++ )
		{
			std::Debug << StageData.mRects[i] << " ";
		}
		std::Debug << std::endl;
	}
}


void TFilterStage_DistortRects::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	auto Kernel = GetKernel(ContextCl);
	Soy::Assert( Kernel != nullptr, "TFilterStage_DistortRects missing kernel" );
	
	//	grab rect data
	auto& RectData = Frame.GetData<TFilterStageRuntimeData_GatherRects>( mMinMaxDataStage );

	//	allocate data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_DistortRects() );
	
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_DistortRects&>( *Data.get() );

	auto& Rects = StageData.mRects;
	Rects.Copy( RectData.mRects );
	Opencl::TBufferArray<cl_float4> RectBuffer( GetArrayBridge(Rects), ContextCl, "Distort rects rect buffer" );
	

	auto Init = [this,&Frame,&RectBuffer,&Rects](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}

		Kernel.SetUniform("MinMaxs", RectBuffer );
		Iterations.PushBack( vec2x<size_t>(0,Rects.GetSize()) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("IndexOffset", size_cast<cl_int>(Iteration.mFirst[0]) );
	};
	
	auto Finished = [&StageData,&Rects,&RectBuffer](Opencl::TKernelState& Kernel)
	{
		Opencl::TSync Semaphore;
		//Kernel.ReadUniform("MinMaxs", GetArrayBridge(Rects), Rects.GetSize() );
		RectBuffer.Read( GetArrayBridge(Rects), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait();
}


void TFilterStage_DrawMinMax::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "TFilterStage_DrawMinMax missing kernel" ) )
		return;
	
	//	generic code
	vec2x<size_t> FragSize;
	TUniformWrapper<std::string> ClearFragStageName("ClearFrag", std::string() );
	std::shared_ptr<SoyPixelsImpl> ClearPixels;
	Frame.SetUniform( ClearFragStageName, ClearFragStageName, mFilter, *this );
	try
	{
		auto& ClearFragStageData = Frame.GetData<TFilterStageRuntimeData&>(ClearFragStageName.mValue);
		ClearPixels = ClearFragStageData.GetPixels( ContextGl );
		if ( ClearPixels )
		{
			FragSize.x = ClearPixels->GetWidth();
			FragSize.y = ClearPixels->GetHeight();
		}
	}
	catch(std::exception& e)
	{
		//	no frag clearing
		ClearPixels.reset();
	}
	if ( !ClearPixels )
	{
		auto FramePixels = Frame.GetFramePixels(mFilter);
		FragSize.x = FramePixels ? FramePixels->GetWidth() : 0;
		FragSize.y = FramePixels ? FramePixels->GetHeight() : 0;
	}

	//	generic code
	auto CreateTexture = [&Frame,&Data,&FragSize,&ClearPixels]
	{
		SoyPixelsMeta OutputPixelsMeta( FragSize.x, FragSize.y, SoyPixelsFormat::RGBA );
		if ( !Data )
		{
			auto* pData = new TFilterStageRuntimeData_ShaderBlit;
			Data.reset( pData );
		}
		auto& StageData = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data );
		
		auto& StageTarget = StageData.mTexture;
		if ( !StageTarget.IsValid() )
		{
			SoyPixelsMeta Meta( OutputPixelsMeta.GetWidth(), OutputPixelsMeta.GetHeight(), OutputPixelsMeta.GetFormat() );
			StageTarget = Opengl::TTexture( Meta, GL_TEXTURE_2D );
		}
		
		static bool ClearToBlack = true;	//	get this as a colour, or FALSE from ClearFlag
		if ( ClearPixels )
		{
			StageTarget.Write( *ClearPixels );
		}
		else if ( ClearToBlack )
		{
			Opengl::TRenderTargetFbo Fbo(StageTarget);
			Fbo.mGenerateMipMaps = false;
			Fbo.Bind();
			Opengl::ClearColour( Soy::TRgb(0,0,0), 0 );
			Fbo.Unbind();
			StageTarget.OnWrite();
		}
	};
	
	auto CreateImageBuffer = [this,&Frame,&Data,&FragSize,&ContextCl]
	{
		SoyPixelsMeta OutputPixelsMeta( FragSize.x, FragSize.y, SoyPixelsFormat::RGBA );
		if ( !Data )
		{
			auto* pData = new TFilterStageRuntimeData_ShaderBlit;
			Data.reset( pData );
		}
		auto& StageData = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data );
		
		auto& StageTarget = StageData.mImageBuffer;
		if ( !StageTarget )
		{
			SoyPixelsMeta Meta( OutputPixelsMeta.GetWidth(), OutputPixelsMeta.GetHeight(), OutputPixelsMeta.GetFormat() );
			Opencl::TSync Semaphore;
			
			std::stringstream BufferName;
			BufferName << this->mName << " stage output";
			std::shared_ptr<Opencl::TBufferImage> ImageBuffer( new Opencl::TBufferImage( OutputPixelsMeta, ContextCl, nullptr, OpenclBufferReadWrite::ReadWrite, BufferName.str(), &Semaphore ) );
			
			Semaphore.Wait();
			//	only assign on success
			StageTarget = ImageBuffer;
		}
	};
	
	static bool MakeTargetAsTexture = true;
	if ( MakeTargetAsTexture )
	{
		Soy::TSemaphore Semaphore;
		auto& ContextGl = mFilter.GetOpenglContext();
		ContextGl.PushJob( CreateTexture, Semaphore );
		Semaphore.Wait();
	}
	else
	{
		Soy::TSemaphore Semaphore;
		ContextCl.PushJob( CreateImageBuffer, Semaphore );
		Semaphore.Wait();
	}
	
	//	load mask
	auto& MinMaxStageData = Frame.GetData<TFilterStageRuntimeData_DistortRects&>(mMinMaxDataStage);
	Opencl::TBufferArray<cl_float4> MinMaxBuffer( GetArrayBridge(MinMaxStageData.mRects), ContextCl, "RectBuffer" );
	
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data );
	
	
	auto Init = [this,&Frame,&StageData,&ContextGl,&MinMaxBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
		
		//	set output depending on what we made
		if ( StageData.mTexture.IsValid(false) )
		{
			//	"frag" is output
			Kernel.SetUniform("Frag", Opengl::TTextureAndContext( StageData.mTexture, ContextGl ), OpenclBufferReadWrite::ReadWrite );
		}
		else if ( StageData.mImageBuffer )
		{
			//	"frag" is output
			Kernel.SetUniform("Frag", *StageData.mImageBuffer );
		}
		else
		{
			throw Soy::AssertException("No pixel output created");
		}

		Kernel.SetUniform("MinMaxs", MinMaxBuffer );
		
		Iterations.PushBack( vec2x<size_t>(0, MinMaxBuffer.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("IndexOffset", size_cast<cl_int>(Iteration.mFirst[0]) );
	};
	
	auto Finished = [&StageData,this,&ContextGl](Opencl::TKernelState& Kernel)
	{
		ofScopeTimerWarning Timer("opencl blit read frag uniform",10);
		
		if ( StageData.mTexture.IsValid(false) )
		{
			Opengl::TTextureAndContext Texture( StageData.mTexture, ContextGl );
			Kernel.ReadUniform("Frag", Texture );
		}
		else if ( StageData.mImageBuffer )
		{
			Kernel.ReadUniform("Frag", *StageData.mImageBuffer );
		}
		else
		{
			throw Soy::AssertException("No pixel output created");
		}
	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait(/*"opencl runner"*/);
}



void TFilterStage_MakeRectAtlas::CreateBlitResources()
{
	auto AllocGeo = [this]
	{
		if ( mBlitGeo )
			return;

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
		mBlitGeo.reset( new Opengl::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex ) );
	};
	
	auto AllocShader = [this]
	{
		if ( mBlitShader )
			return;

		
		auto VertShader =
		"uniform vec4 DstRect;\n"	//	normalised
		"attribute vec2 TexCoord;\n"
		"varying vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);\n"
		"   gl_Position.xy *= DstRect.zw;\n"
		"   gl_Position.xy += DstRect.xy;\n"
		//	move to view space 0..1 to -1..1
		"	gl_Position.xy *= vec2(2,2);\n"
		"	gl_Position.xy -= vec2(1,1);\n"
		"	oTexCoord = vec2(TexCoord.x,1-TexCoord.y);\n"
		"}\n";
		auto FragShader =
		"varying vec2 oTexCoord;\n"
		"uniform sampler2D SrcImage;\n"
		"uniform sampler2D MaskImage;\n"
		"uniform vec4 SrcRect;\n"	//	normalised
		"void main()\n"
		"{\n"
		"	vec2 uv = vec2( oTexCoord.x, 1-oTexCoord.y );\n"
		"   uv *= SrcRect.zw;\n"
		"   uv += SrcRect.xy;\n"
		"	float Mask = texture2D( MaskImage, uv ).w;\n"
		"	vec4 Sample = texture2D( SrcImage, uv );\n"
		"	Sample.w = Mask;\n"
		//	for greyscale we write red
		"	Sample.r = Mask;\n"
		"	if ( Mask < 0.5f )	Sample = vec4(0,0,0,0);\n"
		//"	gl_FragColor = vec4(oTexCoord.x,oTexCoord.y,0,1);\n"
		"	gl_FragColor = Sample;\n"
		"}\n";
		
		auto& OpenglContext = mFilter.GetOpenglContext();
		mBlitShader.reset( new Opengl::TShader( VertShader, FragShader, mBlitGeo->mVertexDescription, "Blit shader", OpenglContext ) );
	};
	
	auto& OpenglContext = mFilter.GetOpenglContext();

	if ( mBlitGeo && mBlitShader )
		return;

	std::lock_guard<std::mutex> Lock( mBlitResourcesLock );
	try
	{
		if ( !mBlitGeo )
		{
			Soy::TSemaphore Semaphore;
			OpenglContext.PushJob( AllocGeo, Semaphore );
			Semaphore.Wait();
		}
	
		if ( !mBlitShader )
		{
			Soy::TSemaphore Semaphore;
			OpenglContext.PushJob( AllocShader, Semaphore );
			Semaphore.Wait();
		}
	}
	catch(...)
	{
		mBlitResourcesLock.unlock();
		throw;
	}
}


void TFilterStage_MakeRectAtlas::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	get data
	auto& RectData = Frame.GetData<TFilterStageRuntimeData_GatherRects>( mRectsStage );
	auto& Rects = RectData.mRects;

	auto& ImageData = Frame.GetData<TFilterStageRuntimeData>( mImageStage );
	auto ImageTexture = ImageData.GetTexture( ContextGl, ContextCl, true );
	
	auto& MaskData = Frame.GetData<TFilterStageRuntimeData>( mMaskStage );
	auto MaskTexture = MaskData.GetTexture( ContextGl, ContextCl, true  );
	
	//	make sure geo & shader are allocated
	CreateBlitResources();

	auto& OpenglContext = mFilter.GetOpenglContext();
	
	//	allocate output
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_MakeRectAtlas() );
	auto& StageData = *dynamic_cast<TFilterStageRuntimeData_MakeRectAtlas*>( Data.get() );
	auto& StageTexture = StageData.mTexture;
	
	//	gr: todo: calc layout first to get optimimum texture... or employ a sprite packer for efficient use
	auto Allocate = [&StageTexture,this]
	{
		if ( StageTexture.IsValid() )
			return;
		auto AtlasWidth = mFilter.GetUniform("AtlasWidth").Decode<int>();
		auto AtlasHeight = mFilter.GetUniform("AtlasHeight").Decode<int>();
		SoyPixelsMeta Meta( AtlasWidth, AtlasHeight, SoyPixelsFormat::Greyscale );
		StageTexture = std::move( Opengl::TTexture( Meta, GL_TEXTURE_2D ) );
	};
	
	
	//	create [temp] fbo to draw to
	std::shared_ptr<Opengl::TFbo> Fbo;
	auto InitFbo = [&StageTexture,&Fbo]
	{
		Fbo.reset( new Opengl::TFbo( StageTexture ) );
		Fbo->Bind();
		Opengl::ClearColour( Soy::TRgb(0,0,0), 0 );
		Fbo->Unbind();
	};
	Soy::TSemaphore AllocSemaphore;
	Soy::TSemaphore ClearSemaphore;
	OpenglContext.PushJob( Allocate, AllocSemaphore );
	OpenglContext.PushJob( InitFbo, ClearSemaphore );
	AllocSemaphore.Wait();
	ClearSemaphore.Wait();

	//	now blit each rect, async by hold all the waits and then waiting for them all at the end
	Array<std::shared_ptr<Soy::TSemaphore>> Waits;
	
	//	walk through each rect, generate a rect-position in the target, make a blit job, and move on!
	vec2x<size_t> Border( 1, 1 );
	size_t RowHeight = 0;
	size_t RectLeft = Border.x;
	size_t RectTop = Border.y;
	auto TargetWidth = StageTexture.GetWidth();
	auto TargetHeight = StageTexture.GetHeight();

	auto& NewNormalisedSourceRects = StageData.mSourceRects;
	auto& NewNormalisedDestRects = StageData.mDestRects;
	Array<Soy::Rectf> RectNewRects;
	
	bool FilledTexture = false;
	for ( int i=0;	i<Rects.GetSize();	i++ )
	{
		auto SourceRect4 = Soy::ClToVector( Rects[i] );
		Soy::Rectf SourceRect( ceil(SourceRect4.x), ceil(SourceRect4.y), ceil(SourceRect4.z-SourceRect4.x), ceil(SourceRect4.w-SourceRect4.y) );
		
		//	work out rect where this will go
		Soy::Rectf DestRect( RectLeft, RectTop, ceil(SourceRect.w), ceil(SourceRect.h) );
		
		if ( DestRect.Right() > TargetWidth )
		{
			//	move to next line
			RectTop += RowHeight + Border.y;
			RowHeight = SourceRect.h;
			RectLeft = 0;
			DestRect = Soy::Rectf( RectLeft, RectTop, SourceRect.w, SourceRect.h );
			
			//	gone off the texture!
			if ( DestRect.Bottom() > TargetHeight )
			{
				FilledTexture = true;
				break;
			}
		}
		
		//	do blit
		auto Blit = [this,&Fbo,&StageData,SourceRect,DestRect,&ImageTexture,&MaskTexture,&NewNormalisedDestRects,&NewNormalisedSourceRects]
		{
			Fbo->Bind();
			auto Shader = mBlitShader->Bind();
			auto SourceRectv = Soy::RectToVector( SourceRect );
			auto DestRectv = Soy::RectToVector( DestRect );
			
			//	normalise rects
			SourceRectv.x /= ImageTexture.GetWidth();
			SourceRectv.y /= ImageTexture.GetHeight();
			SourceRectv.z /= ImageTexture.GetWidth();
			SourceRectv.w /= ImageTexture.GetHeight();
			
			DestRectv.x /= Fbo->GetWidth();
			DestRectv.y /= Fbo->GetHeight();
			DestRectv.z /= Fbo->GetWidth();
			DestRectv.w /= Fbo->GetHeight();
			
			NewNormalisedDestRects.PushBack( Soy::Rectf(DestRectv.x,DestRectv.y,DestRectv.z,DestRectv.w) );
			NewNormalisedSourceRects.PushBack( Soy::Rectf(SourceRectv.x,SourceRectv.y,SourceRectv.z,SourceRectv.w) );
			
			Shader.SetUniform("SrcImage",ImageTexture);
			Shader.SetUniform("MaskImage",MaskTexture);
			Shader.SetUniform("SrcRect",SourceRectv);
			Shader.SetUniform("DstRect",DestRectv);
			mBlitGeo->Draw();
			Fbo->Unbind();
		};
		auto& Semaphore = *Waits.PushBack( std::shared_ptr<Soy::TSemaphore>(new Soy::TSemaphore) );
		OpenglContext.PushJob( Blit, Semaphore );
		
		//	move on for next
		RectLeft = DestRect.Right() + Border.x;
		RowHeight = std::max( RowHeight, size_cast<size_t>(DestRect.h) );
	}
	
	if ( FilledTexture )
		std::Debug << "Warning: MakeRectAtlas overflowed the texture, lost some rects" << std::endl;

	//	wait for all blits to finish
	{
		//Soy::TScopeTimerPrint BlitWaitTimer("MakeRectAtlas blit wait",5);
		for ( int i=0;	i<Waits.GetSize();	i++ )
		{
			auto& Wait = Waits[i];
			Wait->Wait();
		}
	}
	
	//	deffered delete of FBO
	Fbo->Delete( OpenglContext );
}


void TFilterStageRuntimeData_MakeRectAtlas::Shutdown(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	auto DefferedDelete = [this]
	{
		mTexture.Delete();
	};
	
	Soy::TSemaphore Semaphore;
	ContextGl.PushJob( DefferedDelete, Semaphore );
	Semaphore.Wait();
}


TWriteFileStream::TWriteFileStream(const std::string& Filename) :
	SoyWorkerThread		( Soy::GetTypeName(*this) /*+ Filename*/, SoyWorkerWaitMode::Wake )
{
	//	open file
#if defined(USE_STREAM)
	mStream.reset( new std::ofstream( Filename, std::ios::binary|std::ios::out ) );
	if ( !mStream->is_open() )
#else
	mFile = fopen( Filename.c_str(), "wb" );
	if ( !mFile )
#endif
	{
		std::stringstream Error;
		Error << "Failed to open " << Filename << " for writing to";
		throw Soy::AssertException( Error.str() );
	}

	Start();
}

TWriteFileStream::~TWriteFileStream()
{
	//	wait for writes
	mPendingDataLock.lock("~TWriteFileStream");
#if defined(USE_STREAM)
	if ( mStream )
	{
		mStream->close();
		mStream.reset();
	}
#else
	fclose( mFile );
#endif
	mPendingDataLock.unlock();
}

bool TWriteFileStream::CanSleep()
{
	//std::lock_guard<std::mutex> Lock( mPendingDataLock );
	return mPendingData.IsEmpty();
}

bool TWriteFileStream::Iteration()
{
	static size_t BufferSize = 1024 * 1024 * 5;
	mStaticWriteBuffer.Reserve( BufferSize, true );
	auto& Buffer = mStaticWriteBuffer;
	auto BufferBridge = GetArrayBridge(Buffer);

	//	write any pending data
	mPendingDataLock.lock("TWriteFileStream pop");
	//try
	{
		//	pop a chunk of data and continue
		//auto PopSize = std::min( Buffer.MaxSize(), mPendingData.GetDataSize() );
		auto PopSize = std::min( BufferSize, mPendingData.GetDataSize() );
		BufferBridge.PushBackReinterpret( mPendingData.GetArray(), PopSize );
		mPendingData.RemoveBlock( 0, PopSize );
		mPendingDataLock.unlock();
		
	}
	/*
	catch (...)
	{
		mPendingDataLock.unlock();
		throw;
	}
	 */

	//	wasn't anything to write
	if ( Buffer.IsEmpty() )
		return true;
	
#if defined(USE_STREAM)
	auto Stream = mStream;
	
	//	file has been closed
	if ( !Stream )
		return false;

	auto* Data = reinterpret_cast<const char*>( Buffer.GetArray() );
	Stream->write( Data, Buffer.GetDataSize() );
	
	//	check for errors
	if ( Stream->bad() )
	{
		std::Debug << "Failed to write to stream";
		return false;
	}
	
	//	gr: make this flush occur every so often
	Stream->flush();
#else
	size_t Written = 0;
	while ( Written < Buffer.GetDataSize() )
	{
		Written += fwrite( Buffer.GetArray()+Written, 1, Buffer.GetDataSize()-Written, mFile );
	}
#endif
	return true;
}

void TWriteFileStream::PushData(const ArrayBridge<uint8>& Data)
{
	//	closed stream, this write just comes after
#if defined(USE_STREAM)
	if ( !mStream )
		return;
#else 
	if ( !mFile )
		return;
#endif
	
	mPendingDataLock.lock("Waiting for pending data lock");
	try
	{
		mPendingData.PushBackArray( Data );
		mPendingDataLock.unlock();
	}
	catch (...)
	{
		mPendingDataLock.unlock();
		throw;
	}
	
	Wake();
}



TFilterStage_WriteRectAtlasStream::~TFilterStage_WriteRectAtlasStream()
{
	mWriteThreadLock.lock();
	if ( mWriteThread )
	{
		mWriteThread->WaitToFinish();
		mWriteThread.reset();
	}
	mWriteThreadLock.unlock();
}
	
void TFilterStage_WriteRectAtlasStream::PushFrameData(const ArrayBridge<uint8>&& FrameData)
{
	mWriteThreadLock.lock();
	if ( !mWriteThread )
	{
		mWriteThread.reset( new TWriteFileStream( mOutputFilename ) );
	}
	mWriteThreadLock.unlock();

	mWriteThread->PushData( FrameData );
}

void TFilterStage_WriteRectAtlasStream::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	get source data
	auto& AtlasData = Frame.GetData<TFilterStageRuntimeData_MakeRectAtlas>( mAtlasStage );
	
	//	extract the texture data
	SoyPixels AtlasPixels;
	auto ReadPixels = [&AtlasPixels,&AtlasData]
	{
		AtlasData.mTexture.Read( AtlasPixels );
	};
	auto& OpenglContext = mFilter.GetOpenglContext();
	Soy::TSemaphore Sempahore;
	OpenglContext.PushJob( ReadPixels, Sempahore );
	Sempahore.Wait();

	auto& PixelData = AtlasPixels.GetPixelsArray();
	auto& SourceRects = AtlasData.mSourceRects;
	auto& DestRects = AtlasData.mDestRects;
	if ( !Soy::Assert( SourceRects.GetSize() == DestRects.GetSize(), "Source&dest rects size mismatch" ) )
		return;
	
	//	make a header string
	std::stringstream Header;

	Header << "PopTrack=" << SoyTime(true) << ";";
	Header << "Frame=" << Frame.mFrameTime << ";";
	Header << "Rects=";
	for ( int r=0;	r<SourceRects.GetSize();	r++ )
	{
		Header << SourceRects[r] << ">" << DestRects[r] << "#";
	}
	Header << ";";
	Header << "PixelMeta=" << AtlasPixels.GetMeta() << ";";
	Header << "PixelsSize=" << PixelData.GetDataSize() << ";";
	
	Array<uint8> OutputData;
	Soy::StringToArray( Header.str(), GetArrayBridge( OutputData ) );
	
	OutputData.PushBackArray( PixelData );
	
	//	write the data
	PushFrameData( GetArrayBridge(OutputData) );
}

