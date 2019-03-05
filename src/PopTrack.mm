#include "PopTrack.h"
#include "SoyDebug.h"
#include "SoyApp.h"
#include "PopMain.h"
#include "TV8Instance.h"
#include "SoyFileSystem.h"
#include "JsCoreInstance.h"

namespace PopTrack
{
	namespace Private
	{
		//	keep alive after PopMain()
#if defined(TARGET_OSX_BUNDLE)
		std::shared_ptr<TPopTrack> gOpenglApp;
#endif
		
	}
	
	TPopTrack&	GetApp(std::string DataPath);
}


TPopTrack& PopTrack::GetApp(std::string DataPath)
{
	if ( !Private::gOpenglApp )
	{
		if ( DataPath.length() == 0 )
		{
			//	proper way would be to use the current working dir. But for now lets force it
			auto RootDir = Platform::GetAppResourcesDirectory();
			//RootDir += "Data_dlib/";
			//RootDir += "Data_Posenet/";
			//RootDir += "Data_HelloWorld/";
			RootDir += "Data_Holosports/";
			//RootDir += "Data_PeopleDetector/";
			DataPath = RootDir;
		}
		DataPath += "/";
		
		//Private::gOpenglApp.reset( new TPopTrack("Data_Posenet/Bootup.js") );
		Private::gOpenglApp.reset( new TPopTrack( DataPath, "Bootup.js") );
	}
	return *Private::gOpenglApp;
}



TPopAppError::Type PopMain(const ArrayBridge<std::string>& Arguments)
{
	std::string DataPath = Arguments.GetSize() > 0 ? Arguments[0] : "";
	auto& App = PopTrack::GetApp(DataPath);
	
#if !defined(TARGET_OSX_BUNDLE)
	//	run
	App.mConsoleApp.WaitForExit();
#endif

	
	return TPopAppError::Success;
}




TPopTrack::TPopTrack(const std::string& RootDirectory,const std::string& BootupFilename)
{
	//	todo: watch for when a file changes and recreate instance
	//mV8Instance.reset( new TV8Instance(RootDirectory,BootupFilename) );
	mJsCoreInstance.reset( new JsCore::TInstance(RootDirectory,BootupFilename) );

}

TPopTrack::~TPopTrack()
{
}
