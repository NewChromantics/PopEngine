#include "TFilterStageGatherRects.h"


bool TFilterStage_GatherRects::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data)
{
	if ( !mKernel )
		return false;

	//	gr: get proper input source for kernel
	if ( !Frame.mFramePixels )
		return false;
	auto FrameWidth = Frame.mFramePixels->GetWidth();
	auto FrameHeight = Frame.mFramePixels->GetHeight();

	//	allocate data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_GatherRects() );

	auto& StageData = *dynamic_cast<TFilterStageRuntimeData_GatherRects*>( Data.get() );
	auto& ContextCl = mFilter.GetOpenclContext();

	StageData.mRects.SetSize( 1000 );
	int RectBufferCount[] = {0};
	auto RectBufferCountArray = GetRemoteArray( RectBufferCount );
	Opencl::TBufferArray<cl_float4> RectBuffer( GetArrayBridge(StageData.mRects), ContextCl );
	Opencl::TBufferArray<cl_int> RectBufferCounter( GetArrayBridge(RectBufferCountArray), ContextCl );
	
	auto Init = [this,&Frame,&RectBuffer,&RectBufferCounter,&FrameWidth,&FrameHeight](Opencl::TKernelState& Kernel,ArrayBridge<size_t>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter ) )
				continue;
		}
		
		Kernel.SetUniform("Matches", RectBuffer );
		Kernel.SetUniform("MatchesCount", RectBufferCounter );
		Kernel.SetUniform("MatchesMax", size_cast<cl_int>(RectBuffer.GetSize()) );
	
		Iterations.PushBack( FrameWidth );
		Iterations.PushBack( FrameHeight );
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
		std::Debug << "Rect count: " << RectCount << std::endl;
		
		StageData.mRects.SetSize( std::min( RectCount, size_cast<cl_int>(RectBuffer.GetSize()) ) );
		RectBuffer.Read( GetArrayBridge(StageData.mRects), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();

	};
	
	//	run opencl
	{
		Soy::TSemaphore Semaphore;
		std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *mKernel, Init, Iteration, Finished ) );
		ContextCl.PushJob( Job, Semaphore );
		try
		{
			Semaphore.Wait();
		}
		catch (std::exception& e)
		{
			std::Debug << "Opencl stage failed: " << e.what() << std::endl;
			return false;
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
	
	return true;
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
		"uniform vec4 SrcRect;\n"	//	normalised
		"void main()\n"
		"{\n"
		"	vec2 uv = oTexCoord;\n"
		"   uv *= SrcRect.zw;\n"
		"   uv += SrcRect.xy;\n"
		//"	gl_FragColor = vec4(oTexCoord.x,oTexCoord.y,0,1);\n"
		"	gl_FragColor = texture2D(SrcImage,uv);\n"
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


bool TFilterStage_MakeRectAtlas::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data)
{
	//	get data
	auto pRectData = Frame.GetData( mRectsStage );
	if ( !Soy::Assert( pRectData != nullptr, "Missing image stage") )
		return false;
	auto& RectData = *dynamic_cast<TFilterStageRuntimeData_GatherRects*>( pRectData.get() );
	auto& Rects = RectData.mRects;

	auto ImageData = Frame.GetData( mImageStage );
	Soy::Assert( ImageData != nullptr, "Missing image stage");
	auto ImageTexture = ImageData->GetTexture();

	//	make sure geo & shader are allocated
	CreateBlitResources();

	auto& OpenglContext = mFilter.GetOpenglContext();
	
	//	allocate output
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_MakeRectAtlas() );
	auto& StageData = *dynamic_cast<TFilterStageRuntimeData_MakeRectAtlas*>( Data.get() );
	auto& StageTexture = StageData.mTexture;
	
	//	gr: todo: calc layout first to get optimimum texture... or employ a sprite packer for efficient use
	auto Allocate = [&StageTexture]
	{
		if ( StageTexture.IsValid() )
			return;
		static SoyPixelsMeta Meta( 1024, 1024, SoyPixelsFormat::RGBA );
		StageTexture = std::move( Opengl::TTexture( Meta, GL_TEXTURE_2D ) );
	};
	
	
	//	create [temp] fbo to draw to
	std::shared_ptr<Opengl::TFbo> Fbo;
	auto InitFbo = [&StageTexture,&Fbo]
	{
		Fbo.reset( new Opengl::TFbo( StageTexture ) );
		Fbo->Bind();
		Opengl::ClearColour( Soy::TRgb(0,1,0) );
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
	vec2x<size_t> Border( 2, 2 );
	size_t RowHeight = 0;
	size_t RectLeft = Border.x;
	size_t RectTop = Border.y;
	auto TargetWidth = StageTexture.GetWidth();
	auto TargetHeight = StageTexture.GetHeight();

	Array<Soy::Rectf> RectNewRects;
	
	bool FilledTexture = false;
	for ( int i=0;	i<Rects.GetSize();	i++ )
	{
		auto SourceRect4 = Soy::ClToVector( Rects[i] );
		Soy::Rectf SourceRect( SourceRect4.x, SourceRect4.y, SourceRect4.z-SourceRect4.x, SourceRect4.w-SourceRect4.y );
		
		//	work out rect where this will go
		Soy::Rectf DestRect( RectLeft, RectTop, SourceRect.w, SourceRect.h );
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
		auto Blit = [this,&Fbo,&StageData,SourceRect,DestRect,&ImageTexture]
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
			
			Shader.SetUniform("SrcImage",ImageTexture);
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

	//	wait for all blits to finish
	{
		Soy::TScopeTimerPrint BlitWaitTimer("MakeRectAtlas blit wait",10);
		for ( int i=0;	i<Waits.GetSize();	i++ )
		{
			auto& Wait = Waits[i];
			Wait->Wait();
		}
	}
	
	//	deffered delete of FBO
	Fbo->Delete( OpenglContext );
	
	return true;
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

