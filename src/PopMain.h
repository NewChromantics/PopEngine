#pragma once
#include "SoyTypes.h"
#include "SoyRef.h"
#include "SoyOpenglContext.h"


class TChannel;
class TJobParams;
class TParameterTraits;

namespace TPopAppError
{
	enum Type
	{
		Success = 0,
		BadParams,
		InitError,
	};
}



class PopMainThread : public PopWorker::TJobQueue, public PopWorker::TContext
{
public:
	PopMainThread();
	virtual ~PopMainThread()				{}
	
	virtual void	Lock() override		{	}
	virtual void	Unlock() override	{	}
	
	void			TriggerIteration();
};



//	in PopMain.mm
#if defined(TARGET_OSX_BUNDLE)
namespace Soy
{
	namespace Platform
	{
		extern bool	BundleInitialised;
		int			BundleAppMain();
		
		extern std::shared_ptr<PopMainThread>	gMainThread;
	};
};
#endif


namespace Pop
{
	//	callers invoking the engine now pass in a project/data path, we no longer grab it from args
	extern std::string ProjectPath;
}

TPopAppError::Type	PopMain(ArrayBridge<std::string>& Arguments);



