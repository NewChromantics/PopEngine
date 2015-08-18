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
		Kernel.SetUniform("MatchesMax", size_cast<cl_int>(RectBuffer.GetMaxSize()) );

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
		
		StageData.mRects.SetSize( RectCount );
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
			Semaphore.Wait(/*"opencl runner"*/);
		}
		catch (std::exception& e)
		{
			std::Debug << "Opencl stage failed: " << e.what() << std::endl;
			return false;
		}
	}
	
	return false;
}
