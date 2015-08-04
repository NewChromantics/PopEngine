#include "TFilterStageOpencl.h"



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
	}
}


class TOpenclRunner : public PopWorker::TJob
{
public:
	TOpenclRunner(Opencl::TContext& Context,Opencl::TKernel& Kernel) :
		mContext	( Context ),
		mKernel		( Kernel )
	{
	}
	
	virtual bool		Run(std::ostream& Error);

protected:
	//	get iterations and can setup first set of kernel args
	//	number of elements in the array dictates dimensions
	virtual void		Init(Opencl::TKernelState& Kernel,ArrayBridge<size_t>&& Iterations)=0;
	
	//	set any iteration-specific args
	virtual void		RunIteration(Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& WorkGroups,bool& Block)=0;


public:
	Opencl::TKernel&	mKernel;
	Opencl::TContext&	mContext;
};


class TOpenclRunnerLambda : public TOpenclRunner
{
public:
	TOpenclRunnerLambda(Opencl::TContext& Context,Opencl::TKernel& Kernel,std::function<void(Opencl::TKernelState&,ArrayBridge<size_t>&)> InitLambda,std::function<void(Opencl::TKernelState&,const Opencl::TKernelIteration&,bool&)> IterationLambda) :
		TOpenclRunner		( Context, Kernel ),
		mIterationLambda	( IterationLambda ),
		mInitLambda			( InitLambda )
	{
	}
	
	virtual void		Init(Opencl::TKernelState& Kernel,ArrayBridge<size_t>&& Iterations)
	{
		mInitLambda( Kernel, Iterations );
	}

	virtual void		RunIteration(Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& WorkGroups,bool& Block)
	{
		mIterationLambda( Kernel, WorkGroups, Block );
	}
	
public:
	std::function<void(Opencl::TKernelState&,ArrayBridge<size_t>&)>	mInitLambda;
	std::function<void(Opencl::TKernelState&,const Opencl::TKernelIteration&,bool&)>	mIterationLambda;
};



bool TOpenclRunner::Run(std::ostream& Error)
{
	ofScopeTimerWarning Timer( (std::string("Opencl ") + this->mKernel.mKernelName).c_str(), 0 );
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
		auto& Semaphore = (i==IterationSplits.GetSize()-1) ? LastSemaphore : ItSemaphore;
		Kernel.QueueIteration( Iteration, Semaphore );
		if ( Block )
			Semaphore.Wait();
	}
	
	LastSemaphore.Wait();
	
	return true;
}
	


bool TFilterStage_OpenclKernel::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data)
{
	if ( !mKernel )
		return false;

	if ( !Frame.mFramePixels )
		return false;

	//	get the opencl context
	auto& Context = mFilter.GetOpenclContext();

	SoyPixels OutputPixels;
	OutputPixels.Init( Frame.mFramePixels->GetWidth(), Frame.mFramePixels->GetHeight(), SoyPixelsFormat::RGBA );
	//Opencl::DataBuffer OutputPixelsBuffer( OutputPixels, Context );
	
	auto Init = [&Frame,&OutputPixels](Opencl::TKernelState& Kernel,ArrayBridge<size_t>& Iterations)
	{
		//	setup params
		Kernel.SetUniform("Frag", OutputPixels );

		//	todo: auto set this when setting textures/pixels
		vec2f PixelWidthHeight( OutputPixels.GetWidth(), OutputPixels.GetHeight() );
		Kernel.SetUniform("Frag_PixelWidthHeight", PixelWidthHeight );
		
		Iterations.PushBack( OutputPixels.GetWidth() );
		Iterations.PushBack( OutputPixels.GetHeight() );
	};
	
	auto Iteration = [](Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& Iteration,bool& Block)
	{
		Kernel.SetUniform("OffsetX", size_cast<cl_int>(Iteration.mFirst[0]) );
		Kernel.SetUniform("OffsetY", size_cast<cl_int>(Iteration.mFirst[1]) );
	};
	
	
	Soy::TSemaphore Semaphore;
	std::shared_ptr<PopWorker::TJob> Job( new TOpenclRunnerLambda( Context, *	mKernel, Init, Iteration ) );
	Context.PushJob( Job, Semaphore );
	Semaphore.Wait();
	
	return false;
}
	
