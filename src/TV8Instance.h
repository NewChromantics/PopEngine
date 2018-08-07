#pragma once
#include "TV8Container.h"

#include <SoyThread.h>

//	gr: easy tear up/down instance per script
//	should really keep an Isolate and recreate contexts
//	gr: now we're manually pumping the v8 message queue, really, we want a thread
//	that wakes up (soy worker thread)
//	which means really we should implement a v8 platform and manage subthreads and get notified of new ones
class TV8Instance : public SoyWorkerThread
{
public:
	TV8Instance(const std::string& RootDirectory,const std::string& ScriptFilename);
	~TV8Instance();
	
	std::shared_ptr<TV8Container>		mV8Container;
	std::shared_ptr<SoyWorkerThread>	mV8Thread;
	
protected:
	virtual bool	Iteration() override;
	
public:
	std::string		mRootDirectory;
};
