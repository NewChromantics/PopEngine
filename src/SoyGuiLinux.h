#pragma once

#define ENABLE_EGL
#define ENABLE_X11
//#define ENABLE_DRMWINDOW

#if defined(ENABLE_EGL)
#include "EglContext.h"
#include  <EGL/egl.h>
//	GUI types
class EglWindow;
class EglRenderView;
#endif

#if defined(ENABLE_X11)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

//	need to get these global macros under control, move x11 inclues out of header
#undef None

#endif




class EglWindow : public SoyWindow
{
public:
	virtual Soy::Rectx<int32_t>		GetScreenRect() override;

	virtual EGLNativeDisplayType	GetDisplay()=0;
	virtual EGLSurface				GetSurface()=0;
};


//	gr: we should split this into X11 windows and DRM displays (providing surface & display)
//		then EGL renderview which runs on either
//	x11 window has mouse & key stuff... not so for DRM?
class WindowX11 : public EglWindow
{
public:
	WindowX11( const std::string& Name, Soy::Rectx<int32_t>& Rect );
	~WindowX11();
	
	virtual void				SetFullscreen(bool Fullscreen) override	{	}
	virtual bool				IsFullscreen() override					{	return true;	}
	virtual bool				IsMinimised() override					{	return false;	}
	virtual bool				IsForeground() override					{	return true;	}
	virtual void				EnableScrollBars(bool Horz,bool Vert) override	{}

	virtual EGLNativeDisplayType	GetDisplay()	{	return mDisplay;	}
	virtual EGLSurface				GetSurface()	{	return mSurface;	}

	void						RequestPaint();	//	due to single event thread and apparently x11 is unstable multithreaded, we send ourselves an event to paint and then callback

private:
	bool						EventThreadIteration();

public:
	Window		mWindow;
	Display*	mDisplay = nullptr;
	EGLSurface	mSurface = nullptr;	//	surface comes from display&window
	std::shared_ptr<SoyThread>	mEventThread;
};

/*
class Platform::TWindow : public SoyWindow
{
public:
	TWindow( const std::string& Name, Soy::Rectx<int32_t>& Rect );
	
	virtual Soy::Rectx<int32_t>		GetScreenRect() override;

	virtual void									SetFullscreen(bool Fullscreen) override;
	virtual bool									IsFullscreen() override;
	virtual bool									IsMinimised() override;
	virtual bool									IsForeground() override;
	virtual void									EnableScrollBars(bool Horz,bool Vert) override;
};
*/

#if defined(ENABLE_DRMWINDOW)
class WindowDrm : public EglWindow
{
public:
	WindowDrm(const std::string& Name, Soy::Rectx<int32_t>& Rect );
	~WindowDrm();

	virtual Soy::Rectx<int32_t>		GetScreenRect() override;

	virtual void					SetFullscreen(bool Fullscreen) override	{};
	virtual bool					IsFullscreen() override	{	return true;	};
	virtual bool					IsMinimised() override	{	return false;	};
	virtual bool					IsForeground() override	{	return true;	};
	virtual void					EnableScrollBars(bool Horz,bool Vert) override	{};

public:
	std::shared_ptr<Egl::TDisplaySurfaceContext>		mContext;
};
#endif

class EglRenderView : public Gui::TRenderView
{
public:
	EglRenderView(SoyWindow& Parent);

public:
	//	bit unsafe!
	EglWindow&		mWindow;
};
