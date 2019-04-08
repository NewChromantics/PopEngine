#include "PopEngine.h"
#include "SoyDebug.h"
#include "SoyApp.h"
#include "PopMain.h"
#include "SoyFileSystem.h"
#include "TBind.h"

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

	//	use arg0 if provided
	//	otherwise, we assume there's a bootup.js in the app's resources path
	if ( Arguments.GetSize() > 0 )
		DataPath = Arguments[0];

	//	 try to predict full paths vs something embedded in the app
	if ( !Platform::IsFullPath(DataPath) )
	{
		DataPath = Platform::GetAppResourcesDirectory() + DataPath;
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

TPopTrack::TPopTrack(const std::string& RootDirectory,const std::string& BootupFilename)
{
	mApiInstance.reset( new Bind::TInstance(RootDirectory,BootupFilename) );
}

TPopTrack::~TPopTrack()
{
}
