#include "TFilterStageHoughTransform.h"





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
		Angles.PushBack( -90 );
		Angles.PushBack( -70 );
		Angles.PushBack( -50 );
		Angles.PushBack( -30 );
		Angles.PushBack( -10 );
		Angles.PushBack( 0 );
		Angles.PushBack( 10 );
		Angles.PushBack( 30 );
		Angles.PushBack( 50 );
		Angles.PushBack( 70 );

		for ( float Dist=5;	Dist<1000;	Dist+=50 )
		{
			Distances.PushBack( Dist );
		}
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
			
			if ( Frame.SetUniform( Kernel, Uniform, mFilter ) )
				continue;
		}
	
		Kernel.SetUniform("AngleXDistances", AngleXDistancesBuffer );
		Kernel.SetUniform("AngleDegs", AnglesBuffer );
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
	static bool DebugHistogramCount = true;
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



