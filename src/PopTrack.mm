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
	mV8Container.reset( new TV8Container() );
	
	ApiCommon::Bind( *mV8Container );
	ApiOpengl::Bind( *mV8Container );
	
	std::string BootupSource;
	if ( !Soy::FileToString( BootupFilename, BootupSource ) )
	{
		std::stringstream Error;
		Error << "Failed to read bootup file " << BootupFilename;
		throw Soy::AssertException( Error.str() );
	}
	mV8Container->LoadScript( BootupSource );
	
	//	example
	mV8Container->ExecuteGlobalFunc("ReturnSomeString");
}

TPopTrack::~TPopTrack()
{
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}
	
}


std::shared_ptr<Opengl::TContext> TPopTrack::GetContext()
{
	if ( !mWindow )
		return nullptr;
	
	return mWindow->GetContext();
}



