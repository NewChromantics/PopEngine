#include "TFilterStageHoughTransform.h"

#include "TFilterStageOpencl.h"
#include "TFilterStageOpengl.h"


float GetDistance(const vec2f& a,const vec2f& b)
{
	vec2f d( a.x - b.x, a.y - b.y );
	return sqrtf( (d.x*d.x) + (d.y*d.y) );
}

vec2f GetHoughLineStart(const THoughLine& HoughLine)
{
	return vec2f( HoughLine.s[0], HoughLine.s[1] );
}

vec2f GetHoughLineEnd(const THoughLine& HoughLine)
{
	return vec2f( HoughLine.s[2], HoughLine.s[3] );
}

float GetHoughLineAngleIndex(const THoughLine& HoughLine)
{
	return HoughLine.s[4];
}

bool GetHoughLineVertical(const THoughLine& HoughLine)
{
	return HoughLine.s[7] > 0;
}

void SetHoughLineVertical(THoughLine& HoughLine,bool Vertical)
{
	HoughLine.s[7] = Vertical ? 1 : 0;
}

float& GetHoughLineScore(THoughLine& HoughLine)
{
	return HoughLine.s[6];
}


const float& GetHoughLineScore(const THoughLine& HoughLine)
{
	return HoughLine.s[6];
}



void SetHoughLineStartJointVertLineIndex(THoughLine& HoughLine,int Index)
{
	HoughLine.s[10] = Index;
}

void SetHoughLineEndJointVertLineIndex(THoughLine& HoughLine,int Index)
{
	HoughLine.s[11] = Index;
}

size_t GetAngleIndexFromWad(int WadIndex,size_t WindowCount,size_t AngleCount,size_t DistanceCount)
{
	auto WindowIndex = WadIndex / (AngleCount*DistanceCount);
	auto adIndex = WadIndex % (AngleCount*DistanceCount);
	auto AngleIndex = adIndex / DistanceCount;
	auto DistanceIndex = adIndex % DistanceCount;
	return AngleIndex;
}

size_t GetWadIndex(size_t WindowX,size_t WindowY,size_t AngleIndex,size_t DistanceIndex,size_t WindowCountX,size_t WindowCountY,size_t AngleCount,size_t DistanceCount)
{
	//	find window start
	size_t WindowIndex = WindowY * WindowCountX;
	size_t WindowBlockSize = AngleCount * DistanceCount;
	size_t Index = WindowIndex * WindowBlockSize;

	Index += AngleIndex * DistanceCount;
	Index += DistanceIndex;
	return Index;
}


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
		TUniformWrapper<float> DistanceRadius("HougDistanceRadius",900);
		
		Frame.SetUniform( AngleFrom, AngleFrom, mFilter, *this );
		Frame.SetUniform( AngleTo, AngleTo, mFilter, *this );
		Frame.SetUniform( AngleStep, AngleStep, mFilter, *this );
		Frame.SetUniform( DistanceStep, DistanceStep, mFilter, *this );
		Frame.SetUniform( DistanceRadius, DistanceRadius, mFilter, *this );

		float DistanceTo = DistanceRadius;
		float DistanceFrom = -DistanceRadius;
		
		for ( float a=AngleFrom;	a<=AngleTo;	a+=AngleStep )
			Angles.PushBack( a );
		for ( float Dist=DistanceFrom;	Dist<=DistanceTo;	Dist+=DistanceStep )
			Distances.PushBack( Dist );
	}

	auto& StageData = dynamic_cast<TFilterStageRuntimeData_GatherHoughTransforms&>( *Data.get() );
	auto& Angles = StageData.mAngles;
	auto& Distances = StageData.mDistances;
	auto& WindowXAngleXDistances = StageData.mWindowXAngleXDistances;
	auto WindowTotalCount = StageData.mWindowCount.x * StageData.mWindowCount.y;
	
	WindowXAngleXDistances.SetSize( WindowTotalCount * Angles.GetSize() * Distances.GetSize() );
	WindowXAngleXDistances.SetAll(0);
	
	Opencl::TBufferArray<cl_int> WindowXAngleXDistancesBuffer( GetArrayBridge(WindowXAngleXDistances), ContextCl, "mWindowXAngleXDistances" );
	Opencl::TBufferArray<cl_float> AnglesBuffer( GetArrayBridge(Angles), ContextCl, "mAngles" );
	Opencl::TBufferArray<cl_float> DistancesBuffer( GetArrayBridge(Distances), ContextCl, "mDirections" );

	auto Init = [this,&StageData,&Frame,&WindowXAngleXDistancesBuffer,&AnglesBuffer,&DistancesBuffer,&FrameWidth,&FrameHeight](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
	
		Kernel.SetUniform("WindowXAngleXDistances", WindowXAngleXDistancesBuffer );
		Kernel.SetUniform("AngleDegs", AnglesBuffer );
		Kernel.SetUniform("Distances", DistancesBuffer );
		Kernel.SetUniform("AngleCount", size_cast<cl_int>(AnglesBuffer.GetSize()) );
		Kernel.SetUniform("DistanceCount", size_cast<cl_int>(DistancesBuffer.GetSize()) );
		Kernel.SetUniform("WindowCountX", size_cast<cl_int>( StageData.mWindowCount.x ) );
		Kernel.SetUniform("WindowCountY", size_cast<cl_int>( StageData.mWindowCount.y ) );
	
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
	
	auto Finished = [&StageData,&WindowXAngleXDistances,&WindowXAngleXDistancesBuffer](Opencl::TKernelState& Kernel)
	{
		//	read back the histogram
		Opencl::TSync Semaphore;
		WindowXAngleXDistancesBuffer.Read( GetArrayBridge(WindowXAngleXDistances), Kernel.GetContext(), &Semaphore );
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
		for ( int wad=0;	wad<WindowXAngleXDistances.GetSize();	wad++ )
		{
			auto w = wad / WindowTotalCount;
			auto ad = wad % WindowTotalCount;
			auto a = ad / Distances.GetSize();
			auto d = ad % Distances.GetSize();

			std::Debug << "Window[" << w << "] Angle[" << Angles[a] << "][" << Distances[d] << "] x" << WindowXAngleXDistances[wad] << std::endl;
			TotalDistanceCount += WindowXAngleXDistances[wad];
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
	auto& WindowXAnglesXDistances = HoughStageData.mWindowXAngleXDistances;
	Opencl::TBufferArray<cl_int> WindowXAngleXDistancesBuffer( GetArrayBridge(WindowXAnglesXDistances), ContextCl, "mWindowXAngleXDistances" );
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
	
	
	auto Init = [this,&Frame,&StageData,&ContextGl,&Angles,&Distances,&WindowXAngleXDistancesBuffer,&AnglesBuffer,&DistancesBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
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
		
		Kernel.SetUniform("WindowXAngleXDistances", WindowXAngleXDistancesBuffer );
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
	
	auto& HoughStageData = Frame.GetData<TFilterStageRuntimeData_HoughLines>( mHoughLineDataStage );
	Array<THoughLine> HoughLines;
	HoughLines.PushBackArray( HoughStageData.mVertLines );
	HoughLines.PushBackArray( HoughStageData.mHorzLines );
	Opencl::TBufferArray<THoughLine> HoughLinesBuffer( GetArrayBridge(HoughLines), ContextCl, "HoughLines" );
	
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
	auto WindowCount = HoughStageData.mWindowCount;
	auto& WindowXAnglesXDistances = HoughStageData.mWindowXAngleXDistances;
	cl_int WindowCountX = size_cast<cl_int>( WindowCount.x );
	cl_int WindowCountY = size_cast<cl_int>( WindowCount.y );
	Opencl::TBufferArray<cl_int> WindowXAngleXDistancesBuffer( GetArrayBridge(WindowXAnglesXDistances), ContextCl, "mWindowXAngleXDistances" );
	Opencl::TBufferArray<cl_float> AnglesBuffer( GetArrayBridge(Angles), ContextCl, "mAngles" );
	Opencl::TBufferArray<cl_float> DistancesBuffer( GetArrayBridge(Distances), ContextCl, "mDirections" );

	static size_t TotalLineLimit = 8000;
	Array<THoughLine> AllLines;
	AllLines.SetSize(TotalLineLimit);
	int LineBufferCount[] = {0};
	auto LineBufferCountArray = GetRemoteArray( LineBufferCount );
	Opencl::TBufferArray<THoughLine> LineBuffer( GetArrayBridge(AllLines), ContextCl, "LineBuffer" );
	Opencl::TBufferArray<cl_int> LineBufferCounter( GetArrayBridge(LineBufferCountArray), ContextCl, "LineBufferCounter" );

	
	auto Init = [&](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
		
		Kernel.SetUniform("WindowXAngleXDistances", WindowXAngleXDistancesBuffer );
		Kernel.SetUniform("AngleDegs", AnglesBuffer );
		Kernel.SetUniform("AngleCount", size_cast<cl_int>(AnglesBuffer.GetSize()) );
		Kernel.SetUniform("Distances", DistancesBuffer );
		Kernel.SetUniform("DistanceCount", size_cast<cl_int>(DistancesBuffer.GetSize()) );
		Kernel.SetUniform("WindowCountX", size_cast<cl_int>(WindowCountX) );
		Kernel.SetUniform("WindowCountY", size_cast<cl_int>(WindowCountY) );
		Kernel.SetUniform("Matches", LineBuffer );
		Kernel.SetUniform("MatchesCount", LineBufferCounter );
		Kernel.SetUniform("MatchesMax", size_cast<cl_int>(LineBuffer.GetSize()) );
	
		
		Iterations.PushBack( vec2x<size_t>(0, WindowCountX * WindowCountY ) );
		Iterations.PushBack( vec2x<size_t>(0, Angles.GetSize() ) );
		Iterations.PushBack( vec2x<size_t>(0, Distances.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetWindow", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetAngle", size_cast<cl_int>(Iteration.mFirst[1]) );
		Kernel.SetUniform("OffsetDistance", size_cast<cl_int>(Iteration.mFirst[2]) );
	};
	
	auto Finished = [&](Opencl::TKernelState& Kernel)
	{
		cl_int LineCount = 0;
		Opencl::TSync Semaphore;
		LineBufferCounter.Read( LineCount, Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
		
		if ( LineCount > LineBuffer.GetSize() )
		{
			std::Debug << "Warning: Extracted " << LineCount << "/" << LineBuffer.GetSize() << " lines" << std::endl;
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
	
	/*
	//	filter max lines per windows
	TUniformWrapper<int> MaxLinesPerWindow("MaxLinesPerWindow", 1000 );
	Frame.SetUniform( MaxLinesPerWindow, MaxLinesPerWindow, mFilter, *this );
	if ( MaxLinesPerWindow > 0 )
	{
		for ( int wx=0;	wx<WindowCount.x;	wx++ )
		{
			for ( int wy=0;	wy<WindowCount.y;	wy++ )
			{
				auto SortIndex = [&](const size_t& wada,const size_t& wadb)
				{
					auto Scorea = WindowXAnglesXDistances[wada];
					auto Scoreb = WindowXAnglesXDistances[wadb];
					
					//	descending
					if ( Scorea < Scoreb )	return 1;
					if ( Scorea > Scoreb )	return -1;
					return 0;
				};
				Array<size_t> _Indexes;
				SortArrayLambda<size_t> Indexes( GetArrayBridge(_Indexes), SortIndex );

				auto FirstWad = GetWadIndex( wx, wy, 0, 0, WindowCountX, WindowCountY, Angles.GetSize(), Distances.GetSize() );
				for ( int i=0;	i<Distances.GetSize()*Angles.GetSize();	i++ )
				{
					auto WadIndex = FirstWad + i;
					Indexes.Push( WadIndex );
				}
				
				//	invalidate bottom N indexes to remove them
				for ( int i=MaxLinesPerWindow;	i<_Indexes.GetSize();	i++ )
				{
					auto& Score = WindowXAnglesXDistances[i];
					Score = 0;
				}
			}
		}
	}
	*/
	
	static bool ClassifyHorizontalLines = false;
	
	//	evaluate if lines are vertical or horizontal
	//	todo: auto gen this by histogramming, find median (vertical) opposite (horizontal)
	float VerticalAngle = 0;
	if ( ClassifyHorizontalLines )
	{
		Soy::TScopeTimerPrint Timer("", 2 );
		Timer.SetName(std::string(__func__)+ " determine vertical line");
		
		int BestWad = 0;
		for ( int wad=0;	wad<WindowXAnglesXDistances.GetSize();	wad++ )
		{
			if ( WindowXAnglesXDistances[wad] <= WindowXAnglesXDistances[BestWad] )
				continue;
			BestWad = wad;
		}
		auto BestAngleIndex = GetAngleIndexFromWad( BestWad, WindowCount.x*WindowCount.y, Angles.GetSize(), Distances.GetSize() );
		VerticalAngle = Angles[BestAngleIndex];
		if ( VerticalAngle < Angles[0] )
			VerticalAngle = Angles[0];
	}
	
	//	allocate final data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_HoughLines() );
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_HoughLines&>( *Data.get() );

	if ( ClassifyHorizontalLines )
	{
		Soy::TScopeTimerPrint Timer("",2);
		Timer.SetName(std::string(__func__)+ " ClassifyHorizontalLines");
		
		auto CompareLineScores = [](const THoughLine& a,const THoughLine& b)
		{
			auto& aScore = GetHoughLineScore( a );
			auto& bScore = GetHoughLineScore( b );
			if ( aScore > bScore )	return -1;
			if ( aScore < bScore )	return 1;
			return 0;
		};
		
		//	gr: the threshold is LESS than 45 (90 degrees) because viewing angles are skewed, lines are rarely ACTUALLY perpendicualr
		TUniformWrapper<float> VerticalThresholdUniform("VerticalThreshold", 10.f );
		Frame.SetUniform( VerticalThresholdUniform, VerticalThresholdUniform, mFilter, *this );

		//	copy lines whilst sorting & modify flag to say vertical or horizontal
		SortArrayLambda<THoughLine> FinalVertLines( GetArrayBridge(StageData.mVertLines), CompareLineScores );
		SortArrayLambda<THoughLine> FinalHorzLines( GetArrayBridge(StageData.mHorzLines), CompareLineScores );
		for ( int i=0;	i<AllLines.GetSize();	i++ )
		{
			auto& Line = AllLines[i];
			auto Angle = GetHoughLineAngleIndex(Line);
			
			float Threshold = VerticalThresholdUniform.mValue;
			
			float Diff = Angle - VerticalAngle;
			while ( Diff < -90.f )
				Diff += 180.f;
			while ( Diff > 90.f )
				Diff -= 180.f;
			Diff = fabsf(Diff);
			
			bool IsVertical = ( Diff <= Threshold );
			SetHoughLineVertical( Line, IsVertical );
			
			static bool DebugVerticalTest = false;
			if ( DebugVerticalTest )
				std::Debug << Angle << " -> " << VerticalAngle << " diff= " << Diff << " vertical: " << IsVertical << " (threshold: " << Threshold << ")" << std::endl;
			
			//	add to sorted list
			if ( IsVertical )
				FinalVertLines.Push( Line );
			else
				FinalHorzLines.Push( Line );
		}
		
		//	limit output
		TUniformWrapper<int> MaxVertLinesUniform("MaxVertLines",500);
		TUniformWrapper<int> MaxHorzLinesUniform("MaxHorzLines",3000);
		SetUniform( MaxVertLinesUniform, MaxVertLinesUniform );
		SetUniform( MaxHorzLinesUniform, MaxHorzLinesUniform );
		FinalVertLines.SetSize( std::min<size_t>( FinalVertLines.GetSize(), MaxVertLinesUniform.mValue ) );
		FinalHorzLines.SetSize( std::min<size_t>( FinalHorzLines.GetSize(), MaxHorzLinesUniform.mValue ) );
	}
	else
	{
		StageData.mVertLines.Copy( AllLines );
		//FinalVertLines.SetSize( std::min<size_t>( FinalVertLines.GetSize(), MaxVertLinesUniform.mValue ) );
	}
	
	static bool OutputLinesForConfig = false;
	if ( OutputLinesForConfig )
	{
		Array<THoughLine> Lines;
		Lines.PushBackArray( StageData.mVertLines );
		Lines.PushBackArray( StageData.mHorzLines );
		std::Debug.PushStreamSettings();
		std::Debug << std::fixed << std::setprecision(3);
		
		auto PrintLineArray = [](const Array<THoughLine>& Lines,std::function<bool(const THoughLine&)> Filter)
		{
			auto ComponentDelin = Soy::VecNXDelins[0];
			auto VectorDelin = Soy::VecNXDelins[1];
			
			for ( int i=0;	i<Lines.GetSize();	i++ )
			{
				auto Line = Lines[i];
				if ( !Filter(Line) )
					continue;
				
				for ( int s=0;	s<sizeofarray(Line.s);	s++ )
				{
					if ( s>0 )
						std::Debug << ComponentDelin;
					std::Debug << Line.s[s];
				}
				std::Debug << VectorDelin;
			}
		};
		
		std::Debug << "VerticalHoughLines=";
		PrintLineArray( Lines, [](const THoughLine& Line)	{	return GetHoughLineVertical(Line);	} );
		std::Debug << std::endl;
		
		std::Debug << "HorizontalHoughLines=";
		PrintLineArray( Lines, [](const THoughLine& Line)	{	return GetHoughLineVertical(Line);	} );
		std::Debug << std::endl;
		
		std::Debug.PopStreamSettings();
	}
	
	{
		auto v = StageData.mVertLines.GetSize();
		auto h = StageData.mHorzLines.GetSize();
		std::Debug << "Extracted x" << v << " vert lines and x" << h << " horz lines (" << v+h << " total)" << std::endl;
	}
	static bool DebugExtractedHoughLines = false;
	if ( DebugExtractedHoughLines )
	{
		Array<THoughLine> Lines;
		Lines.PushBackArray( StageData.mVertLines );
		Lines.PushBackArray( StageData.mHorzLines );
		for ( int i=0;	i<Lines.GetSize();	i++ )
		{
			auto Line = Lines[i];
			float Score = GetHoughLineScore(Line);
			float Vertical = GetHoughLineVertical(Line);
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

	//	special "stage name" which reads truths from uniforms
	//	todo: truths will later be "calculated" in a previous stage (calculated once in filter lifetime)
	Array<THoughLine> Lines;
	auto& HoughStageData = Frame.GetData<TFilterStageRuntimeData_HoughLines>( mHoughLineStage );
	Lines.PushBackArray( HoughStageData.mVertLines );
	Lines.PushBackArray( HoughStageData.mHorzLines );
	Opencl::TBufferArray<THoughLine> LinesBuffer( GetArrayBridge(Lines), ContextCl, "mHoughLines" );
	
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
	//Job->mCatchExceptions = false;
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
	
	std::Debug << mName << " extracted x" << FinalCorners.GetSize() << " hough corners: ";
	static bool DebugExtractedHoughCorners = false;
	if ( DebugExtractedHoughCorners )
	{
		std::Debug.PushStreamSettings();
		for ( int i=0;	i<FinalCorners.GetSize();	i++ )
		{
			auto Corner4 = Soy::ClToVector( FinalCorners[i] );
			static int Precision = 0;
			std::Debug << std::fixed << std::setprecision( Precision ) << Corner4.x << "x" << Corner4.y << ",";// << Corner4.z << std::endl;
		}
		std::Debug.PopStreamSettings();
	}
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
			throw Soy::AssertException( std::string("Missing uniform ")+TruthCornersUniform.mName);
		
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
	
	
	//	gr: change this to generate likely pairings;
	//	2 horizontal & 2 vertical lines
	//	use most significant (highest scores)
	//		always use best score against others?
	//	note: vertical & horizontal could be backwards in either image, so test horz x vert as well as vertxvert
	//	favour check things like sensible aspect ratios, area consumed by rect
	
	
	
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
		Kernel.SetUniform("HomographyInvs", HomographyInvsBuffer );
		
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





void TFilterStage_GetTruthLines::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	auto ReadUniform_ArrayFloat8 = [&Frame,this](Array<cl_float8>& Float8s,std::string UniformName)
	{
		TUniformWrapper<std::string> TruthCornersUniform(UniformName,std::string());
		if ( !Frame.SetUniform( TruthCornersUniform, TruthCornersUniform, mFilter, *this ) )
			throw Soy::AssertException( std::string("Missing uniform ") + UniformName );
		
		auto PushVec = [&Float8s](const std::string& Part,const char& Delin)
		{
			BufferArray<float,8> Components;
			if ( !Soy::StringParseVecNx( Part, GetArrayBridge(Components) ) )
				return false;
			
			auto float8 = Soy::VectorToCl8(GetArrayBridge(Components));
			Float8s.PushBack( float8 );
			return true;
		};
		std::string Delins;
		Delins += Soy::VecNXDelins[1];
		Soy::StringSplitByMatches( PushVec, TruthCornersUniform.mValue, Delins );
		Soy::Assert( !Float8s.IsEmpty(), std::string("Failed to extract truth lines ") + UniformName );
		
	};
	
	auto ReadUniform_ArrayFloat16 = [&Frame,this](Array<cl_float16>& Float8s,std::string UniformName)
	{
		TUniformWrapper<std::string> TruthCornersUniform(UniformName,std::string());
		if ( !Frame.SetUniform( TruthCornersUniform, TruthCornersUniform, mFilter, *this ) )
			throw Soy::AssertException( std::string("Missing uniform ") + UniformName );
		
		auto PushVec = [&Float8s](const std::string& Part,const char& Delin)
		{
			BufferArray<float,16> Components;
			if ( !Soy::StringParseVecNx( Part, GetArrayBridge(Components) ) )
				return false;
			
			auto float8 = Soy::VectorToCl16(GetArrayBridge(Components));
			Float8s.PushBack( float8 );
			return true;
		};
		std::string Delins;
		Delins += Soy::VecNXDelins[1];
		Soy::StringSplitByMatches( PushVec, TruthCornersUniform.mValue, Delins );
		Soy::Assert( !Float8s.IsEmpty(), std::string("Failed to extract truth lines ") + UniformName );
		
	};
	
	//	allocate data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_HoughLines() );
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_HoughLines&>( *Data.get() );
	
	auto& FinalVertLines = StageData.mVertLines;
	auto& FinalHorzLines = StageData.mHorzLines;
	
	ReadUniform_ArrayFloat16( StageData.mVertLines, mVerticalLinesUniform );
	ReadUniform_ArrayFloat16( StageData.mHorzLines, mHorzLinesUniform );

	//	dupes & bad cases
	//FinalVertLines.RemoveBlock(0,1);
	//FinalHorzLines.RemoveBlock(0,1);
	
	//	remove non vertical/horizontal lines
	TUniformWrapper<int> SquareAnglesOnlyUniform("SquareAnglesOnly",false);
	Frame.SetUniform( SquareAnglesOnlyUniform, SquareAnglesOnlyUniform, mFilter, *this );
	bool SquareAnglesOnly = SquareAnglesOnlyUniform.mValue!=0;
	if ( SquareAnglesOnly )
	{
		for ( int i=StageData.mHorzLines.GetSize()-1;	i>=0;	i-- )
		{
			throw Soy::AssertException("Test me");
			auto LineAngle = GetHoughLineAngleIndex(StageData.mHorzLines[i]);
			static float Tolerance=10;
			bool AngleIs0 = fabsf( Soy::AngleDegDiff( LineAngle, 0 ) ) < Tolerance;
			bool AngleIs90 = fabsf( Soy::AngleDegDiff( LineAngle, 90 ) ) < Tolerance;
			bool AngleIs180 = fabsf( Soy::AngleDegDiff( LineAngle, 180 ) ) < Tolerance;
			bool AngleIs270 = fabsf( Soy::AngleDegDiff( LineAngle, 270 ) ) < Tolerance;
			if ( AngleIs0 || AngleIs90 || AngleIs180 || AngleIs270 )
				continue;
			StageData.mHorzLines.RemoveBlock( i,1 );
			//std::Debug << "Removed horz truth line at " << LineAngle << " degrees" << std::endl;
		}
	}
	
	//	restrict testing to specific sets
	//FinalVertLines.SetSize( std::min<size_t>( FinalVertLines.GetSize(), 2 ) );
	//FinalHorzLines.SetSize( std::min<size_t>( FinalHorzLines.GetSize(), 2 ) );

}

													

#define float2 cl_float2
#define float3 cl_float3
#define float4 cl_float4
#define float8 cl_float8
#define float16 cl_float16
#define int4 cl_int4
#define __kernel
#define global 


//	z is 0 if bad lines
static float2 GetRayRayIntersection(float8 RayA,float8 RayB)
{
#define x s[0]
#define y s[1]
#define z s[2]
#define w s[3]
	float Ax = RayA.x;
	float Ay = RayA.y;
	float Bx = RayA.x + RayA.z;
	float By = RayA.y + RayA.w;
	float Cx = RayB.x;
	float Cy = RayB.y;
	float Dx = RayB.x + RayB.z;
	float Dy = RayB.y + RayB.w;
	
	
	float  distAB, theCos, theSin, newX, ABpos ;
	
	//  Fail if either line is undefined.
	//if (Ax==Bx && Ay==By || Cx==Dx && Cy==Dy) return NO;
	
	//  (1) Translate the system so that point A is on the origin.
	Bx-=Ax; By-=Ay;
	Cx-=Ax; Cy-=Ay;
	Dx-=Ax; Dy-=Ay;
	
	//  Discover the length of segment A-B.
	distAB=sqrt(Bx*Bx+By*By);
	
	//  (2) Rotate the system so that point B is on the positive X axis.
	theCos=Bx/distAB;
	theSin=By/distAB;
	newX=Cx*theCos+Cy*theSin;
	Cy  =Cy*theCos-Cx*theSin; Cx=newX;
	newX=Dx*theCos+Dy*theSin;
	Dy  =Dy*theCos-Dx*theSin; Dx=newX;
	
	//  Fail if the lines are parallel.
	//if (Cy==Dy) return NO;
	float Score = (Cy==Dy) ? 0 : 1;
	
	//  (3) Discover the position of the intersection point along line A-B.
	ABpos=Dx+(Cx-Dx)*Dy/(Dy-Cy);
	
	//  (4) Apply the discovered position to line A-B in the original coordinate system.
	float2 Intersection;
	Intersection.x = Ax + ABpos * theCos;
	Intersection.y = Ay + ABpos * theSin;
	return (float2){.s={Intersection.x,Intersection.y}};
}

static float8 LineToRay(float8 Line)
{
	return (cl_float8){ .s={ Line.x, Line.y, Line.z-Line.x, Line.w-Line.y }};
}

static float8 LineToRay(float16 Line)
{
	return (cl_float8){ .s={ Line.x, Line.y, Line.z-Line.x, Line.w-Line.y }};
}

//	z is 0 if bad lines
static float2 GetLineLineInfiniteIntersection(float8 LineA,float8 LineB)
{
	float8 RayA = LineToRay(LineA);
	float8 RayB = LineToRay(LineB);
	return GetRayRayIntersection( RayA, RayB );
}

static float2 GetLineLineInfiniteIntersection(float16 LineA,float16 LineB)
{
	float8 RayA = LineToRay(LineA);
	float8 RayB = LineToRay(LineB);
	return GetRayRayIntersection( RayA, RayB );
}

#undef x
#undef y
#undef z
#undef w



void gaussian_elimination(float *input, int n)
{
	// ported to c from pseudocode in
	// http://en.wikipedia.org/wiki/Gaussian_elimination
	
	float * A = input;
	int i = 0;
	int j = 0;
	int m = n-1;
	while (i < m && j < n){
		// Find pivot in column j, starting in row i:
		int maxi = i;
		for(int k = i+1; k<m; k++){
			if(fabs(A[k*n+j]) > fabs(A[maxi*n+j])){
				maxi = k;
			}
		}
		if (A[maxi*n+j] != 0){
			//swap rows i and maxi, but do not change the value of i
			if(i!=maxi)
				for(int k=0;k<n;k++){
					float aux = A[i*n+k];
					A[i*n+k]=A[maxi*n+k];
					A[maxi*n+k]=aux;
				}
			//Now A[i,j] will contain the old value of A[maxi,j].
			//divide each entry in row i by A[i,j]
			float A_ij=A[i*n+j];
			for(int k=0;k<n;k++){
				A[i*n+k]/=A_ij;
			}
			//Now A[i,j] will have the value 1.
			for(int u = i+1; u< m; u++){
				//subtract A[u,j] * row i from row u
				float A_uj = A[u*n+j];
				for(int k=0;k<n;k++){
					A[u*n+k]-=A_uj*A[i*n+k];
				}
				//Now A[u,j] will be 0, since A[u,j] - A[i,j] * A[u,j] = A[u,j] - 1 * A[u,j] = 0.
			}
			
			i++;
		}
		j++;
	}
	
	//back substitution
	for(int i=m-2;i>=0;i--){
		for(int j=i+1;j<n-1;j++){
			A[i*n+m]-=A[i*n+j]*A[j*n+m];
			//A[i*n+j]=0;
		}
	}
}

//	http://forum.openframeworks.cc/t/quad-warping-homography-without-opencv/3121/21
cl_float16 of_findHomography(cl_float2 srccl[4], cl_float2 dstcl[4])
{
	float src[4][2];
	float dst[4][2];
	
	for ( int i=0;	i<4;	i++ )
	{
		for ( int xy=0;	xy<2;	xy++ )
		{
			src[i][xy] = srccl[i].s[xy];
			dst[i][xy] = dstcl[i].s[xy];
			//src[i][xy] = dstcl[i].s[xy];
			//dst[i][xy] = srccl[i].s[xy];
		}
	}
	
	// create the equation system to be solved
	//
	// from: Multiple View Geometry in Computer Vision 2ed
	//       Hartley R. and Zisserman A.
	//
	// x' = xH
	// where H is the homography: a 3 by 3 matrix
	// that transformed to inhomogeneous coordinates for each point
	// gives the following equations for each point:
	//
	// x' * (h31*x + h32*y + h33) = h11*x + h12*y + h13
	// y' * (h31*x + h32*y + h33) = h21*x + h22*y + h23
	//
	// as the homography is scale independent we can let h33 be 1 (indeed any of the terms)
	// so for 4 points we have 8 equations for 8 terms to solve: h11 - h32
	// after ordering the terms it gives the following matrix
	// that can be solved with gaussian elimination:
	
	float P[8][9]={
		{-src[0][0], -src[0][1], -1,   0,   0,  0, src[0][0]*dst[0][0], src[0][1]*dst[0][0], -dst[0][0] }, // h11
		{  0,   0,  0, -src[0][0], -src[0][1], -1, src[0][0]*dst[0][1], src[0][1]*dst[0][1], -dst[0][1] }, // h12
		
		{-src[1][0], -src[1][1], -1,   0,   0,  0, src[1][0]*dst[1][0], src[1][1]*dst[1][0], -dst[1][0] }, // h13
		{  0,   0,  0, -src[1][0], -src[1][1], -1, src[1][0]*dst[1][1], src[1][1]*dst[1][1], -dst[1][1] }, // h21
		
		{-src[2][0], -src[2][1], -1,   0,   0,  0, src[2][0]*dst[2][0], src[2][1]*dst[2][0], -dst[2][0] }, // h22
		{  0,   0,  0, -src[2][0], -src[2][1], -1, src[2][0]*dst[2][1], src[2][1]*dst[2][1], -dst[2][1] }, // h23
		
		{-src[3][0], -src[3][1], -1,   0,   0,  0, src[3][0]*dst[3][0], src[3][1]*dst[3][0], -dst[3][0] }, // h31
		{  0,   0,  0, -src[3][0], -src[3][1], -1, src[3][0]*dst[3][1], src[3][1]*dst[3][1], -dst[3][1] }, // h32
	};
	
	gaussian_elimination(&P[0][0],9);
/*
	// gaussian elimination gives the results of the equation system
	// in the last column of the original matrix.
	// opengl needs the transposed 4x4 matrix:
	float aux_H[]=
	{
		P[0][8],P[3][8],0,P[6][8], // h11  h21 0 h31
		P[1][8],P[4][8],0,P[7][8], // h12  h22 0 h32
		0      ,      0,0,0,       // 0    0   0 0
		P[2][8],P[5][8],0,1 // h13  h23 0 h33
	};

	//	4x4 to 3x3
	cl_float16 Result;
	Result.s[0] = aux_H[0];
	Result.s[1] = aux_H[1];
	Result.s[2] = aux_H[3];
	
	Result.s[3] = aux_H[4];
	Result.s[4] = aux_H[5];
	Result.s[5] = aux_H[7];

	Result.s[6] = aux_H[12];
	Result.s[7] = aux_H[13];
	Result.s[8] = aux_H[15];
*/
	//	non transposed 3x3
	cl_float16 Result;
	Result.s[0] = P[0][8];
	Result.s[1] = P[1][8];
	Result.s[2] = P[2][8];
	
	Result.s[3] = P[3][8];
	Result.s[4] = P[4][8];
	Result.s[5] = P[5][8];
	
	Result.s[6] = P[6][8];
	Result.s[7] = P[7][8];
	Result.s[8] = 1;
	//Result.s[8] = P[8][8];

	
	static bool Test = false;
	if ( Test )
	{
		//	test
		for ( int i=0;	i<4;	i++ )
		{
			auto H = Result.s;
			float x = H[0]*src[i][0] + H[1]*src[i][1] + H[2];
			float y = H[3]*src[i][0] + H[4]*src[i][1] + H[5];
			float z = H[6]*src[i][0] + H[7]*src[i][1] + H[8];
			
			x /= z;
			y /= z;
			
			float diffx = dst[i][0] - x;
			float diffy = dst[i][1] - y;
			std::Debug << "err src->dst #" << i << ": " << diffx << "," << diffy << std::endl;
		}
		for ( int i=0;	i<4;	i++ )
		{
			auto H = Result.s;
			float x = H[0]*dst[i][0] + H[1]*dst[i][1] + H[2];
			float y = H[3]*dst[i][0] + H[4]*dst[i][1] + H[5];
			float z = H[6]*dst[i][0] + H[7]*dst[i][1] + H[8];
			
			x /= z;
			y /= z;
			
			float diffx = src[i][0] - x;
			float diffy = src[i][1] - y;
			std::Debug << "err dst->src #" << i << ": " << diffx << "," << diffy << std::endl;
		}
	}
	
	return Result;
}

static double SIGN(double a, double b)
{
	if(b > 0) {
		return fabs(a);
	}
	
	return -fabs(a);
}

static double PYTHAG(double a, double b)
{
	double at = fabs(a), bt = fabs(b), ct, result;
	
	if (at > bt)       { ct = bt / at; result = at * sqrt(1.0 + ct * ct); }
	else if (bt > 0.0) { ct = at / bt; result = bt * sqrt(1.0 + ct * ct); }
	else result = 0.0;
	return(result);
}

// Returns 1 on success, fail otherwise
static int dsvd(float *a, int m, int n, float *w, float *v)
{
	// Thos SVD code requires rows >= columns.
#define M 9 // rows
#define N 9 // cols
	//	float w[N];
	//	float v[N*N];
	
	int flag, i, its, j, jj, k, l, nm;
	double c, f, h, s, x, y, z;
	double anorm = 0.0, g = 0.0, scale = 0.0;
	double rv1[N];
	
	if (m < n)
	{
		//fprintf(stderr, "#rows must be > #cols \n");
		return(-1);
	}
	
	
	//	Householder reduction to bidiagonal form
	for (i = 0; i < n; i++)
	{
		//left-hand reduction
		l = i + 1;
		rv1[i] = scale * g;
		g = s = scale = 0.0;
		if (i < m)
		{
			for (k = i; k < m; k++)
				scale += fabsf(a[k*n+i]);
			
			if (scale )
			{
				for (k = i; k < m; k++)
				{
					a[k*n+i] /= scale;
					s += a[k*n+i] * a[k*n+i];
				}
				
				 f = (double)a[i*n+i];
				 g = -SIGN(sqrt(s), f);
				 h = f * g - s;
				 a[i*n+i] = (float)(f - g);
				 if (i != n - 1)
				 {
					for (j = l; j < n; j++)
					{
				 for (s = 0.0, k = i; k < m; k++)
				 s += ((double)a[k*n+i] * (double)a[k*n+j]);
				 f = s / h;
				 for (k = i; k < m; k++)
				 a[k*n+j] += (float)(f * (double)a[k*n+i]);
					}
				 }
				 for (k = i; k < m; k++)
					a[k*n+i] = (float)((double)a[k*n+i]*scale);
				
			}
			
		}
		w[i] = (float)(scale * g);
		
		 /// right-hand reduction
		 g = s = scale = 0.0;
		 if (i < m && i != n - 1)
		 {
			for (k = l; k < n; k++)
		 scale += fabs((double)a[i*n+k]);
			if (scale)
			{
		 for (k = l; k < n; k++)
		 {
		 a[i*n+k] = (float)((double)a[i*n+k]/scale);
		 s += ((double)a[i*n+k] * (double)a[i*n+k]);
		 }
		 f = (double)a[i*n+l];
		 g = -SIGN(sqrt(s), f);
		 h = f * g - s;
		 a[i*n+l] = (float)(f - g);
		 for (k = l; k < n; k++)
		 rv1[k] = (double)a[i*n+k] / h;
		 if (i != m - 1)
		 {
		 for (j = l; j < m; j++)
		 {
		 for (s = 0.0, k = l; k < n; k++)
		 s += ((double)a[j*n+k] * (double)a[i*n+k]);
		 for (k = l; k < n; k++)
		 a[j*n+k] += (float)(s * rv1[k]);
		 }
		 }
		 for (k = l; k < n; k++)
		 a[i*n+k] = (float)((double)a[i*n+k]*scale);
			}
		 }
		anorm = std::max(anorm, (fabs((double)w[i]) + fabs(rv1[i])));
		 
	}
	
	 // accumulate the right-hand transformation
	 for (i = n - 1; i >= 0; i--)
	 {
		if (i < n - 1)
		{
	 if (g)
	 {
	 for (j = l; j < n; j++)
	 v[j*n+i] = (float)(((double)a[i*n+j] / (double)a[i*n+l]) / g);
	 // double division to avoid underflow
	 for (j = l; j < n; j++)
	 {
	 for (s = 0.0, k = l; k < n; k++)
	 s += ((double)a[i*n+k] * (double)v[k*n+j]);
	 for (k = l; k < n; k++)
	 v[k*n+j] += (float)(s * (double)v[k*n+i]);
	 }
	 }
	 for (j = l; j < n; j++)
	 v[i*n+j] = v[j*n+i] = 0.0;
		}
		v[i*n+i] = 1.0;
		g = rv1[i];
		l = i;
	 }
	 
	 //accumulate the left-hand transformation
	 for (i = n - 1; i >= 0; i--)
	 {
		l = i + 1;
		g = (double)w[i];
		if (i < n - 1)
	 for (j = l; j < n; j++)
	 a[i*n+j] = 0.0;
		if (g)
		{
	 g = 1.0 / g;
	 if (i != n - 1)
	 {
	 for (j = l; j < n; j++)
	 {
	 for (s = 0.0, k = l; k < m; k++)
	 s += ((double)a[k*n+i] * (double)a[k*n+j]);
	 f = (s / (double)a[i*n+i]) * g;
	 for (k = i; k < m; k++)
	 a[k*n+j] += (float)(f * (double)a[k*n+i]);
	 }
	 }
	 for (j = i; j < m; j++)
	 a[j*n+i] = (float)((double)a[j*n+i]*g);
		}
		else
		{
	 for (j = i; j < m; j++)
	 a[j*n+i] = 0.0;
		}
		++a[i*n+i];
	 }
	 
	 // diagonalize the bidiagonal form
	 for (k = n - 1; k >= 0; k--)
	 {                           // loop over singular values
		for (its = 0; its < 30; its++)
		{                       // loop over allowed iterations
	 flag = 1;
	 for (l = k; l >= 0; l--)
	 {                     // test for splitting
	 nm = l - 1;
	 if (fabs(rv1[l]) + anorm == anorm)
	 {
	 flag = 0;
	 break;
	 }
	 if (fabs((double)w[nm]) + anorm == anorm)
	 break;
	 }
	 if (flag)
	 {
	 c = 0.0;
	 s = 1.0;
	 for (i = l; i <= k; i++)
	 {
	 f = s * rv1[i];
	 if (fabs(f) + anorm != anorm)
	 {
	 g = (double)w[i];
	 h = PYTHAG(f, g);
	 w[i] = (float)h;
	 h = 1.0 / h;
	 c = g * h;
	 s = (- f * h);
	 for (j = 0; j < m; j++)
	 {
	 y = (double)a[j*n+nm];
	 z = (double)a[j*n+i];
	 a[j*n+nm] = (float)(y * c + z * s);
	 a[j*n+i] = (float)(z * c - y * s);
	 }
	 }
	 }
	 }
	 z = (double)w[k];
	 if (l == k)
	 {                  //convergence
	 if (z < 0.0)
	 {              // make singular value nonnegative
	 w[k] = (float)(-z);
	 for (j = 0; j < n; j++)
	 v[j*n+k] = (-v[j*n+k]);
	 }
	 break;
	 }
	 if (its >= 30) {
	 //free((void*) rv1);
	 //fprintf(stderr, "No convergence after 30,000! iterations \n");
	 return(0);
	 }
	 
	 ///shift from bottom 2 x 2 minor
	 x = (double)w[l];
	 nm = k - 1;
	 y = (double)w[nm];
	 g = rv1[nm];
	 h = rv1[k];
	 f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.0 * h * y);
	 g = PYTHAG(f, 1.0);
	 f = ((x - z) * (x + z) + h * ((y / (f + SIGN(g, f))) - h)) / x;
	 
	 // next QR transformation
	 c = s = 1.0;
	 for (j = l; j <= nm; j++)
	 {
	 i = j + 1;
	 g = rv1[i];
	 y = (double)w[i];
	 h = s * g;
	 g = c * g;
	 z = PYTHAG(f, h);
	 rv1[j] = z;
	 c = f / z;
	 s = h / z;
	 f = x * c + g * s;
	 g = g * c - x * s;
	 h = y * s;
	 y = y * c;
	 for (jj = 0; jj < n; jj++)
	 {
	 x = (double)v[jj*n+j];
	 z = (double)v[jj*n+i];
	 v[jj*n+j] = (float)(x * c + z * s);
	 v[jj*n+i] = (float)(z * c - x * s);
	 }
	 z = PYTHAG(f, h);
	 w[j] = (float)z;
	 if (z)
	 {
	 z = 1.0 / z;
	 c = f * z;
	 s = h * z;
	 }
	 f = (c * g) + (s * y);
	 x = (c * y) - (s * g);
	 for (jj = 0; jj < m; jj++)
	 {
	 y = (double)a[jj*n+j];
	 z = (double)a[jj*n+i];
	 a[jj*n+j] = (float)(y * c + z * s);
	 a[jj*n+i] = (float)(z * c - y * s);
	 }
	 }
	 rv1[l] = 0.0;
	 rv1[k] = f;
	 w[k] = (float)x;
		}
	 }
	 
#undef M
#undef N

	return(1);
}



/*****************************************************************
 
 Solve least squares Problem C*x+d = r, |r| = min!, by Given rotations
 (QR-decomposition). Direct implementation of the algorithm
 presented in H.R.Schwarz: Numerische Mathematik, 'equation'
 number (7.33)
 
 If 'd == NULL', d is not accesed: the routine just computes the QR
 decomposition of C and exits.
 
 If 'want_r == 0', r is not rotated back (\hat{r} is returned
 instead).
 
 *****************************************************************/
inline int fsign(double x)
{
	return (x > 0 ? 1 : (x < 0) ? -1 : 0);
}

bool Givens(double** C, double* d, double* x, double* r, int N, int n, int want_r)
{
	int i, j, k;
	double w, gamma, sigma, rho, temp;
	double epsilon = DBL_EPSILON;	/* FIXME (?) */
	
	/*
	 * First, construct QR decomposition of C, by 'rotating away'
	 * all elements of C below the diagonal. The rotations are
	 * stored in place as Givens coefficients rho.
	 * Vector d is also rotated in this same turn, if it exists
	 */
	for (j = 0; j < n; j++)
	{
		for (i = j + 1; i < N; i++)
		{
			if (C[i][j])
			{
				if (fabs(C[j][j]) < epsilon * fabs(C[i][j]))
				{
					/* find the rotation parameters */
					w = -C[i][j];
					gamma = 0;
					sigma = 1;
					rho = 1;
				}
				else
				{
					w = fsign(C[j][j]) * sqrt(C[j][j] * C[j][j] + C[i][j] * C[i][j]);
					if (w == 0)
					{
						return false;
					}
					gamma = C[j][j] / w;
					sigma = -C[i][j] / w;
					rho = (fabs(sigma) < gamma) ? sigma : fsign(sigma) / gamma;
				}
				C[j][j] = w;
				C[i][j] = rho;	/* store rho in place, for later use */
				for (k = j + 1; k < n; k++)
				{
					/* rotation on index pair (i,j) */
					temp = gamma * C[j][k] - sigma * C[i][k];
					C[i][k] = sigma * C[j][k] + gamma * C[i][k];
					C[j][k] = temp;
					
				}
				if (d)  	/* if no d vector given, don't use it */
				{
					temp = gamma * d[j] - sigma * d[i];		/* rotate d */
					d[i] = sigma * d[j] + gamma * d[i];
					d[j] = temp;
				}
			}
		}
	}
	
	if (!d)			/* stop here if no d was specified */
	{
		return true;
	}
	
	/* solve R*x+d = 0, by backsubstitution */
	for (i = n - 1; i >= 0; i--)
	{
		double s = d[i];
		
		r[i] = 0;		/* ... and also set r[i] = 0 for i<n */
		for (k = i + 1; k < n; k++)
		{
			s += C[i][k] * x[k];
		}
		if (C[i][i] == 0)
		{
			return false;
		}
		x[i] = -s / C[i][i];
	}
	
	for (i = n; i < N; i++)
	{
		r[i] = d[i];    /* set the other r[i] to d[i] */
	}
	
	if (!want_r)		/* if r isn't needed, stop here */
	{
		return true;
	}
	
	/* rotate back the r vector */
	for (j = n - 1; j >= 0; j--)
	{
		for (i = N - 1; i >= 0; i--)
		{
			if ((rho = C[i][j]) == 1)  		/* reconstruct gamma, sigma from stored rho */
			{
				gamma = 0;
				sigma = 1;
			}
			else if (fabs(rho) < 1)
			{
				sigma = rho;
				gamma = sqrt(1 - sigma * sigma);
			}
			else
			{
				gamma = 1 / fabs(rho);
				sigma = fsign(rho) * sqrt(1 - gamma * gamma);
			}
			temp = gamma * r[j] + sigma * r[i];		/* rotate back indices (i,j) */
			r[i] = -sigma * r[j] + gamma * r[i];
			r[j] = temp;
		}
	}
	return true;
}

static float16 CalcHomographyHugin(float2 src[4],float2 dst[4])
{
	int iNMatches = 4;

	
	// for each set of points (img1, img2), find the vector
	// to apply to all points to have coordinates centered
	// on the barycenter.
	float _v1x = 0;
	float _v2x = 0;
	float _v1y = 0;
	float _v2y = 0;
	
	static bool NormaliseMatches = false;
	
	if (NormaliseMatches )
	{
			//estimate the center of gravity
		for (size_t i = 0; i < iNMatches; ++i)
		{
			float2 img1 = src[i];
			float2 img2 = dst[i];
			_v1x += img1.s[0];
			_v1y += img1.s[1];
			_v2x += img2.s[0];
			_v2y += img2.s[1];
		}
		_v1x /= (double)iNMatches;
		_v1y /= (double)iNMatches;
		_v2x /= (double)iNMatches;
		_v2y /= (double)iNMatches;
	}
	
	int kNCols = 8;
	int aNRows = iNMatches * 2;
	auto _Amat = new double*[aNRows];
	for(int aRowIter = 0; aRowIter < aNRows; ++aRowIter)
	{
		_Amat[aRowIter] = new double[kNCols];
	}
	auto _Bvec = new double[aNRows];
	auto _Rvec = new double[aNRows];
	auto _Xvec = new double[kNCols];
	int _nMatches = iNMatches;
	
	// fill the matrices and vectors with points
	for (size_t iIndex = 0; iIndex < iNMatches; ++iIndex)
	{
		float2 img1 = src[iIndex];
		float2 img2 = dst[iIndex];

		size_t aRow = iIndex * 2;
		double aI1x = img1.s[0] - _v1x;
		double aI1y = img1.s[1] - _v1y;
		double aI2x = img2.s[0] - _v2x;
		double aI2y = img1.s[1] - _v2y;

		_Amat[aRow][0] = 0;
		_Amat[aRow][1] = 0;
		_Amat[aRow][2] = 0;
		_Amat[aRow][3] = - aI1x;
		_Amat[aRow][4] = - aI1y;
		_Amat[aRow][5] = -1;
		_Amat[aRow][6] = aI1x * aI2y;
		_Amat[aRow][7] = aI1y * aI2y;

		_Bvec[aRow] = aI2y;

		aRow++;

		_Amat[aRow][0] = aI1x;
		_Amat[aRow][1] = aI1y;
		_Amat[aRow][2] = 1;
		_Amat[aRow][3] = 0;
		_Amat[aRow][4] = 0;
		_Amat[aRow][5] = 0;
		_Amat[aRow][6] = - aI1x * aI2x;
		_Amat[aRow][7] = - aI1y * aI2x;

		_Bvec[aRow] = - aI2x;
	}
	
	
	bool Result = Givens(_Amat, _Bvec, _Xvec, _Rvec, _nMatches*2, kNCols, 0);

#define ROW_COL(r,c)	s[r*3+c]
	float16 _H;
	_H.ROW_COL(0,0) = _Xvec[0];
	_H.ROW_COL(0,1) = _Xvec[1];
	_H.ROW_COL(0,2) = _Xvec[2];
	_H.ROW_COL(1,0) = _Xvec[3];
	_H.ROW_COL(1,1) = _Xvec[4];
	_H.ROW_COL(1,2) = _Xvec[5];
	_H.ROW_COL(2,0) = _Xvec[6];
	_H.ROW_COL(2,1) = _Xvec[7];
	_H.ROW_COL(2,2) = 1.0;
/*
	ret_H.s[0] = v[0*N + col];
	ret_H.s[1] = v[1*N + col];
	ret_H.s[2] = v[2*N + col];
	ret_H.s[3] = v[3*N + col];
	ret_H.s[4] = v[4*N + col];
	ret_H.s[5] = v[5*N + col];
	ret_H.s[6] = v[6*N + col];
	ret_H.s[7] = v[7*N + col];
	ret_H.s[8] = v[8*N + col];
*/
	return _H;
}

static float16 CalcHomography(float2 src[4],float2 dst[4])
{
#define M 9 // rows
#define N 9 // cols
	// This version does not normalised the input data, which is contrary to what Multiple View Geometry says.
	// I included it to see what happens when you don't do this step.
	float X[M*N]; // M,N #define inCUDA_SVD.cu
	
	for(int i=0; i < 4; i++)
	{
		float srcx = src[i].s[0];
		float srcy = src[i].s[1];
		float dstx = dst[i].s[0];
		float dsty = dst[i].s[1];
		
		int y1 = (i*2 + 0)*N;
		int y2 = (i*2 + 1)*N;
		
		// First row
		X[y1+0] = 0.f;
		X[y1+1] = 0.f;
		X[y1+2] = 0.f;
		
		X[y1+3] = -srcx;
		X[y1+4] = -srcy;
		X[y1+5] = -1.f;
		
		X[y1+6] = dsty*srcx;
		X[y1+7] = dsty*srcy;
		X[y1+8] = dsty;
		
		// Second row
		X[y2+0] = srcx;
		X[y2+1] = srcy;
		X[y2+2] = 1.f;
		
		X[y2+3] = 0.f;
		X[y2+4] = 0.f;
		X[y2+5] = 0.f;
		
		X[y2+6] = -dstx*srcx;
		X[y2+7] = -dstx*srcy;
		X[y2+8] = -dstx;
	}
	
	// Fill the last row
	float srcx = src[3].s[0];
	float srcy = src[3].s[1];
	float dstx = dst[3].s[0];
	float dsty = dst[3].s[1];
	
	int y = 8*N;
	X[y+0] = -dsty*srcx;
	X[y+1] = -dsty*srcy;
	X[y+2] = -dsty;
	
	X[y+3] = dstx*srcx;
	X[y+4] = dstx*srcy;
	X[y+5] = dstx;
	
	X[y+6] = 0;
	X[y+7] = 0;
	X[y+8] = 0;
	
	float w[N];
	float v[N*N];
	
	float16 ret_H;
	int ret = dsvd(X, M, N, w, v);
	
	if(ret == 1)
	{
		// Sort
		float smallest = w[0];
		int col = 0;

		for(int i=1; i < N; i++)
		{
			if(w[i] < smallest)
			{
				smallest = w[i];
				col = i;
			}
		}

		ret_H.s[0] = v[0*N + col];
		ret_H.s[1] = v[1*N + col];
		ret_H.s[2] = v[2*N + col];
		ret_H.s[3] = v[3*N + col];
		ret_H.s[4] = v[4*N + col];
		ret_H.s[5] = v[5*N + col];
		ret_H.s[6] = v[6*N + col];
		ret_H.s[7] = v[7*N + col];
		ret_H.s[8] = v[8*N + col];


		static bool NoTransform = false;
		static bool NoRotation = false;

		//	remove transform
		if ( NoTransform )
		{
			ret_H.s[2] = 0;
			ret_H.s[5] = 0;
			ret_H.s[8] = 1;
		}
		 
		 if ( NoRotation )
		 {
			ret_H.s[0] = 1;
			ret_H.s[1] = 0;
			//ret_H.s[2] = 0;

			ret_H.s[3] = 0;
			ret_H.s[4] = 1;
			//ret_H.s[5] = 0;

			ret_H.s[6] = 0;
			ret_H.s[7] = 0;
			//ret_H.s[8] = 1;
		 }
	 }
	else
	{
		//	identity
		ret_H.s[0] = 1;
		ret_H.s[1] = 0;
		ret_H.s[2] = 0;
		
		ret_H.s[3] = 0;
		ret_H.s[4] = 1;
		ret_H.s[5] = 0;
		
		ret_H.s[6] = 0;
		ret_H.s[7] = 0;
		ret_H.s[8] = 1;
	}
	
	return ret_H;
	
#undef M
#undef N
}


#define Index3x3(r,c)	((r*3)+c)

static float16 GetMatrix3x3Inverse(float16 m)
{
	float det =
	m.s[Index3x3(0,0)]*
	m.s[Index3x3(1,1)]*
	m.s[Index3x3(2,2)]+
	m.s[Index3x3(1,0)]*
	m.s[Index3x3(2,1)]*
	m.s[Index3x3(0,2)]+
	m.s[Index3x3(2,0)]*
	m.s[Index3x3(0,1)]*
	m.s[Index3x3(1,2)]-
	m.s[Index3x3(0,0)]*
	m.s[Index3x3(2,1)]*
	m.s[Index3x3(1,2)]-
	m.s[Index3x3(2,0)]*
	m.s[Index3x3(1,1)]*
	m.s[Index3x3(0,2)]-
	m.s[Index3x3(1,0)]*
	m.s[Index3x3(0,1)]*
	m.s[Index3x3(2,2)];
	
	float16 inv;// = 0;
	inv.s[Index3x3(0,0)] = m.s[Index3x3(1,1)]*m.s[Index3x3(2,2)] - m.s[Index3x3(1,2)]*m.s[Index3x3(2,1)];
	inv.s[Index3x3(0,1)] = m.s[Index3x3(0,2)]*m.s[Index3x3(2,1)] - m.s[Index3x3(0,1)]*m.s[Index3x3(2,2)];
	inv.s[Index3x3(0,2)] = m.s[Index3x3(0,1)]*m.s[Index3x3(1,2)] - m.s[Index3x3(0,2)]*m.s[Index3x3(1,1)];
	//	inv[Index3x3(0,3].w = 0.f;
	
	inv.s[Index3x3(1,0)] = m.s[Index3x3(1,2)]*m.s[Index3x3(2,0)] - m.s[Index3x3(1,0)]*m.s[Index3x3(2,2)];
	inv.s[Index3x3(1,1)] = m.s[Index3x3(0,0)]*m.s[Index3x3(2,2)] - m.s[Index3x3(0,2)]*m.s[Index3x3(2,0)];
	inv.s[Index3x3(1,2)] = m.s[Index3x3(0,2)]*m.s[Index3x3(1,0)] - m.s[Index3x3(0,0)]*m.s[Index3x3(1,2)];
	//	inv[Index3x3(1,3)].w = 0.f;
	
	inv.s[Index3x3(2,0)] = m.s[Index3x3(1,0)]*m.s[Index3x3(2,1)] - m.s[Index3x3(1,1)]*m.s[Index3x3(2,0)];
	inv.s[Index3x3(2,1)] = m.s[Index3x3(0,1)]*m.s[Index3x3(2,0)] - m.s[Index3x3(0,0)]*m.s[Index3x3(2,1)];
	inv.s[Index3x3(2,2)] = m.s[Index3x3(0,0)]*m.s[Index3x3(1,1)] - m.s[Index3x3(0,1)]*m.s[Index3x3(1,0)];
	//	inv[Index3x3(2,3)].w = 0.f;
	
	inv.s[0] *= 1.0f / det;
	inv.s[1] *= 1.0f / det;
	inv.s[2] *= 1.0f / det;
	inv.s[3] *= 1.0f / det;
	inv.s[4] *= 1.0f / det;
	inv.s[5] *= 1.0f / det;
	inv.s[6] *= 1.0f / det;
	inv.s[7] *= 1.0f / det;
	inv.s[8] *= 1.0f / det;
	return inv;
}

__kernel void HoughLineHomography(int TruthPairIndexOffset,
								  int HoughPairIndexOffset,
								  global int4* TruthPairIndexes,
								  global int4* HoughPairIndexes,
								  int TruthPairIndexCount,
								  int HoughPairIndexCount,
								  global THoughLine* HoughLines,
								  global THoughLine* TruthLines,
								  global float16* Homographys,
								  global float16* HomographyInvs
								  )
{
	int TruthPairIndex = TruthPairIndexOffset;
	int HoughPairIndex = HoughPairIndexOffset;
	
	//	indexes are vvhh
	int4 TruthIndexes = TruthPairIndexes[TruthPairIndex];
	int4 HoughIndexes = HoughPairIndexes[HoughPairIndex];
	
	//	grab the lines and find their intersections to get our four corners
	THoughLine SampleTruthLines[4] =
	{
		TruthLines[TruthIndexes.s[0]],
		TruthLines[TruthIndexes.s[1]],
		TruthLines[TruthIndexes.s[2]],
		TruthLines[TruthIndexes.s[3]],
	};
	THoughLine SampleHoughLines[4] =
	{
		HoughLines[HoughIndexes.s[0]],
		HoughLines[HoughIndexes.s[1]],
		HoughLines[HoughIndexes.s[2]],
		HoughLines[HoughIndexes.s[3]],
	};
	//	find v/h intersections
	float2 TruthCorners[4] =
	{
		GetLineLineInfiniteIntersection( SampleTruthLines[0], SampleTruthLines[2] ),
		GetLineLineInfiniteIntersection( SampleTruthLines[0], SampleTruthLines[3] ),
		GetLineLineInfiniteIntersection( SampleTruthLines[1], SampleTruthLines[2] ),
		GetLineLineInfiniteIntersection( SampleTruthLines[1], SampleTruthLines[3] ),
	};
	float2 HoughCorners[4] =
	{
		GetLineLineInfiniteIntersection( SampleHoughLines[0], SampleHoughLines[2] ),
		GetLineLineInfiniteIntersection( SampleHoughLines[0], SampleHoughLines[3] ),
		GetLineLineInfiniteIntersection( SampleHoughLines[1], SampleHoughLines[2] ),
		GetLineLineInfiniteIntersection( SampleHoughLines[1], SampleHoughLines[3] ),
	};
	
	//float16 Homography = CalcHomography( HoughCorners, TruthCorners );
	//float16 Homography = CalcHomographyHugin( HoughCorners, TruthCorners );
	float16 Homography = of_findHomography( HoughCorners, TruthCorners );
	float16 HomographyInv = GetMatrix3x3Inverse( Homography );
	Homographys[(TruthPairIndex*HoughPairIndexCount)+HoughPairIndex] = Homography;
	HomographyInvs[(TruthPairIndex*HoughPairIndexCount)+HoughPairIndex] = HomographyInv;
}




void TFilterStage_GetHoughLineHomographys::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	copy kernel in case it's replaced during run
	auto Kernel = GetKernel(ContextCl);
	if ( !Soy::Assert( Kernel != nullptr, std::string(__func__) + " missing kernel" ) )
		return;

	static bool SwapTruthHorzAndVert = false;
	//	gr: need to do all the sets flipped. todo: come up with an aspect-ratio thing to work out if we should be doing vert or horz, per set?
	static bool SwapHoughHorzAndVert = false;
	
	//	merge truth lines into one big set for kernel
	auto& TruthLinesStageData = Frame.GetData<TFilterStageRuntimeData_HoughLines>( mTruthLineStage );
	auto& HorzTruthLines = SwapTruthHorzAndVert ? TruthLinesStageData.mVertLines : TruthLinesStageData.mHorzLines;
	auto& VertTruthLines = SwapTruthHorzAndVert ? TruthLinesStageData.mHorzLines : TruthLinesStageData.mVertLines;
	Array<THoughLine> AllTruthLines;
	AllTruthLines.PushBackArray( VertTruthLines );
	AllTruthLines.PushBackArray( HorzTruthLines );
	
	auto& HoughLinesStageData = Frame.GetData<TFilterStageRuntimeData_HoughLines>( mHoughLineStage );
	auto& VertHoughLines = SwapHoughHorzAndVert ? HoughLinesStageData.mHorzLines : HoughLinesStageData.mVertLines;
	auto& HorzHoughLines = SwapHoughHorzAndVert ? HoughLinesStageData.mVertLines : HoughLinesStageData.mHorzLines;
	Array<THoughLine> AllHoughLines;
	AllHoughLines.PushBackArray( VertHoughLines );
	AllHoughLines.PushBackArray( HorzHoughLines );
	Opencl::TBufferArray<THoughLine> HoughLinesBuffer( GetArrayBridge(AllHoughLines), ContextCl, "AllHoughLines" );
	Opencl::TBufferArray<THoughLine> TruthLinesBuffer( GetArrayBridge(AllTruthLines), ContextCl, "AllTruthLines" );
	
	//	make a list of truth indexes sorted by score
	//	gr: right now I'm pretty sure they are in score order, but later we'll make sure with a sort array
	Array<size_t> TruthVertLineIndexes;
	Array<size_t> TruthHorzLineIndexes;
	Array<size_t> HoughVertLineIndexes;
	Array<size_t> HoughHorzLineIndexes;

	for ( int i=0;	i<AllTruthLines.GetSize();	i++ )
	{
		if ( i < VertTruthLines.GetSize() )
			TruthVertLineIndexes.PushBack(i);
		else
			TruthHorzLineIndexes.PushBack(i);
	}
	
	for ( int i=0;	i<AllHoughLines.GetSize();	i++ )
	{
		if ( i < VertHoughLines.GetSize() )
			HoughVertLineIndexes.PushBack(i);
		else
			HoughHorzLineIndexes.PushBack(i);
	}

	//	cap sizes
	TUniformWrapper<int> MaxLinesInTruthSetsUniform("MaxLinesInTruthSets",10);
	TUniformWrapper<int> MaxLinesInHoughSetsUniform("MaxLinesInHoughSets",10);
	Frame.SetUniform( MaxLinesInTruthSetsUniform, MaxLinesInTruthSetsUniform, mFilter, *this );
	Frame.SetUniform( MaxLinesInHoughSetsUniform, MaxLinesInHoughSetsUniform, mFilter, *this );
	size_t MaxLinesInTruthSets = MaxLinesInTruthSetsUniform.mValue;
	size_t MaxLinesInHoughSets = MaxLinesInHoughSetsUniform.mValue;
	Soy::Assert( MaxLinesInTruthSets>=2, "MaxLinesInSets must be at least 2");
	Soy::Assert( MaxLinesInHoughSets>=2, "MaxLinesInHoughSets must be at least 2");
	TruthVertLineIndexes.SetSize( std::min(MaxLinesInTruthSets,TruthVertLineIndexes.GetSize()) );
	TruthHorzLineIndexes.SetSize( std::min(MaxLinesInTruthSets,TruthHorzLineIndexes.GetSize()) );
	HoughVertLineIndexes.SetSize( std::min(MaxLinesInHoughSets,HoughVertLineIndexes.GetSize()) );
	HoughHorzLineIndexes.SetSize( std::min(MaxLinesInHoughSets,HoughHorzLineIndexes.GetSize()) );
	
	//	get pairs of 2 vertical lines and 2 horizontal lines
	Soy::Assert( TruthVertLineIndexes.GetSize()>=2, "Not enough vertical truth lines");
	Soy::Assert( TruthHorzLineIndexes.GetSize()>=2, "Not enough horizontal truth lines");
	Soy::Assert( HoughVertLineIndexes.GetSize()>=2, "Not enough vertical lines");
	Soy::Assert( HoughHorzLineIndexes.GetSize()>=2, "Not enough horizontal lines");
	
	//	example
	Array<cl_int4> TruthLinePairs;	//	vv, hh
	Array<cl_int4> HoughLinePairs;	//	vv, hh
	
	
	for ( int va=0;	va<TruthVertLineIndexes.GetSize();	va++ )
		for ( int vb=va+1;	vb<TruthVertLineIndexes.GetSize();	vb++ )
			for ( int ha=0;	ha<TruthHorzLineIndexes.GetSize();	ha++ )
				for ( int hb=ha+1;	hb<TruthHorzLineIndexes.GetSize();	hb++ )
				{
					TruthLinePairs.PushBack( Soy::VectorToCl( vec4x<int>( TruthVertLineIndexes[va], TruthVertLineIndexes[vb], TruthHorzLineIndexes[ha], TruthHorzLineIndexes[hb] ) ) );
				}
	
	for ( int va=0;	va<HoughVertLineIndexes.GetSize();	va++ )
		for ( int vb=va+1;	vb<HoughVertLineIndexes.GetSize();	vb++ )
			for ( int ha=0;	ha<HoughHorzLineIndexes.GetSize();	ha++ )
				for ( int hb=ha+1;	hb<HoughHorzLineIndexes.GetSize();	hb++ )
				{
					static bool FlipHoughLines = false;
					static bool MirroHoughLines = true;
					int _va = FlipHoughLines ? vb : va;
					int _vb = FlipHoughLines ? va : vb;
					int _ha = MirroHoughLines ? hb : ha;
					int _hb = MirroHoughLines ? ha : hb;
					HoughLinePairs.PushBack( Soy::VectorToCl( vec4x<int>( HoughVertLineIndexes[_vb], HoughVertLineIndexes[_va], HoughHorzLineIndexes[_ha], HoughHorzLineIndexes[_hb] ) ) );
				}
	
	Opencl::TBufferArray<cl_int4> TruthLinePairsBuffer( GetArrayBridge(TruthLinePairs), ContextCl, "TruthLinePairs" );
	Opencl::TBufferArray<cl_int4> HoughLinePairsBuffer( GetArrayBridge(HoughLinePairs), ContextCl, "HoughLinePairs" );
	
	Array<cl_float16> Homographys;
	Array<cl_float16> HomographyInvs;
	Homographys.SetSize( TruthLinePairsBuffer.GetSize() * HoughLinePairsBuffer.GetSize() );
	HomographyInvs.SetSize( TruthLinePairsBuffer.GetSize() * HoughLinePairsBuffer.GetSize() );
	Opencl::TBufferArray<cl_float16> HomographysBuffer( GetArrayBridge(Homographys), ContextCl, "Homographys" );
	Opencl::TBufferArray<cl_float16> HomographyInvsBuffer( GetArrayBridge(HomographyInvs), ContextCl, "HomographyInvs" );
	
	auto Init = [this,&Frame,&TruthLinePairsBuffer,&HoughLinePairsBuffer,&HomographysBuffer,&HomographyInvsBuffer,&HoughLinesBuffer,&TruthLinesBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
		
		Kernel.SetUniform("TruthPairIndexes", TruthLinePairsBuffer );
		Kernel.SetUniform("HoughPairIndexes", HoughLinePairsBuffer );
		Kernel.SetUniform("TruthPairIndexCount", size_cast<int>(TruthLinePairsBuffer.GetSize()) );
		Kernel.SetUniform("HoughPairIndexCount", size_cast<int>(HoughLinePairsBuffer.GetSize()) );
		Kernel.SetUniform("HoughLines", HoughLinesBuffer );
		Kernel.SetUniform("TruthLines", TruthLinesBuffer );
		
		Kernel.SetUniform("Homographys", HomographysBuffer );
		Kernel.SetUniform("HomographyInvs", HomographyInvsBuffer );
		
		Iterations.PushBack( vec2x<size_t>(0, TruthLinePairsBuffer.GetSize() ) );
		Iterations.PushBack( vec2x<size_t>(0, HoughLinePairsBuffer.GetSize() ) );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("TruthPairIndexOffset", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("HoughPairIndexOffset", size_cast<cl_int>(Iteration.mFirst[1]) );
	};
	
	auto Finished = [&HomographysBuffer,&Homographys,&HomographyInvsBuffer,&HomographyInvs](Opencl::TKernelState& Kernel)
	{
		Opencl::TSync Semaphore;
		HomographysBuffer.Read( GetArrayBridge(Homographys), Kernel.GetContext(), &Semaphore );
		Semaphore.Wait();
		Opencl::TSync Semaphore2;
		HomographyInvsBuffer.Read( GetArrayBridge(HomographyInvs), Kernel.GetContext(), &Semaphore2 );
		Semaphore2.Wait();
	};
	
	
	
	
	//	gr: cpu version works much better!
	static bool CpuVersion = true;
	
	if ( CpuVersion )
	{
		cl_int4* TruthPairIndexes = TruthLinePairs.GetArray();//TruthLinePairsBuffer;
		cl_int4* HoughPairIndexes = HoughLinePairs.GetArray();//HoughLinePairsBuffer
		int TruthPairIndexCount = size_cast<int>(TruthLinePairsBuffer.GetSize());
		int HoughPairIndexCount = size_cast<int>(HoughLinePairsBuffer.GetSize());
		THoughLine* HoughLines = AllHoughLines.GetArray();	//	HoughLinesBuffer;
		THoughLine* TruthLines = AllTruthLines.GetArray();	//	TruthLinesBuffer;
		cl_float16* Homographys_ = Homographys.GetArray();	//	HomographysBuffer
		cl_float16* HomographyInvs_ = HomographyInvs.GetArray();	//	HomographyInvsBuffer

		
		for ( int x=0;	x<TruthLinePairsBuffer.GetSize();	x++ )
		{
			for ( int y=0;	y<HoughLinePairsBuffer.GetSize();	y++ )
			{
				int TruthPairIndexOffset = x;
				int HoughPairIndexOffset = y;
				HoughLineHomography( TruthPairIndexOffset,
									HoughPairIndexOffset,
									TruthPairIndexes,
									HoughPairIndexes,
									TruthPairIndexCount,
									HoughPairIndexCount,
									HoughLines,
									TruthLines,
									Homographys_,
									HomographyInvs_
									);
			}
		}
	}
	else
	{
		//	run opencl
		Soy::TSemaphore Semaphore;
		std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( ContextCl, *Kernel, Init, Iteration, Finished ) );
		ContextCl.PushJob( Job, Semaphore );
		Semaphore.Wait(/*"opencl runner"*/);
	}
	
	//	allocate data
	if ( !Data )
		Data.reset( new TFilterStageRuntimeData_GetHoughCornerHomographys() );
	auto& StageData = dynamic_cast<TFilterStageRuntimeData_GetHoughCornerHomographys&>( *Data.get() );
	
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
	


	auto& CornerStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughCorners>( mHoughCornerDataStage );
	auto& TruthStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughCorners>( mTruthCornerDataStage );
	auto& HomographyStageData = Frame.GetData<TFilterStageRuntimeData_GetHoughCornerHomographys>( mHomographyDataStage );
	auto& HoughCorners = CornerStageData.mCorners;
	auto& TruthCorners = TruthStageData.mCorners;
	//	dont modify original!
	auto Homographys = HomographyStageData.mHomographys;
	auto HomographyInvs = HomographyStageData.mHomographyInvs;
	Opencl::TBufferArray<cl_float4> HoughCornersBuffer( GetArrayBridge(HoughCorners), ContextCl, "Corners" );
	Opencl::TBufferArray<cl_float4> TruthCornersBuffer( GetArrayBridge(TruthCorners), ContextCl, "TruthCorners" );
	Opencl::TBufferArray<cl_float16> HomographysBuffer( GetArrayBridge(Homographys), ContextCl, "Homographys" );
	Opencl::TBufferArray<cl_float16> HomographyInvsBuffer( GetArrayBridge(HomographyInvs), ContextCl, "HomographyInvs" );

	auto Init = [this,&Frame,&HoughCornersBuffer,&TruthCornersBuffer,&HomographysBuffer,&HomographyInvsBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter, *this ) )
				continue;
		}
		
		Kernel.SetUniform("HoughCorners", HoughCornersBuffer );
		Kernel.SetUniform("HoughCornerCount", size_cast<int>(HoughCornersBuffer.GetSize()) );
		Kernel.SetUniform("TruthCorners", TruthCornersBuffer );
		Kernel.SetUniform("TruthCornerCount", size_cast<int>(TruthCornersBuffer.GetSize()) );
		Kernel.SetUniform("Homographys", HomographysBuffer );
		Kernel.SetUniform("HomographyInvs", HomographyInvsBuffer );
		
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
		Opencl::TSync Semaphore2;
		HomographyInvsBuffer.Read( GetArrayBridge(HomographyInvs), Kernel.GetContext(), &Semaphore2 );
		Semaphore2.Wait();
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
	
	auto CompareHomographyScores = [](const cl_float16& a,const cl_float16& b)
	{
		auto& aScore = a.s[15];
		auto& bScore = b.s[15];
		if ( aScore > bScore )	return -1;
		if ( aScore < bScore )	return 1;
		return 0;
	};

	//	here we should sort, and keep the best based on a histogram
	//	get all the valid homographys
	//	gr: these SHOULD sort in pairs... lets hope so
	SortArrayLambda<cl_float16> FinalHomographys( GetArrayBridge(StageData.mHomographys), CompareHomographyScores );
	SortArrayLambda<cl_float16>  FinalHomographyInvs( GetArrayBridge(StageData.mHomographyInvs), CompareHomographyScores );
	for ( int i=0;	i<Homographys.GetSize();	i++ )
	{
		//	if it's all zero's its invalid
		auto& Homography = Homographys[i];
		auto& HomographyInv = HomographyInvs[i];
		float Score = Homography.s[15];

		if ( Score == 0 )
			continue;
		//std::Debug << "Homography #" << i << " score: " << Score << std::endl;
		
		FinalHomographys.Push( Homography );
		FinalHomographyInvs.Push( HomographyInv );
	}
	
	//	cap number of homographys (gets top X from sorted list)
	TUniformWrapper<int> LimitScoredHomographyCountUniform("LimitScoredHomography", 100 );
	Frame.SetUniform( LimitScoredHomographyCountUniform, LimitScoredHomographyCountUniform, mFilter, *this );
	FinalHomographys.SetSize( std::min<size_t>(LimitScoredHomographyCountUniform.mValue, FinalHomographys.GetSize() ) );

	std::Debug << "Extracted & capped x" << FinalHomographys.GetSize() << "/" << Homographys.GetSize() << " SCORED homographys: ";
	static bool DebugExtractedHomographys = false;
	if ( DebugExtractedHomographys )
	{
		std::Debug.PushStreamSettings();
		
		for ( int i=0;	i<FinalHomographys.GetSize();	i++ )
		{
			auto& Homography = FinalHomographys[i];
			auto& InvHomograph = FinalHomographyInvs[i];
			static int Precision = 5;
			
			std::Debug << std::endl;
			std::Debug << "homo #" << i << "; ";
			for ( int s=0;	s<sizeofarray(Homography.s);	s++ )
				std::Debug << std::fixed << std::setprecision( Precision ) << Homography.s[s] << " x ";
			
			std::Debug << std::endl;
			std::Debug << "inv #" << i << "; ";
			for ( int s=0;	s<sizeofarray(InvHomograph.s);	s++ )
				std::Debug << std::fixed << std::setprecision( Precision ) << InvHomograph.s[s] << " x ";
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
	
	
	
	auto& CornerStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughCorners>( mHoughCornerDataStage );
	auto& TruthStageData = Frame.GetData<TFilterStageRuntimeData_ExtractHoughCorners>( mTruthCornerDataStage );
	auto& HomographyStageData = Frame.GetData<TFilterStageRuntimeData_GetHoughCornerHomographys>( mHomographyDataStage );
	auto& HoughCorners = CornerStageData.mCorners;
	auto& TruthCorners = TruthStageData.mCorners;
	auto& Homographys = HomographyStageData.mHomographys;
	Opencl::TBufferArray<cl_float4> HoughCornersBuffer( GetArrayBridge(HoughCorners), ContextCl, "Corners" );
	Opencl::TBufferArray<cl_float4> TruthCornersBuffer( GetArrayBridge(TruthCorners), ContextCl, "TruthCorners" );
	Opencl::TBufferArray<cl_float16> HomographysBuffer( GetArrayBridge(Homographys), ContextCl, "Homographys" );
	
	auto Init = [this,&Frame,&StageData,&ContextGl,&HoughCornersBuffer,&TruthCornersBuffer,&HomographysBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
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
		Kernel.SetUniform("TruthCorners", TruthCornersBuffer );
		Kernel.SetUniform("TruthCornerCount", size_cast<int>(TruthCornersBuffer.GetSize()) );
		Kernel.SetUniform("HoughCorners", HoughCornersBuffer );
		Kernel.SetUniform("Homographys", HomographysBuffer );
		
		Iterations.PushBack( vec2x<size_t>(0, HoughCornersBuffer.GetSize() ) );

		//	+1 to draw truth (no-homography)
		Iterations.PushBack( vec2x<size_t>(0, HomographysBuffer.GetSize()+1 ) );
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
	auto& MaskStageData = Frame.GetData<TFilterStageRuntimeData&>(mMaskStage);
	auto MaskPixels = MaskStageData.GetPixels( mFilter.GetOpenglContext() );
	Soy::Assert( MaskPixels != nullptr, "Missing mask pixels" );

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
	
	auto Init = [this,&Frame,&MaskPixels,&StageData,&ContextGl,&HomographysBuffer,&HomographyInvsBuffer](Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>& Iterations)
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
	
		Kernel.SetUniform("Mask", *MaskPixels, OpenclBufferReadWrite::ReadWrite );
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
		Kernel.SetUniform("HomographyIndexOffset", size_cast<cl_int>(Iteration.mFirst[2]) );
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



void TFilterStage_JoinHoughLines::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	TUniformWrapper<std::string> HoughDataStageName("HoughData", std::string() );
	Frame.SetUniform( HoughDataStageName, HoughDataStageName, mFilter, *this );
	auto& HoughStageData = Frame.GetData<TFilterStageRuntimeData_HoughLines>( HoughDataStageName );
	
	auto& VertLines = HoughStageData.mVertLines;
	auto& HorzLines = HoughStageData.mHorzLines;
	
	for ( int v=VertLines.GetSize()-1;	v>=0;	v-- )
	{
		auto& v_HoughLine = VertLines[v];
		auto vstart = GetHoughLineStart( v_HoughLine );
		auto vend = GetHoughLineEnd( v_HoughLine );
		
		float BestStartDistance = 99999;
		int BestStartVertLineIndex = -1;
		int BestStartVertLineAngleIndex;
		float BestEndDistance = 99999;
		int BestEndVertLineIndex = -1;
		int BestEndVertLineAngleIndex;
	
		auto UpdateBest = [](const THoughLine& TargetLine,const vec2f& TargetStart,float& BestDistance,int& BestVertLineIndex,int& BestVertLineAngleIndex,vec2f MatchPos,size_t MatchIndex,const THoughLine& MatchHoughLine)
		{
			static float MaxDistance = 2;
			float MatchDistance = GetDistance( TargetStart, MatchPos );
			auto MatchAngleIndex = GetHoughLineAngleIndex( MatchHoughLine );
			int AngleDifference = MatchAngleIndex - GetHoughLineAngleIndex( TargetLine );
			float MatchScore = GetHoughLineScore( MatchHoughLine );
			if ( MatchDistance > BestDistance )
				return;
			if ( MatchDistance > MaxDistance )
				return;
			BestVertLineIndex = MatchIndex;
			BestDistance = MatchDistance;
			BestVertLineAngleIndex = MatchAngleIndex;
		};

		auto UpdateBestStart = [&](vec2f MatchPos,size_t MatchIndex,const THoughLine& MatchHoughLine)
		{
			UpdateBest( v_HoughLine, vstart, BestStartDistance, BestStartVertLineIndex, BestStartVertLineAngleIndex, MatchPos, MatchIndex, MatchHoughLine );
		};
		
		auto UpdateBestEnd = [&](vec2f MatchPos,size_t MatchIndex,const THoughLine& MatchHoughLine)
		{
			UpdateBest( v_HoughLine, vend, BestEndDistance, BestEndVertLineIndex, BestEndVertLineAngleIndex, MatchPos, MatchIndex, MatchHoughLine );
		};
		

		for ( int i=0;	i<VertLines.GetSize();	i++ )
		{
			if ( i == v )
				continue;
			
			auto& i_HoughLine = VertLines[i];
			
			//	filter here by neighbouring window, max angle diff etc
			
			auto istart = GetHoughLineStart( i_HoughLine );
			auto iend = GetHoughLineEnd( i_HoughLine );
			
			//	find new best start joint
			UpdateBestStart( istart, i, i_HoughLine );
			UpdateBestStart( iend, i, i_HoughLine );
			UpdateBestEnd( istart, i, i_HoughLine );
			UpdateBestEnd( iend, i, i_HoughLine );
			
		}
		
		//	update houghline
		SetHoughLineStartJointVertLineIndex( v_HoughLine, BestStartVertLineIndex );
		SetHoughLineEndJointVertLineIndex( v_HoughLine, BestEndVertLineIndex );
		
		//	remove if no joints
		if ( BestStartVertLineIndex < 0 && BestEndVertLineIndex < 0 )
		{
			VertLines.RemoveBlock( v, 1 );
			continue;
		}
		
		//	merge larger index into smaller, maybe invalidate wad data?
		{
		}
	}
}






