#include "PopEngine.h"
#include "SoyDebug.h"
#include "SoyApp.h"
#include "PopMain.h"
#include "SoyFileSystem.h"
#include "JsCoreInstance.h"

namespace PopTrack
{
	namespace Private
	{
		//	keep alive after PopMain()
		std::shared_ptr<TPopTrack> gOpenglApp;
		
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


namespace Platform
{
	void		Loop(bool Blocking,std::function<void()> OnQuit);
}


TPopAppError::Type PopMain(const ArrayBridge<std::string>& Arguments)
{
	std::string DataPath = Arguments.GetSize() > 0 ? Arguments[0] : "";
	auto& App = PopTrack::GetApp(DataPath);
	
#if !defined(TARGET_OSX_BUNDLE)
	//	run
	//Soy::Platform::TConsoleApp app;
	//app.WaitForExit();
#endif

#if defined(TARGET_WINDOWS)
	bool Running = true;
	while ( Running )
	{
		auto OnQuit = [&]()
		{
			Running = false;
		};
		Platform::Loop(true,OnQuit);
	}
#endif

	
	return TPopAppError::Success;
}



#include "SoyOpenglWindow.h"
std::shared_ptr<TOpenglWindow> pWindow;

TPopTrack::TPopTrack(const std::string& RootDirectory,const std::string& BootupFilename)
{
	//	todo: watch for when a file changes and recreate instance
	//mV8Instance.reset( new TV8Instance(RootDirectory,BootupFilename) );
#if !defined(TARGET_WINDOWS)
	mJsCoreInstance.reset( new JsCore::TInstance(RootDirectory,BootupFilename) );
#endif
	pWindow.reset(new TOpenglWindow("Test", Soy::Rectf(), TOpenglParams() ));
}

TPopTrack::~TPopTrack()
{
}
