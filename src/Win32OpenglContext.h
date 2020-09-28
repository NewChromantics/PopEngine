#pragma once

#include <windowsx.h>
//	gr: this class was supposed to skip opengl requirement...
#include <SoyOpenglContext.h>

//	to allow access to some win32 specific opengl things, 
//	make an interface class
namespace Win32
{
	class TOpenglContext;
	class TOpenglParams;
}


class Win32::TOpenglParams
{
public:
	bool		mFullscreen = false;
	bool		mHardwareAccellerationOnly = true;
	bool		mDoubleBuffer = true;
	bool		mRedrawWithDisplayLink = true;
	int			mVsyncSwapInterval = 1;	//	0 = no vsync

	//	move these out of "hardware params" (they're things we've added at mid-level and could just be high level)
	int			mRefreshRate = 60;		//	will try to skip redraws if vsync on
	bool		mAutoRedraw = true;
};

class Win32::TOpenglContext : public Opengl::TContext
{
public:
	virtual ~TOpenglContext(){}

	virtual HDC		GetHdc() = 0;
	virtual HGLRC	GetHglrc() = 0;
	virtual HWND	GetHwnd() = 0;
	
	//	context
	virtual void	Lock() override;
	virtual void	Unlock() override;
};

