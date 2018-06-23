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
	let VertShaderSource = `#version 410
		const vec4 Rect = vec4(0,0,1,1);
		in vec2 TexCoord;
		out vec2 uv;
		void main()
		{
		   gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);
		   gl_Position.xy *= Rect.zw;
		   gl_Position.xy += Rect.xy;
		//	move to view space 0..1 to -1..1
			gl_Position.xy *= vec2(2,2);
			gl_Position.xy -= vec2(1,1);
			uv = vec2(TexCoord.x,1-TexCoord.y);
		}
	`;
	
	let FragShaderSource = `#version 410
		in vec2 uv;
		//out vec4 FragColor;
		void main()
		{
			gl_FragColor = vec4(uv.x,uv.y,1,1);
		}
	`;

	//log("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Hello!");
	//let Window2 = new OpenglWindow("Hello2!");
	let Shader = null;
	
	let OnRender = function()
	{
		try
		{
			if ( Shader == null )
				Shader = new OpenglShader( Window1, VertShaderSource, FragShaderSource );
			
			Window1.ClearColour(0,1,0);
			Window1.DrawQuad( Shader );
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



