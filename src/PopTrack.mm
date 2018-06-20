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
		Private::gOpenglApp.reset( new TPopTrack("PopEngine") );
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



TPopTrack::TPopTrack(const std::string& WindowName)
{
	Soy::Rectf Rect( 0, 0, 300, 300 );
	TOpenglParams Params;
	/*
	mWindow.reset( new TOpenglWindow( WindowName, Rect, Params ) );
	if ( !mWindow->IsValid() )
	{
		mWindow.reset();
		return;
	}
	
	mWindow->mOnRender.AddListener(*this,&TPopTrack::OnOpenglRender);
	*/
	mV8Container.reset( new TV8Container() );
}

TPopTrack::~TPopTrack()
{
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}
	
}



void TPopTrack::OnOpenglRender(Opengl::TRenderTarget& RenderTarget)
{
	auto FrameBufferSize = RenderTarget.GetSize();
	
	
	Soy::Rectf Viewport(0,0,1,1);
	RenderTarget.SetViewportNormalised( Viewport );
	
	Opengl::ClearColour( Soy::TRgb(51/255.f,204/255.f,255/255.f) );
	Opengl::ClearDepth();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	
	//	make rendering tile rect
	Soy::Rectf TileRect( 0, 0, 1,1);
	
	auto OpenglContext = this->GetContext();
	
	//DrawQuad( nullptr, TileRect );
	
	Opengl_IsOkay();
}

std::shared_ptr<Opengl::TContext> TPopTrack::GetContext()
{
	if ( !mWindow )
		return nullptr;
	
	return mWindow->GetContext();
}



