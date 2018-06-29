#include "PopTrack.h"
#include <SoyDebug.h>
#include <SoyApp.h>
#include <PopMain.h>
#include <SoyPixels.h>
#include <SoyString.h>
#include <SortArray.h>
#include <SoyOpenglWindow.h>
#include <SoyFilesystem.h>

#define FILTER_MAX_FRAMES	10
#define FILTER_MAX_THREADS	1
#define JOB_THREAD_COUNT	1

#include "TV8Container.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"


namespace PopTrack
{
	namespace Private
	{
		//	keep alive after PopMain()
#if defined(TARGET_OSX_BUNDLE)
		std::shared_ptr<TPopTrack> gOpenglApp;
#endif
		
	}
	
	TPopTrack&	GetApp();
}


TPopTrack& PopTrack::GetApp()
{
	if ( !Private::gOpenglApp )
	{
		Private::gOpenglApp.reset( new TPopTrack("Data/Bootup.js") );
	}
	return *Private::gOpenglApp;
}



TPopAppError::Type PopMain()
{
	auto& App = PopTrack::GetApp();
	
#if !defined(TARGET_OSX_BUNDLE)
	//	run
	App.mConsoleApp.WaitForExit();
#endif

	return TPopAppError::Success;
}










TPopTrack::TPopTrack(const std::string& BootupFilename)
{
	//	todo: watch for when a file changes and recreate instance
	mV8Instance.reset( new TV8Instance(BootupFilename) );

}

TPopTrack::~TPopTrack()
{
	/*
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}*/
	
}

/*
std::shared_ptr<Opengl::TContext> TPopTrack::GetContext()
{
	if ( !mWindow )
		return nullptr;
	
	return mWindow->GetContext();
}
*/

TV8Instance::TV8Instance(const std::string& ScriptFilename) :
	SoyWorkerThread	( ScriptFilename, SoyWorkerWaitMode::Sleep )
{
	//	bind first
	try
	{
		mV8Container.reset( new TV8Container() );
		ApiCommon::Bind( *mV8Container );
		ApiOpengl::Bind( *mV8Container );

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
