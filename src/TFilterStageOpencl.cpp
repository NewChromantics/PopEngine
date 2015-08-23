#include "TFilterStageOpencl.h"
#include "TFilterStageOpengl.h"



TFilterStage_OpenclKernel::TFilterStage_OpenclKernel(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter) :
	TFilterStage		( Name, Filter ),
	mKernelFilename		( KernelFilename ),
	mKernelName			( KernelName ),
	mKernelFileWatch	( KernelFilename )
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
	
	mKernelFileWatch.mOnChanged.AddListener( OnFileChanged );
	
	Reload();
}
	
void TFilterStage_OpenclKernel::Reload()
{
	//	delete the old ones
	mKernel.reset();
	mProgram.reset();
	
	//	load shader
	auto& Context = mFilter.GetOpenclContext();
	
	try
	{
		std::string Source;
		Soy::FileToString( mKernelFilename, Source );
		mProgram.reset( new Opencl::TProgram( Source, Context ) );
		
		//	now load kernel
		mKernel.reset( new Opencl::TKernel( mKernelName, *mProgram ) );
	}
	catch (std::exception& e)
	{
		std::Debug << "Failed to load opencl stage " << mName << ": " << e.what() << std::endl;
		mProgram.reset();
		mKernel.reset();
		return;
	}

	std::Debug << "Loaded kernel (" << mKernelName << ") okay for " << this->mName << std::endl;
	this->mOnChanged.OnTriggered(*this);
}



void TOpenclRunner::Run()
{
	//ofScopeTimerWarning Timer( (std::string("Opencl ") + this->mKernel.mKernelName).c_str(), 0 );
	auto Kernel = mKernel.Lock(mContext);

	//	get iterations we want
	Array<size_t> Iterations;
	Init( Kernel, GetArrayBridge( Iterations ) );
	
	//	divide up the iterations
	Array<Opencl::TKernelIteration> IterationSplits;
	Kernel.GetIterations( GetArrayBridge(IterationSplits), GetArrayBridge(Iterations) );

	//	for now, because buffers get realeased etc when the kernelstate is destructed,
	//	lets just block on the last execution to make sure nothing is in use. Optimise later.
	Opencl::TSync LastSemaphore;
	
	for ( int i=0;	i<IterationSplits.GetSize();	i++ )
	{
		auto& Iteration = IterationSplits[i];
		
		//	setup the iteration
		bool Block = false;
		RunIteration( Kernel, Iteration, Block );
		
		//	execute it
		Opencl::TSync ItSemaphore;
		auto* Semaphore = Block ? &ItSemaphore : nullptr;
		if ( i == IterationSplits.GetSize()-1 )
			Semaphore = &LastSemaphore;
	
		if ( Semaphore )
		{
			Kernel.QueueIteration( Iteration, *Semaphore );
			Semaphore->Wait();
		}
		else
		{
			Kernel.QueueIteration( Iteration );
		}
	}
	
	LastSemaphore.Wait();
	
	OnFinished( Kernel );
}
	


bool TFilterStage_OpenclBlit::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data)
{
	if ( !mKernel )
		return false;

	if ( !Frame.mFramePixels )
		return false;

	SoyPixels OutputPixels;
	OutputPixels.Init( Frame.mFramePixels->GetWidth(), Frame.mFramePixels->GetHeight(), SoyPixelsFormat::RGBA );

	//	clear for drawing on
	static bool CopyFrameBeforeDraw = false;
	if ( CopyFrameBeforeDraw )
	{
		OutputPixels.Copy( *Frame.mFramePixels );
	}
	
	static bool ClearBeforeDraw = false;
	if ( ClearBeforeDraw )
	{
		BufferArray<uint8,4> rgba;
		rgba.PushBack( 255 );
		rgba.PushBack( 0 );
		rgba.PushBack( 0 );
		rgba.PushBack( 255 );
		OutputPixels.SetColour( GetArrayBridge(rgba) );
	}
	
	auto Init = [this,&Frame,&OutputPixels](Opencl::TKernelState& Kernel,ArrayBridge<size_t>& Iterations)
	{
		//	setup params
		for ( int u=0;	u<Kernel.mKernel.mUniforms.GetSize();	u++ )
		{
			auto& Uniform = Kernel.mKernel.mUniforms[u];
				
			if ( Frame.SetUniform( Kernel, Uniform, mFilter ) )
				continue;

			//	maybe surpress this until we need it... or only warn once
			static bool DebugUnsetUniforms = false;
			if ( DebugUnsetUniforms )
				std::Debug << "Warning; unset uniform " << Uniform.mName << std::endl;
		}
		
		//	"frag" is output. todo; non pixel output!
		Kernel.SetUniform("Frag", OutputPixels );
		
		Iterations.PushBack( OutputPixels.GetWidth() );
		Iterations.PushBack( OutputPixels.GetHeight() );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetX", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetY", size_cast<cl_int>(Iteration.mFirst[1]) );
	};
	
	auto Finished = [&OutputPixels](Opencl::TKernelState& Kernel)
	{
		Kernel.ReadUniform("Frag", OutputPixels );
	};
	
	//	run opencl
	{
		auto& ContextCl = mFilter.GetOpenclContext();
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
	
	//	copy output to texture
	{
		//ofScopeTimerWarning Timer("Copy kernel output to texture", 4);
		auto& ContextGl = mFilter.GetOpenglContext();
		
		auto CopyJob = [&Data,&OutputPixels]()
		{
			if ( !Data )
			{
				auto* pData = new TFilterStageRuntimeData_ShaderBlit;
				Data.reset( pData );
				auto& StageTarget = pData->mTexture;
				
				if ( !StageTarget.IsValid() )
				{
					auto Format = SoyPixelsFormat::RGBA;
					SoyPixelsMeta Meta( OutputPixels.GetWidth(), OutputPixels.GetHeight(), Format );
					StageTarget = Opengl::TTexture( Meta, GL_TEXTURE_2D );
				}
			}
			auto& StageTarget = dynamic_cast<TFilterStageRuntimeData_ShaderBlit*>( Data.get() )->mTexture;
			StageTarget.Write( OutputPixels );
			return true;
		};
		Soy::TSemaphore Semaphore;
		ContextGl.PushJob( CopyJob, Semaphore );
		Semaphore.Wait();
	}
	
	return false;
}
	
