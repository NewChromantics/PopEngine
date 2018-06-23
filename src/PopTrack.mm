#include "PopTrack.h"
#include <SoyDebug.h>
#include <SoyApp.h>
#include <PopMain.h>
#include <SoyPixels.h>
#include <SoyString.h>
#include <SortArray.h>
#include <SoyOpenglWindow.h>

#define FILTER_MAX_FRAMES	10
#define FILTER_MAX_THREADS	1
#define JOB_THREAD_COUNT	1

#include "TV8Container.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"





auto JavascriptMain = R"DONTPANIC(

function ReturnSomeString()
{
	return "Hello world";
}

function test_function()
{
	let FragShaderSource = `
	varying vec2 oTexCoord;
	void main()
	{
		gl_FragColor = vec4(oTexCoord,0,1);
	}
	`;
	
	//log("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Hello!");
	//let Window2 = new OpenglWindow("Hello2!");
	
	let OnRender = function()
	{
		try
		{
			Window1.ClearColour(0,1,0);
			Window1.DrawQuad();
		}
		catch(Exception)
		{
			Window1.ClearColour(1,0,0);
			log(Exception);
		}
	}
	Window1.OnRender = OnRender;
}

//	main
test_function();

)DONTPANIC";




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
		Private::gOpenglApp.reset( new TPopTrack(JavascriptMain) );
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











TPopTrack::TPopTrack(const std::string& BootupJavascript)
{
	mV8Container.reset( new TV8Container() );
	
	ApiCommon::Bind( *mV8Container );
	ApiOpengl::Bind( *mV8Container );
	
	//	gr: change bootup to include('main.js');
	mV8Container->LoadScript( BootupJavascript );
	
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



