#include "TFilterStageHoughTransform.h"

#include "TFilterStageOpencl.h"
#include "TFilterStageOpengl.h"




void TFilterStage_GatherHoughTransforms::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "TFilterStage_GatherHoughTransforms missing kernel" ) )
		return;
	auto FramePixels = Frame.GetFramePixels(mFilter);
	if ( !Soy::Assert( FramePixels != nullptr, "Frame missing frame pixels" ) )
		return;

	auto FrameWidth = FramePixels->GetWidth();
	auto FrameHeight = FramePixels->GetHeight();

	//	allocate data
	if ( !Data )
	{
		Data.reset( new TFilterStageRuntimeData_GatherHoughTransforms() );
		auto& StageData = dynamic_cast<TFilterStageRuntimeData_GatherHoughTransforms&>( *Data.get() );
		auto& Angles = StageData.mAngles;
		auto& Distances = StageData.mDistances;
		
		TUniformWrapper<float> AngleFrom("HoughAngleFrom",0);
		TUniformWrapper<float> AngleTo("HoughAngleTo",179);
		TUniformWrapper<float> AngleStep("HoughAngleStep",0.3333f);
		TUniformWrapper<float> DistanceStep("HoughDistanceStep",2);
		
		Frame.SetUniform( AngleFrom, AngleFrom, mFilter, *this );
		Frame.SetUniform( AngleTo, AngleTo, mFilter, *this );
		Frame.SetUniform( AngleStep, AngleStep, mFilter, *this );
		Frame.SetUniform( DistanceStep, DistanceStep, mFilter, *this );

		//	gr: remove this and generate max from the image
		static float DistanceFrom = -900;
		float DistanceTo = -DistanceFrom;
		
		for ( float a=AngleFrom;	a<=AngleTo;	a+=AngleStep )
			Angles.PushBack( a );
		for ( float Dist=DistanceFrom;	Dist<=DistanceTo;	Dist+=DistanceStep )
			Distances.PushBack( Dist );
	}

	auto& StageData = dynamic_cast<TFilterStageRuntimeData_GatherHoughTransforms&>( *Data.get() );
	auto& Angles = StageData.mAngles;
	auto& Distances = StageData.mDistances;
	auto& AngleXDistances = StageData.mAngleXDistances;
	
	AngleXDistances.SetSize( Angles.GetSize() * Distances.GetSize() );
	AngleXDistances.SetAll(0);
	
	Opencl::TBufferArray<cl_int> AngleXDistancesBuffer( GetArrayBridge(AngleXDistances), ContextCl, "mAngleXDistances" );
	Opencl::TBufferArray<cl_float> AnglesBuffer( GetArrayBridge(Angles), ContextCl, "mAngles" );
	Opencl::TBufferArray<cl_float> DistancesBuffer( GetArrayBridge(Distances), ContextCl, "mDirections" );

	auto Init = [this,&Frame,&AngleXDistancesBuffer,&AnglesBuffer,&DistancesBuffer,&FrameWidth,&FrameHeight](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
	
		Kernel.SetUniform("AngleXDistances", AngleXDistancesBuffer );
		Kernel.SetUniform("AngleDegs", AnglesBuffer );
		Kernel.SetUniform("Distances", DistancesBuffer );
		Kernel.SetUniform("AngleCount", size_cast<cl_int>(AnglesBuffer.GetSize()) );
		Kernel.SetUniform("DistanceCount", size_cast<cl_int>(DistancesBuffer.GetSize()) );
	
		Iterations.PushBack( vec2x<size_t>(ImageCropLeft,FrameWidth-ImageCropRight) );
		Iterations.PushBack( vec2x<size_t>(ImageCropTop,FrameHeight-ImageCropBottom) );
		Iterations.PushBack( vec2x<size_t>(0,AnglesBuffer.GetSize()) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetX", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetY", size_cast<cl_int>(Iteration.mFirst[1]) );
		Kernel.SetUniform("OffsetAngle", size_cast<cl_int>(Iteration.mFirst[2]) );
	};
	
	auto Finished = [&StageData,&AngleXDistances,&AngleXDistancesBuffer](Opencl::TKernelState& Kernel)
	{
		//	read back the histogram
		Opencl::TSync Semaphore;
		AngleXDistancesBuffer.Read( GetArrayBridge(AngleXDistances), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait();
	
	
	//	find the biggest number (needed for rendering later, but just testing here
	static bool DebugHistogramCount = false;
	if ( DebugHistogramCount )
	{
		int TotalDistanceCount = 0;
		for ( int ad=0;	ad<AngleXDistances.GetSize();	ad++ )
		{
			auto d = ad % Distances.GetSize();
			auto a = ad / Distances.GetSize();

			std::Debug << "Angle[" << Angles[a] << "][" << Distances[d] << "] x" << AngleXDistances[ad] << std::endl;
			TotalDistanceCount += AngleXDistances[ad];
		}
		std::Debug << "Total distance count: " << TotalDistanceCount << std::endl;
	}
}



void TFilterStage_DrawHoughLinesDynamic::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "TFilterStage_DrawHoughLinesDynamic missing kernel" ) )
		return;
	auto FramePixels = Frame.GetFramePixels(mFilter,true);
	if ( !Soy::Assert( FramePixels != nullptr, "Frame missing frame pixels" ) )
		return;
	
	auto& HoughStageData = Frame.GetData<TFilterStageRuntimeData_GatherHoughTransforms>( mHoughDataStage );
	auto& Angles = HoughStageData.mAngles;
	auto& Distances = HoughStageData.mDistances;
	auto& AnglesXDistances = HoughStageData.mAngleXDistances;
	Opencl::TBufferArray<cl_int> AngleXDistancesBuffer( GetArrayBridge(AnglesXDistances), ContextCl, "mAngleXDistances" );
	Opencl::TBufferArray<cl_float> AnglesBuffer( GetArrayBridge(Angles), ContextCl, "mAngles" );
	Opencl::TBufferArray<cl_float> DistancesBuffer( GetArrayBridge(Distances), ContextCl, "mDirections" );

	TUniformWrapper<std::string> ClearFragStageName("ClearFrag", std::string() );
	try
	{
		//auto& ClearFragStageData = Frame.GetData<TFilterStageRuntimeData&>(ClearFragStageName.mName);
	}
	catch(std::exception& e)
	{
		//	no frag clearing
	}

	
	//	write straight to a texture
	//	gr: changed to write to a buffer image, if anything wants a texture, it'll convert on request
	if ( !Data )
	{
		auto CreateTexture = [&Frame,&Data,&FramePixels]
		{
			SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
			auto* pData = new TFilterStageRuntimeData_ShaderBlit;
			Data.reset( pData );
			
			auto& StageTarget = pData->mTexture;
			if ( !StageTarget.IsValid() )
			{
				SoyPixelsMeta Meta( OutputPixelsMeta.GetWidth(), OutputPixelsMeta.GetHeight(), OutputPixelsMeta.GetFormat() );
				StageTarget = Opengl::TTexture( Meta, GL_TEXTURE_2D );
				
				static bool ClearWithSource = true;
				if ( ClearWithSource )
				{
					StageTarget.Write( *FramePixels );
				}
				
				//	clear it
				static bool ClearTarget = false;
				if ( ClearTarget )
				{
					Opengl::TRenderTargetFbo Fbo(StageTarget);
					Fbo.mGenerateMipMaps = false;
					Fbo.Bind();
					Opengl::ClearColour( Soy::TRgb(0,0,0), 0 );
					Fbo.Unbind();
					StageTarget.OnWrite();
				}
			}
		};
		
		auto CreateImageBuffer = [this,&Frame,&Data,&FramePixels,&ContextCl]
		{
			SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
			auto* pData = new TFilterStageRuntimeData_ShaderBlit;
			Data.reset( pData );
			
			auto& StageTarget = pData->mImageBuffer;
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
	}
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data );
	
	
	auto Init = [this,&Frame,&StageData,&ContextGl,&Angles,&Distances,&AngleXDistancesBuffer,&AnglesBuffer,&DistancesBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//ofScopeTimerWarning Timer("opencl blit init",40);
		
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
			
			//	maybe surpress this until we need it... or only warn once
			static bool DebugUnsetUniforms = false;
			if ( DebugUnsetUniforms )
				std::Debug << "Warning; unset uniform " << Uniform.mName << std::endl;
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
		
		Kernel.SetUniform("AngleXDistances", AngleXDistancesBuffer );
		Kernel.SetUniform("AngleDegs", AnglesBuffer );
		Kernel.SetUniform("AngleCount", size_cast<cl_int>(AnglesBuffer.GetSize()) );
		Kernel.SetUniform("Distances", DistancesBuffer );
		Kernel.SetUniform("DistanceCount", size_cast<cl_int>(DistancesBuffer.GetSize()) );
	
		Iterations.PushBack( vec2x<size_t>(0, Angles.GetSize() ) );
		Iterations.PushBack( vec2x<size_t>(0, Distances.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetAngle", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetDistance", size_cast<cl_int>(Iteration.mFirst[1]) );
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


void TFilterStage_DrawHoughLines::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "TFilterStage_DrawHoughLines missing kernel" ) )
		return;
	auto FramePixels = Frame.GetFramePixels(mFilter,true);
	if ( !Soy::Assert( FramePixels != nullptr, "Frame missing frame pixels" ) )
		return;
	
	TUniformWrapper<std::string> ClearFragStageName("ClearFrag", std::string() );
	std::shared_ptr<SoyPixelsImpl> ClearPixels;
	Frame.SetUniform( ClearFragStageName, ClearFragStageName, mFilter, *this );
	try
	{
		auto& ClearFragStageData = Frame.GetData<TFilterStageRuntimeData&>(ClearFragStageName.mValue);
		ClearPixels = ClearFragStageData.GetPixels( ContextGl );
	}
	catch(std::exception& e)
	{
		//	no frag clearing
		ClearPixels.reset();
	}
	
	
	//	write straight to a texture
	//	gr: changed to write to a buffer image, if anything wants a texture, it'll convert on request
	//	gr: for re-iterating over the same image we wnat to re-clear
	auto CreateTexture = [&Frame,&Data,&FramePixels,&ClearPixels]
	{
		SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
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
	
	auto CreateImageBuffer = [this,&Frame,&Data,&FramePixels,&ContextCl]
	{
		SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
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
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data );
	
	auto& HoughStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughLines>( mHoughLineDataStage );
	auto& HoughLines = HoughStageData.mHoughLines;
	Opencl::TBufferArray<cl_float8> HoughLinesBuffer( GetArrayBridge(HoughLines), ContextCl, "HoughLines" );
	
	auto Init = [this,&Frame,&StageData,&ContextGl,&HoughLinesBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//ofScopeTimerWarning Timer("opencl blit init",40);
		
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
			
			//	maybe surpress this until we need it... or only warn once
			static bool DebugUnsetUniforms = false;
			if ( DebugUnsetUniforms )
				std::Debug << "Warning; unset uniform " << Uniform.mName << std::endl;
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
		
		Kernel.SetUniform("HoughLines", HoughLinesBuffer );
		
		Iterations.PushBack( vec2x<size_t>(0, HoughLinesBuffer.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetIndex", size_cast<cl_int>(Iteration.mFirst[0]) );
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



void TFilterStage_ExtractHoughLines::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "TFilterStage_DrawHoughLines missing kernel" ) )
		return;
	auto FramePixels = Frame.GetFramePixels(mFilter,true);
	if ( !Soy::Assert( FramePixels != nullptr, "Frame missing frame pixels" ) )
		return;
	
	auto& HoughStageData = Frame.GetData<TFilterStageRuntimeData_GatherHoughTransforms>( mHoughDataStage );
	auto& Angles = HoughStageData.mAngles;
	auto& Distances = HoughStageData.mDistances;
	auto& AnglesXDistances = HoughStageData.mAngleXDistances;
	Opencl::TBufferArray<cl_int> AngleXDistancesBuffer( GetArrayBridge(AnglesXDistances), ContextCl, "mAngleXDistances" );
	Opencl::TBufferArray<cl_float> AnglesBuffer( GetArrayBridge(Angles), ContextCl, "mAngles" );
	Opencl::TBufferArray<cl_float> DistancesBuffer( GetArrayBridge(Distances), ContextCl, "mDirections" );

	
	Array<cl_float8> AllLines;
	AllLines.SetSize(400);
	int LineBufferCount[] = {0};
	auto LineBufferCountArray = GetRemoteArray( LineBufferCount );
	Opencl::TBufferArray<cl_float8> LineBuffer( GetArrayBridge(AllLines), ContextCl, "LineBuffer" );
	Opencl::TBufferArray<cl_int> LineBufferCounter( GetArrayBridge(LineBufferCountArray), ContextCl, "LineBufferCounter" );

	
	auto Init = [this,&Frame,&LineBuffer,&LineBufferCounter,&Angles,&Distances,&AngleXDistancesBuffer,&AnglesBuffer,&DistancesBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
		
		Kernel.SetUniform("AngleXDistances", AngleXDistancesBuffer );
		Kernel.SetUniform("AngleDegs", AnglesBuffer );
		Kernel.SetUniform("AngleCount", size_cast<cl_int>(AnglesBuffer.GetSize()) );
		Kernel.SetUniform("Distances", DistancesBuffer );
		Kernel.SetUniform("DistanceCount", size_cast<cl_int>(DistancesBuffer.GetSize()) );
		Kernel.SetUniform("Matches", LineBuffer );
		Kernel.SetUniform("MatchesCount", LineBufferCounter );
		Kernel.SetUniform("MatchesMax", size_cast<cl_int>(LineBuffer.GetSize()) );
	
		
		Iterations.PushBack( vec2x<size_t>(0, Angles.GetSize() ) );
		Iterations.PushBack( vec2x<size_t>(0, Distances.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetAngle", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetDistance", size_cast<cl_int>(Iteration.mFirst[1]) );
	};
	
	auto Finished = [&AllLines,&LineBuffer,&LineBufferCounter](Opencl::TKernelState& Kernel)
	{
		cl_int LineCount = 0;
		Opencl::TSync Semaphore;
		LineBufferCounter.Read( LineCount, Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
		
		if ( LineCount > LineBuffer.GetSize() )
		{
			std::Debug << "Extracted " << LineCount << "/" << LineBuffer.GetSize() << std::endl;
		}
		
		AllLines.SetSize( std::min( LineCount, size_cast<cl_int>(LineBuffer.GetSize()) ) );
		LineBuffer.Read( GetArrayBridge(AllLines), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait(/*"opencl runner"*/);
	
	
	//	evaluate if lines are vertical or horizontal
	//	todo: auto gen this by histogramming, find median (vertical) opposite (horizontal)
	float VerticalAngle = 0;
	int BestAngleXDistance = 0;
	for ( int axd=0;	axd<AnglesXDistances.GetSize();	axd++ )
	{
		if ( AnglesXDistances[axd] <= AnglesXDistances[BestAngleXDistance] )
			continue;
		BestAngleXDistance = axd;
	}
	VerticalAngle = Angles[BestAngleXDistance / Distances.GetSize()];

	//	allocate final data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_ExtractHoughLines() );
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ExtractHoughLines&>( *Data.get() );

	auto CompareLineScores = [](const cl_float8& a,const cl_float8& b)
	{
		auto& aScore = a.s[6];
		auto& bScore = b.s[6];
		if ( aScore > bScore )	return -1;
		if ( aScore < bScore )	return 1;
		return 0;
	};
	
	//	gr: the threshold is LESS than 45 (90 degrees) because viewing angles are skewed, lines are rarely ACTUALLY perpendicualr
	TUniformWrapper<float> VerticalThresholdUniform("VerticalThreshold", 10.f );
	Frame.SetUniform( VerticalThresholdUniform, VerticalThresholdUniform, mFilter, *this );

	//	copy lines whilst sorting & modify flag to say vertical or horizontal
	SortArrayLambda<cl_float8> FinalLines( GetArrayBridge(StageData.mHoughLines), CompareLineScores );
	for ( int i=0;	i<AllLines.GetSize();	i++ )
	{
		auto& Line = AllLines[i];
		auto& Angle = Angles[Line.s[4]];
		auto& IsVertical = Line.s[7];
		
		float Threshold = VerticalThresholdUniform.mValue;
		
		float Diff = Angle - VerticalAngle;
		while ( Diff < -90.f )
			Diff += 180.f;
		while ( Diff > 90.f )
			Diff -= 180.f;
		Diff = fabsf(Diff);
		
		if ( Diff <= Threshold )
			IsVertical = 1;
		else
			IsVertical = 0;
		
		static bool DebugVerticalTest = true;
		if ( DebugVerticalTest )
			std::Debug << Angle << " -> " << VerticalAngle << " diff= " << Diff << " vertical: " << IsVertical << " (threshold: " << Threshold << ")" << std::endl;
		
		//	add to sorted list
		FinalLines.Push( Line );
	}
	
	static bool DebugExtractedHoughLines = true;
	if ( DebugExtractedHoughLines )
	{
		auto& Lines = StageData.mHoughLines;
		std::Debug << "Extracted x" << Lines.GetSize() << " lines" << std::endl;
		for ( int i=0;	i<Lines.GetSize();	i++ )
		{
			auto Line = Lines[i];
			float Score = Line.s[6];
			float Vertical = Line.s[7];
			std::Debug << "#" << i << " Score: " << Score << ", Vertical: " << Vertical << std::endl;
		}
	}
}






void TFilterStage_ExtractHoughCorners::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, std::string(__func__) + " missing kernel" ) )
		return;
	
	auto& HoughStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughLines>( mHoughLineStage );
	auto& Lines = HoughStageData.mHoughLines;
	Opencl::TBufferArray<cl_float8> LinesBuffer( GetArrayBridge(Lines), ContextCl, "mHoughLines" );
	
	//	allocate data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_ExtractHoughCorners() );
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ExtractHoughCorners&>( *Data.get() );
	
	Array<cl_float4> Corners;
	Corners.SetSize( Lines.GetSize() * Lines.GetSize() );
	Opencl::TBufferArray<cl_float4> CornersBuffer( GetArrayBridge(Corners), ContextCl, "mCorners" );
	
	
	auto Init = [this,&Frame,&LinesBuffer,&CornersBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
		
		int LineCount = size_cast<int>(LinesBuffer.GetSize());
		Kernel.SetUniform("HoughLines", LinesBuffer );
		Kernel.SetUniform("HoughLineCount", LineCount );
		Kernel.SetUniform("HoughCorners", CornersBuffer );
		
		Iterations.PushBack( vec2x<size_t>(0, LineCount ) );
		Iterations.PushBack( vec2x<size_t>(0, LineCount ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetHoughLineAIndex", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetHoughLineBIndex", size_cast<cl_int>(Iteration.mFirst[1]) );
	};
	
	auto Finished = [&StageData,&CornersBuffer,&Corners](Opencl::TKernelState& Kernel)
	{
		Opencl::TSync Semaphore;
		CornersBuffer.Read( GetArrayBridge(Corners), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait(/*"opencl runner"*/);
	
	
	//	get all the valid corners, and merge duplicates
	TUniformWrapper<float> CornerMergeDistanceUniform("CornerMergeDistance", 2.f );
	Frame.SetUniform( CornerMergeDistanceUniform, CornerMergeDistanceUniform, mFilter, *this );
	auto& CornerMergeDistance = CornerMergeDistanceUniform.mValue;
	
	static bool DebugSkippedCorners = false;
	int SkippedCount = 0;
	auto& FinalCorners = StageData.mCorners;
	for ( int i=0;	i<Corners.GetSize();	i++ )
	{
		auto Corner4 = Corners[i];
		auto Corner = Soy::VectorToMatrix( Soy::ClToVector( Corner4 ) );
		auto Score = Corner.z();
		if ( Score <= 0 )
			continue;
		
		//	look for merge
		bool Skip = false;
		for ( int e=0;	!Skip && e<FinalCorners.GetSize();	e++ )
		{
			auto OldCorner = Soy::VectorToMatrix( Soy::ClToVector( FinalCorners[e] ) );
			auto Distance = (OldCorner - Corner).Length();
			if ( Distance < CornerMergeDistance )
			{
				if ( DebugSkippedCorners )
					std::Debug << "skipped corner " << Corner4 << " against old " << Soy::ClToVector( FinalCorners[e] ) << " (" << Distance << "px)" << std::endl;
				Skip = true;
				SkippedCount++;
			}
		}
		if ( Skip )
			continue;
		
		FinalCorners.PushBack( Corner4 );
	}
	
	std::Debug << "Extracted x" << FinalCorners.GetSize() << " hough corners: ";
	std::Debug.PushStreamSettings();
	for ( int i=0;	i<FinalCorners.GetSize();	i++ )
	{
		auto Corner4 = Soy::ClToVector( FinalCorners[i] );
		static int Precision = 0;
		std::Debug << std::fixed << std::setprecision( Precision ) << Corner4.x << "x" << Corner4.y << ",";// << Corner4.z << std::endl;
	}
	std::Debug.PopStreamSettings();
	std::Debug << std::endl;
}




void TFilterStage_DrawHoughCorners::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "TFilterStage_DrawHoughLines missing kernel" ) )
		return;
	auto FramePixels = Frame.GetFramePixels(mFilter,true);
	if ( !Soy::Assert( FramePixels != nullptr, "Frame missing frame pixels" ) )
		return;
	
	TUniformWrapper<std::string> ClearFragStageName("ClearFrag", std::string() );
	std::shared_ptr<SoyPixelsImpl> ClearPixels;
	Frame.SetUniform( ClearFragStageName, ClearFragStageName, mFilter, *this );
	try
	{
		auto& ClearFragStageData = Frame.GetData<TFilterStageRuntimeData&>(ClearFragStageName.mValue);
		ClearPixels = ClearFragStageData.GetPixels( ContextGl );
	}
	catch(std::exception& e)
	{
		//	no frag clearing
		ClearPixels.reset();
	}
	
	
	//	write straight to a texture
	//	gr: changed to write to a buffer image, if anything wants a texture, it'll convert on request
	//	gr: for re-iterating over the same image we wnat to re-clear
	auto CreateTexture = [&Frame,&Data,&FramePixels,&ClearPixels]
	{
		SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
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
	
	auto CreateImageBuffer = [this,&Frame,&Data,&FramePixels,&ContextCl]
	{
		SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
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
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data );
	
	auto& HoughCornerStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughCorners>( mHoughCornerDataStage );
	auto& HoughCorners = HoughCornerStageData.mCorners;
	Opencl::TBufferArray<cl_float4> HoughCornersBuffer( GetArrayBridge(HoughCorners), ContextCl, "HoughCorners" );
	
	auto Init = [this,&Frame,&StageData,&ContextGl,&HoughCornersBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//ofScopeTimerWarning Timer("opencl blit init",40);
		
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
		
		Kernel.SetUniform("HoughCorners", HoughCornersBuffer );
		
		Iterations.PushBack( vec2x<size_t>(0, HoughCornersBuffer.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetIndex", size_cast<cl_int>(Iteration.mFirst[0]) );
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






void TFilterStage_GetHoughCornerHomographys::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, std::string(__func__) + " missing kernel" ) )
		return;
	
	//	get truth corners if we haven't set them up
	if ( mTruthCorners.IsEmpty() )
	{
		TUniformWrapper<std::string> TruthCornersUniform("TruthCorners",std::string());
		if ( !Frame.SetUniform( TruthCornersUniform, TruthCornersUniform, mFilter, *this ) )
			throw Soy::AssertException("Missing TruthCorners uniform");
		
		auto PushVec = [this](const std::string& Part,const char& Delin)
		{
			vec2f Coord;
			Soy::StringToType( Coord, Part );
			mTruthCorners.PushBack( Soy::VectorToCl(Coord) );
			return true;
		};
		Soy::StringSplitByMatches( PushVec, TruthCornersUniform.mValue, "," );
		Soy::Assert( !mTruthCorners.IsEmpty(), "Failed to extract any truth corners");
	}
	
	auto& HoughCornerStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughCorners>( mHoughCornerStage );
	auto& Corners = HoughCornerStageData.mCorners;
	Opencl::TBufferArray<cl_float4> CornersBuffer( GetArrayBridge(Corners), ContextCl, "Corners" );
	Opencl::TBufferArray<cl_float2> TruthCornersBuffer( GetArrayBridge(mTruthCorners), ContextCl, "mTruthCorners" );
	
	//	make ransac random indexes
	Array<cl_int4> CornerIndexes;
	Array<cl_int4> TruthIndexes;
	//	gr: ransac that favours good corners?
	static int RansacSize = 50;
	for ( int r=0;	r<RansacSize;	r++ )
	{
		auto& SampleCornerIndexs = CornerIndexes.PushBack();
		auto& SampleTruthIndexs = TruthIndexes.PushBack();
		for ( int n=0;	n<4;	n++ )
		{
			int CornerIndex = rand() % CornersBuffer.GetSize();
			int TruthIndex = rand() % TruthCornersBuffer.GetSize();
			SampleCornerIndexs.s[n] = CornerIndex;
			SampleTruthIndexs.s[n] = TruthIndex;
		}
	}
	
	Opencl::TBufferArray<cl_int4> CornerIndexesBuffer( GetArrayBridge(CornerIndexes), ContextCl, "CornerIndexesBuffer" );
	Opencl::TBufferArray<cl_int4> TruthIndexesBuffer( GetArrayBridge(TruthIndexes), ContextCl, "TruthIndexesBuffer" );

	//	allocate data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_GetHoughCornerHomographys() );
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_GetHoughCornerHomographys&>( *Data.get() );
	
	Array<cl_float16> Homographys;
	Array<cl_float16> HomographyInvs;
	Homographys.SetSize( CornerIndexesBuffer.GetSize() * TruthIndexesBuffer.GetSize() );
	HomographyInvs.SetSize( CornerIndexesBuffer.GetSize() * TruthIndexesBuffer.GetSize() );
	Opencl::TBufferArray<cl_float16> HomographysBuffer( GetArrayBridge(Homographys), ContextCl, "Homographys" );
	Opencl::TBufferArray<cl_float16> HomographyInvsBuffer( GetArrayBridge(HomographyInvs), ContextCl, "HomographyInvs" );
	
	
	auto Init = [this,&Frame,&CornersBuffer,&TruthCornersBuffer,&HomographysBuffer,&HomographyInvsBuffer,&CornerIndexesBuffer,&TruthIndexesBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
		
		int MatchIndexesCount = size_cast<int>(CornerIndexesBuffer.GetSize());
		Kernel.SetUniform("MatchIndexes", CornerIndexesBuffer );
		Kernel.SetUniform("TruthIndexes", TruthIndexesBuffer );
		Kernel.SetUniform("MatchIndexesCount", MatchIndexesCount );
		Kernel.SetUniform("MatchCorners", CornersBuffer );
		Kernel.SetUniform("TruthCorners", TruthCornersBuffer );
		Kernel.SetUniform("Homographys", HomographysBuffer );
		Kernel.SetUniform("HomographysInv", HomographyInvsBuffer );
		
		Iterations.PushBack( vec2x<size_t>(0, CornerIndexesBuffer.GetSize() ) );
		Iterations.PushBack( vec2x<size_t>(0, TruthIndexesBuffer.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("MatchIndexOffset", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("TruthIndexOffset", size_cast<cl_int>(Iteration.mFirst[1]) );
	};
	
	auto Finished = [&StageData,&HomographysBuffer,&Homographys,&HomographyInvsBuffer,&HomographyInvs](Opencl::TKernelState& Kernel)
	{
		Opencl::TSync Semaphore;
		HomographysBuffer.Read( GetArrayBridge(Homographys), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
		Opencl::TSync Semaphore2;
		HomographyInvsBuffer.Read( GetArrayBridge(HomographyInvs), Kernel.GetContext(), &Semaphore2 );
		Semaphore2.Wait();
	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait(/*"opencl runner"*/);
	
	
	//	get all the valid homographys
	auto& FinalHomographys = StageData.mHomographys;
	auto& FinalHomographyInvs = StageData.mHomographyInvs;
	for ( int i=0;	i<Homographys.GetSize();	i++ )
	{
		//	if it's all zero's its invalid
		auto& Homography = Homographys[i];
		auto& HomographyInv = HomographyInvs[i];
		
		float Sum = 0;
		for ( int s=0;	s<sizeofarray(Homography.s);	s++ )
			Sum += Homography.s[s];

		if ( Sum == 0 )
			continue;
		
		FinalHomographys.PushBack( Homography );
		FinalHomographyInvs.PushBack( HomographyInv );
	}
	
	std::Debug << "Extracted x" << FinalHomographys.GetSize() << " homographys: ";
	static bool DebugExtractedHomographys = false;
	if ( DebugExtractedHomographys )
	{
		std::Debug.PushStreamSettings();
		for ( int i=0;	i<FinalHomographys.GetSize();	i++ )
		{
			auto& Homography = FinalHomographys[i];
			static int Precision = 5;
			std::Debug << std::endl;
			for ( int s=0;	s<sizeofarray(Homography.s);	s++ )
				std::Debug << std::fixed << std::setprecision( Precision ) << Homography.s[s] << " x ";
		}
		std::Debug.PopStreamSettings();
	}
	std::Debug << std::endl;
}





void TFilterStage_ScoreHoughCornerHomographys::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, std::string(__func__) + " missing kernel" ) )
		return;
	
	//	get truth corners if we haven't set them up
	if ( mTruthCorners.IsEmpty() )
	{
		TUniformWrapper<std::string> TruthCornersUniform("TruthCorners",std::string());
		if ( !Frame.SetUniform( TruthCornersUniform, TruthCornersUniform, mFilter, *this ) )
			throw Soy::AssertException("Missing TruthCorners uniform");
		
		auto PushVec = [this](const std::string& Part,const char& Delin)
		{
			vec2f Coord;
			Soy::StringToType( Coord, Part );
			mTruthCorners.PushBack( Soy::VectorToCl(Coord) );
			return true;
		};
		Soy::StringSplitByMatches( PushVec, TruthCornersUniform.mValue, "," );
		Soy::Assert( !mTruthCorners.IsEmpty(), "Failed to extract any truth corners");
	}
	
	auto& CornerStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughCorners>( mCornerDataStage );
	auto& HomographyStageData = Frame.GetData<TFilterStageRuntimeData_GetHoughCornerHomographys>( mHomographyDataStage );
	auto& Corners = CornerStageData.mCorners;
	//	dont modify original!
	auto Homographys = HomographyStageData.mHomographys;
	auto HomographyInvs = HomographyStageData.mHomographyInvs;
	Opencl::TBufferArray<cl_float4> CornersBuffer( GetArrayBridge(Corners), ContextCl, "Corners" );
	Opencl::TBufferArray<cl_float2> TruthCornersBuffer( GetArrayBridge(mTruthCorners), ContextCl, "mTruthCorners" );
	Opencl::TBufferArray<cl_float16> HomographysBuffer( GetArrayBridge(Homographys), ContextCl, "Homographys" );
	Opencl::TBufferArray<cl_float16> HomographyInvsBuffer( GetArrayBridge(HomographyInvs), ContextCl, "HomographyInvs" );

	
	auto Init = [this,&Frame,&CornersBuffer,&TruthCornersBuffer,&HomographysBuffer,&HomographyInvsBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
		
		Kernel.SetUniform("HoughCorners", CornersBuffer );
		Kernel.SetUniform("HoughCornerCount", size_cast<int>(CornersBuffer.GetSize()) );
		Kernel.SetUniform("TruthCorners", TruthCornersBuffer );
		Kernel.SetUniform("TruthCornerCount", size_cast<int>(TruthCornersBuffer.GetSize()) );
		Kernel.SetUniform("Homographys", HomographysBuffer );
		Kernel.SetUniform("HomographysInv", HomographyInvsBuffer );
		
		Iterations.PushBack( vec2x<size_t>(0, HomographysBuffer.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("HomographyIndexOffset", size_cast<cl_int>(Iteration.mFirst[0]) );
	};
	
	auto Finished = [&HomographysBuffer,&Homographys,&HomographyInvsBuffer,&HomographyInvs](Opencl::TKernelState& Kernel)
	{
		Opencl::TSync Semaphore;
		HomographysBuffer.Read( GetArrayBridge(Homographys), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
		//Opencl::TSync Semaphore2;
		//HomographyInvsBuffer.Read( GetArrayBridge(HomographyInvs), Kernel.GetContext(), &Semaphore2 );
		//Semaphore2.Wait();
	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait(/*"opencl runner"*/);

	//	allocate output data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_GetHoughCornerHomographys() );
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_GetHoughCornerHomographys&>( *Data.get() );
	
	//	here we should sort, and keep the best based on a histogram
	//	get all the valid homographys
	
	auto& FinalHomographys = StageData.mHomographys;
	auto& FinalHomographyInvs = StageData.mHomographyInvs;
	for ( int i=0;	i<Homographys.GetSize();	i++ )
	{
		//	if it's all zero's its invalid
		auto& Homography = Homographys[i];
		auto& HomographyInv = HomographyInvs[i];
		float Score = Homography.s[15];

		if ( Score == 0 )
			continue;
		std::Debug << "Homography #" << i << " score: " << Score << std::endl;
		
		if ( Score > 0.01f )
		{
			FinalHomographys.PushBack( Homography );
			FinalHomographyInvs.PushBack( HomographyInv );
		}
	}
	
	std::Debug << "Extracted x" << FinalHomographys.GetSize() << " SCORED homographys: ";
	static bool DebugExtractedHomographys = true;
	if ( DebugExtractedHomographys )
	{
		std::Debug.PushStreamSettings();
		
		for ( int i=0;	i<FinalHomographys.GetSize();	i++ )
		{
			auto& Homography = FinalHomographys[i];
			static int Precision = 5;
			std::Debug << std::endl;
			for ( int s=0;	s<sizeofarray(Homography.s);	s++ )
				std::Debug << std::fixed << std::setprecision( Precision ) << Homography.s[s] << " x ";
		}

		std::Debug.PopStreamSettings();
	}
	std::Debug << std::endl;
}



void TFilterStage_DrawHomographyCorners::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "TFilterStage_DrawHoughLines missing kernel" ) )
		return;
	auto FramePixels = Frame.GetFramePixels(mFilter,true);
	if ( !Soy::Assert( FramePixels != nullptr, "Frame missing frame pixels" ) )
		return;
	
	TUniformWrapper<std::string> ClearFragStageName("ClearFrag", std::string() );
	std::shared_ptr<SoyPixelsImpl> ClearPixels;
	Frame.SetUniform( ClearFragStageName, ClearFragStageName, mFilter, *this );
	try
	{
		auto& ClearFragStageData = Frame.GetData<TFilterStageRuntimeData&>(ClearFragStageName.mValue);
		ClearPixels = ClearFragStageData.GetPixels( ContextGl );
	}
	catch(std::exception& e)
	{
		//	no frag clearing
		ClearPixels.reset();
	}
	
	
	//	write straight to a texture
	//	gr: changed to write to a buffer image, if anything wants a texture, it'll convert on request
	//	gr: for re-iterating over the same image we wnat to re-clear
	auto CreateTexture = [&Frame,&Data,&FramePixels,&ClearPixels]
	{
		SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
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
	
	auto CreateImageBuffer = [this,&Frame,&Data,&FramePixels,&ContextCl]
	{
		SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
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
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data );
	
	//	get truth corners if we haven't set them up
	Array<cl_float2> TruthCorners;
	{
		TUniformWrapper<std::string> TruthCornersUniform("TruthCorners",std::string());
		if ( !Frame.SetUniform( TruthCornersUniform, TruthCornersUniform, mFilter, *this ) )
			throw Soy::AssertException("Missing TruthCorners uniform");
		
		auto PushVec = [&TruthCorners](const std::string& Part,const char& Delin)
		{
			vec2f Coord;
			Soy::StringToType( Coord, Part );
			TruthCorners.PushBack( Soy::VectorToCl(Coord) );
			return true;
		};
		Soy::StringSplitByMatches( PushVec, TruthCornersUniform.mValue, "," );
		Soy::Assert( !TruthCorners.IsEmpty(), "Failed to extract any truth corners");
	}
	
	
	auto& CornerStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughCorners>( mCornerDataStage );
	auto& HomographyStageData = Frame.GetData<TFilterStageRuntimeData_GetHoughCornerHomographys>( mHomographyDataStage );
	auto& Corners = CornerStageData.mCorners;
	auto& Homographys = HomographyStageData.mHomographys;
	Opencl::TBufferArray<cl_float4> CornersBuffer( GetArrayBridge(Corners), ContextCl, "Corners" );
	Opencl::TBufferArray<cl_float16> HomographysBuffer( GetArrayBridge(Homographys), ContextCl, "Homographys" );
	Opencl::TBufferArray<cl_float2> TruthCornersBuffer( GetArrayBridge(TruthCorners), ContextCl, "mTruthCorners" );
	
	auto Init = [this,&Frame,&StageData,&ContextGl,&CornersBuffer,&HomographysBuffer,&TruthCornersBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//ofScopeTimerWarning Timer("opencl blit init",40);
		
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
		
		Kernel.SetUniform("HoughCorners", CornersBuffer );
		Kernel.SetUniform("TruthCorners", TruthCornersBuffer );
		Kernel.SetUniform("Homographys", HomographysBuffer );
		Kernel.SetUniform("TruthCornerCount", size_cast<int>(TruthCornersBuffer.GetSize()) );
		
		Iterations.PushBack( vec2x<size_t>(0, CornersBuffer.GetSize() ) );
		Iterations.PushBack( vec2x<size_t>(0, HomographysBuffer.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("CornerIndexOffset", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("HomographyIndexOffset", size_cast<cl_int>(Iteration.mFirst[1]) );
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







void TFilterStage_DrawMaskOnFrame::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, "TFilterStage_DrawHoughLines missing kernel" ) )
		return;
	auto FramePixels = Frame.GetFramePixels(mFilter,true);
	if ( !Soy::Assert( FramePixels != nullptr, "Frame missing frame pixels" ) )
		return;
	
	TUniformWrapper<std::string> ClearFragStageName("ClearFrag", std::string() );
	std::shared_ptr<SoyPixelsImpl> ClearPixels;
	Frame.SetUniform( ClearFragStageName, ClearFragStageName, mFilter, *this );
	try
	{
		auto& ClearFragStageData = Frame.GetData<TFilterStageRuntimeData&>(ClearFragStageName.mValue);
		ClearPixels = ClearFragStageData.GetPixels( ContextGl );
	}
	catch(std::exception& e)
	{
		//	no frag clearing
		ClearPixels.reset();
	}
	
	
	//	write straight to a texture
	//	gr: changed to write to a buffer image, if anything wants a texture, it'll convert on request
	//	gr: for re-iterating over the same image we wnat to re-clear
	auto CreateTexture = [&Frame,&Data,&FramePixels,&ClearPixels]
	{
		SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
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
	
	auto CreateImageBuffer = [this,&Frame,&Data,&FramePixels,&ContextCl]
	{
		SoyPixelsMeta OutputPixelsMeta( FramePixels->GetWidth(), FramePixels->GetHeight(), SoyPixelsFormat::RGBA );
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
	if ( !mMaskTexture )
	{
		Array<char> PngData;
		Soy::FileToArray( GetArrayBridge(PngData), mMaskFilename );
		SoyPixels MaskPixels;
		MaskPixels.SetPng( GetArrayBridge(PngData) );
		auto MakeTexture = [&MaskPixels,this]
		{
			mMaskTexture.reset( new Opengl::TTexture( MaskPixels.GetMeta(), GL_TEXTURE_2D ) );
			mMaskTexture->Write( MaskPixels );
		};
		Soy::TSemaphore Semaphore;
		auto& ContextGl = mFilter.GetOpenglContext();
		ContextGl.PushJob( MakeTexture, Semaphore );
		Semaphore.Wait();
	}
	
	
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ShaderBlit&>( *Data );
	
	
	
	auto& HomographyStageData = Frame.GetData<TFilterStageRuntimeData_GetHoughCornerHomographys>( mHomographyDataStage );
	auto& Homographys = HomographyStageData.mHomographys;
	auto& HomographyInvs = HomographyStageData.mHomographyInvs;
	Opencl::TBufferArray<cl_float16> HomographysBuffer( GetArrayBridge(Homographys), ContextCl, "Homographys" );
	Opencl::TBufferArray<cl_float16> HomographyInvsBuffer( GetArrayBridge(HomographyInvs), ContextCl, "HomographyInvs" );
	
	
	//	option to draw frame on mask instead of mask on frame
	TUniformWrapper<int> DrawFrameOnMaskUniform("DrawFrameOnMask",0);
	Frame.SetUniform( DrawFrameOnMaskUniform, DrawFrameOnMaskUniform, mFilter, *this );
	bool DrawFrameOnMask = DrawFrameOnMaskUniform.mValue == 1;
	
	auto Init = [this,&Frame,&StageData,&ContextGl,&HomographysBuffer,&HomographyInvsBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//ofScopeTimerWarning Timer("opencl blit init",40);
		
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
	
		Kernel.SetUniform("Mask", Opengl::TTextureAndContext( *this->mMaskTexture, ContextGl ), OpenclBufferReadWrite::ReadWrite );
		Kernel.SetUniform("Homographys", HomographysBuffer );
		Kernel.SetUniform("HomographyInvs", HomographyInvsBuffer );
		
		static size_t HomographysToTest = 5000;
		auto HomographyCount = std::min(HomographysBuffer.GetSize(),HomographysToTest);
		
		Iterations.PushBack( vec2x<size_t>(0, StageData.mTexture.GetWidth() ) );
		Iterations.PushBack( vec2x<size_t>(0, StageData.mTexture.GetHeight() ) );
		Iterations.PushBack( vec2x<size_t>(0, HomographyCount ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetX", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetY", size_cast<cl_int>(Iteration.mFirst[1]) );
		Kernel.SetUniform("HomographyIndexOffset", size_cast<cl_int>(Iteration.mFirst[1]) );
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




