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
	std::string DataPath;
	
	if ( Arguments.GetSize() > 0 )
	{
		DataPath = Arguments[0];
	}
	else
	{
		DataPath = Platform::GetAppResourcesDirectory() + "GuildhallGildwall";
	}
	
	
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


//	windows test window code
#if defined(TARGET_WINDOWS)
#include "SoyOpenglWindow.h"
std::shared_ptr<TOpenglWindow> pWindow;
#endif

TPopTrack::TPopTrack(const std::string& RootDirectory,const std::string& BootupFilename)
{
	//	todo: watch for when a file changes and recreate instance
	//mV8Instance.reset( new TV8Instance(RootDirectory,BootupFilename) );
#if !defined(TARGET_WINDOWS)
	mJsCoreInstance.reset( new JsCore::TInstance(RootDirectory,BootupFilename) );
#endif
	
	//	windows test window code
#if defined(TARGET_WINDOWS)
	pWindow.reset(new TOpenglWindow("Test", Soy::Rectf(), TOpenglParams() ));
#endif
}

TPopTrack::~TPopTrack()
{
}
