#pragma once


#include "TFilter.h"





class TOpenclRunner : public PopWorker::TJob
{
public:
	TOpenclRunner(Opencl::TContext& Context,Opencl::TKernel& Kernel) :
	mContext	( Context ),
	mKernel		( Kernel )
	{
	}
	
	virtual void		Run() override;
	
protected:
	//	get iterations and can setup first set of kernel args
	//	number of elements in the array dictates dimensions
	virtual void		Init(Opencl::TKernelState& Kernel,ArrayBridge<size_t>&& Iterations)=0;
	
	//	set any iteration-specific args
	virtual void		RunIteration(Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& WorkGroups,bool& Block)=0;
	
	//	after last iteration - read back data etc
	virtual void		OnFinished(Opencl::TKernelState& Kernel)=0;
	
public:
	Opencl::TKernel&	mKernel;
	Opencl::TContext&	mContext;
};


class TOpenclRunnerLambda : public TOpenclRunner
{
public:
	TOpenclRunnerLambda(Opencl::TContext& Context,Opencl::TKernel& Kernel,std::function<void(Opencl::TKernelState&,ArrayBridge<size_t>&)> InitLambda,std::function<void(Opencl::TKernelState&,const Opencl::TKernelIteration&,bool&)> IterationLambda,std::function<void(Opencl::TKernelState&)> FinishedLambda) :
	TOpenclRunner		( Context, Kernel ),
	mIterationLambda	( IterationLambda ),
	mInitLambda			( InitLambda ),
	mFinishedLambda		( FinishedLambda )
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
	
	//	after last iteration - read back data etc
	virtual void		OnFinished(Opencl::TKernelState& Kernel)
	{
		mFinishedLambda( Kernel );
	}
	
public:
	std::function<void(Opencl::TKernelState&,ArrayBridge<size_t>&)>	mInitLambda;
	std::function<void(Opencl::TKernelState&,const Opencl::TKernelIteration&,bool&)>	mIterationLambda;
	std::function<void(Opencl::TKernelState&)>						mFinishedLambda;
};





class TFilterStage_OpenclKernel : public TFilterStage
{
public:
	TFilterStage_OpenclKernel(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter);
	
	void				Reload();
	//virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
	bool				operator==(const std::string& Name) const	{	return mName == Name;	}
	
	std::shared_ptr<Opencl::TKernel>	GetKernel(Opencl::TContext& Context)
	{
		std::lock_guard<std::mutex> Lock( mKernelLock );
		return mKernel[Context.GetContext()];
	}
	
public:
	std::string				mKernelFilename;
	std::string				mKernelName;
	Soy::TFileWatch			mKernelFileWatch;
	
	std::mutex				mKernelLock;
	std::map<cl_context,std::shared_ptr<Opencl::TKernel>>	mKernel;
	std::map<cl_context,std::shared_ptr<Opencl::TProgram>>	mProgram;
};



class TFilterStage_OpenclBlit : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_OpenclBlit(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
};

