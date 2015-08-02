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

bool TFilterStage_OpenclKernel::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data)
{
	return false;
}
	
