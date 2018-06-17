#pragma once
#include <SoyTypes.h>
#include <SoyRef.h>


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


#include <SoyOpenglContext.h>

class PopMainThread : public PopWorker::TJobQueue, public PopWorker::TContext
{
public:
	PopMainThread();
	virtual ~PopMainThread()				{}
	
	virtual bool	Lock() override		{	return true;	}
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
		int			BundleAppMain(int argc, const char * argv[]);
		
		extern std::shared_ptr<PopMainThread>	gMainThread;
	};
};
#endif

TPopAppError::Type	PopMain();



