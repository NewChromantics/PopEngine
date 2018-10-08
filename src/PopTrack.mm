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

#include "Js_Duktape.h"

void Js::OnAssert()
{
	std::Debug << "some assert" << std::endl;
}

TPopAppError::Type PopMain()
{
	std::function<void(const char*)> OnError = [&](const char* String)
	{
		std::Debug << String << std::endl;
		return 0;
	};

	std::function<void(int,int,const char*)> Print = [&](int x,int y,const char* String)
	{
		std::Debug << String << std::endl;
		return 0;
	};
	Js::TContext::mPrint = &Print;
	(*Js::TContext::mPrint)(0,0,"One");
	Js::TContext Context( OnError );
	Context.Execute("print('hello!');");
	
	return TPopAppError::Success;
	
	auto& App = PopTrack::GetApp();
	
#if !defined(TARGET_OSX_BUNDLE)
	//	run
	App.mConsoleApp.WaitForExit();
#endif

	return TPopAppError::Success;
}



#include "v8Minimal.h"

TPopTrack::TPopTrack(const std::string& RootDirectory,const std::string& BootupFilename)
{
	//mV8Instance.reset( new TV8Instance(RootDirectory,BootupFilename) );

	v8min_main(RootDirectory);
}

TPopTrack::~TPopTrack()
{
}
