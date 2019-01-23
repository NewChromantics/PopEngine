#include "TV8Instance.h"
#include "TV8Container.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"
#include "TApiOpencl.h"
#include "TApiDlib.h"
#include "TApiMedia.h"
#include "TApiWebsocket.h"
#include "TApiSocket.h"
#include "TApiHttp.h"
#include "TApiCoreMl.h"

#include "SoyFilesystem.h"



TV8Instance::TV8Instance(const std::string& RootDirectory,const std::string& ScriptFilename) :
	SoyWorkerThread	( ScriptFilename, SoyWorkerWaitMode::Sleep ),
	mRootDirectory	( RootDirectory )
{
	//	bind first
	try
	{
		mV8Container.reset( new TV8Container(mRootDirectory) );
		ApiCommon::Bind( *mV8Container );
		ApiOpengl::Bind( *mV8Container );
		ApiOpencl::Bind( *mV8Container );
		ApiDlib::Bind( *mV8Container );
		ApiMedia::Bind( *mV8Container );
		ApiWebsocket::Bind( *mV8Container );
		ApiHttp::Bind( *mV8Container );
		ApiSocket::Bind( *mV8Container );
		ApiCoreMl::Bind( *mV8Container );

		//	gr: start the thread immediately, there should be no problems having the thread running before queueing a job
		this->Start();
		
		std::string BootupSource;
		Soy::FileToString( mRootDirectory + ScriptFilename, BootupSource );
		
		auto* Container = mV8Container.get();
		auto LoadScript = [=](v8::Local<v8::Context> Context)
		{
			Container->LoadScript( Context, BootupSource, ScriptFilename );
		};
		
		mV8Container->QueueScoped( LoadScript );
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

std::chrono::milliseconds TV8Instance::GetSleepDuration()
{
	return std::chrono::milliseconds(1);
}

bool TV8Instance::Iteration()
{
	if ( !mV8Container )
		return false;
	
	auto IsRunning = [this]()
	{
		return this->IsWorking();
	};
	
	mV8Container->ProcessJobs( IsRunning );
	return true;
}
