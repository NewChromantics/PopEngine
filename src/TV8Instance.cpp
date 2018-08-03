#include "TV8Instance.h"
#include "TV8Container.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"
#include "TApiOpencl.h"
#include "TApiDlib.h"
#include "TApiMedia.h"
#include "TApiWebsocket.h"

#include <SoyFilesystem.h>



TV8Instance::TV8Instance(const std::string& ScriptFilename) :
	SoyWorkerThread	( ScriptFilename, SoyWorkerWaitMode::NoWait )
{
	//	bind first
	try
	{
		mV8Container.reset( new TV8Container() );
		ApiCommon::Bind( *mV8Container );
		ApiOpengl::Bind( *mV8Container );
		ApiOpencl::Bind( *mV8Container );
		ApiDlib::Bind( *mV8Container );
		ApiMedia::Bind( *mV8Container );
		ApiWebsocket::Bind( *mV8Container );

		//	gr: start the thread immediately, there should be no problems having the thread running before queueing a job
		this->Start();
		
		std::string BootupSource;
		if ( !Soy::FileToString( ScriptFilename, BootupSource ) )
		{
			std::stringstream Error;
			Error << "Failed to read bootup file " << ScriptFilename;
			throw Soy::AssertException( Error.str() );
		}
		
		auto* Container = mV8Container.get();
		auto LoadScript = [=](v8::Local<v8::Context> Context)
		{
			Container->LoadScript( Context, BootupSource );
		};
		
		//	gr: running on another thread causes crash...
		mV8Container->QueueScoped( LoadScript );
		//mV8Container->RunScoped( LoadScript );
	}
	catch(std::exception& e)
	{
		//	clean up
		mV8Container.reset();
		throw;
	}
}

TV8Instance::~TV8Instance()
{
	mV8Thread.reset();
	mV8Container.reset();
	
}

bool TV8Instance::Iteration()
{
	if ( !mV8Container )
		return false;
	
	mV8Container->ProcessJobs();
	return true;
}
