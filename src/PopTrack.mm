#include "PopTrack.h"
#include <SoyDebug.h>
#include <SoyApp.h>
#include <PopMain.h>
#include "TV8Instance.h"
#include <SoyFileSystem.h>

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
		//	proper way would be to use the current working dir. But for now lets force it
		auto RootDir = Platform::GetAppResourcesDirectory();
		//RootDir += "Data_dlib/";
		//RootDir += "Data_Posenet/";
		RootDir += "Data_HelloWorld/";
		
		//Private::gOpenglApp.reset( new TPopTrack("Data_Posenet/Bootup.js") );
		Private::gOpenglApp.reset( new TPopTrack( RootDir, "Bootup.js") );
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




TPopTrack::TPopTrack(const std::string& RootDirectory,const std::string& BootupFilename)
{
	//	todo: watch for when a file changes and recreate instance
	mV8Instance.reset( new TV8Instance(RootDirectory,BootupFilename) );

}

TPopTrack::~TPopTrack()
{
}
