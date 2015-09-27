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
	
	auto& HoughStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughLines>( mHoughLineDataStage );
	auto& HoughLines = HoughStageData.mHoughLines;
	Opencl::TBufferArray<cl_float8> HoughLinesBuffer( GetArrayBridge(HoughLines), ContextCl, "HoughLines" );
	
	TUniformWrapper<std::string> ClearFragStageName("ClearFrag", std::string() );
	Frame.SetUniform( ClearFragStageName, ClearFragStageName, mFilter, *this );
	std::shared_ptr<SoyPixelsImpl> ClearPixels;
	try
	{
		auto& ClearFragStageData = Frame.GetData<TFilterStageRuntimeData&>(ClearFragStageName.mValue);
		auto ClearTexture = ClearFragStageData.GetTexture();
		auto Read = [&ClearTexture,&ClearPixels]
		{
			ClearPixels.reset( new SoyPixels );
			ClearTexture.Read( *ClearPixels );
		};
		Soy::TSemaphore Semaphore;
		auto& ContextGl = mFilter.GetOpenglContext();
		ContextGl.PushJob( Read, Semaphore );
		Semaphore.Wait();
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
		
		static bool ClearToBlack = true;
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

	//	allocate data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_ExtractHoughLines() );
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_ExtractHoughLines&>( *Data.get() );

	StageData.mHoughLines.SetSize( 100 );
	int LineBufferCount[] = {0};
	auto LineBufferCountArray = GetRemoteArray( LineBufferCount );
	Opencl::TBufferArray<cl_float8> LineBuffer( GetArrayBridge(StageData.mHoughLines), ContextCl, "LineBuffer" );
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
	
	auto Finished = [&StageData,&LineBuffer,&LineBufferCounter](Opencl::TKernelState& Kernel)
	{
		cl_int LineCount = 0;
		Opencl::TSync Semaphore;
		LineBufferCounter.Read( LineCount, Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
		
		StageData.mHoughLines.SetSize( std::min( LineCount, size_cast<cl_int>(LineBuffer.GetSize()) ) );
		LineBuffer.Read( GetArrayBridge(StageData.mHoughLines), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
	};
	
	//	run opencl
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
	ContextCl.PushJob( Job, Semaphore );
	Semaphore.Wait(/*"opencl runner"*/);
}

