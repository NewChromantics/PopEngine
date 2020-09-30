#pragma once

//	gr: this class is still seperated to keep these abstract from each other
//		gui
//		openxr
//		sokol 

#include <windowsx.h>
//	gr: this class was supposed to skip opengl requirement...
#include <SoyOpenglContext.h>
#include "SoyGui_Win32.h"

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


class Platform::TOpenglContext : public  Opengl::TRenderTarget, public Win32::TOpenglContext
{
public:
	TOpenglContext(TControl& Parent, Win32::TOpenglParams& Params);
	~TOpenglContext();

	//	render target
	virtual void				Bind() override;
	virtual void				Unbind() override;
	virtual Soy::Rectx<size_t>	GetSize() override;

	//	window stuff
	void			Repaint();
	void			OnPaint();

	std::function<void(Opengl::TRenderTarget&, std::function<void()>)>	mOnRender;

	//	win32::TOpenglContext
	virtual HDC		GetHdc() override { return mHDC; }
	virtual HGLRC	GetHglrc() override { return mHGLRC; }
	virtual HWND	GetHwnd() override { return mHwnd; }

	//	context stuff
	TControl&		mParent;	//	control we're bound to
	HWND&			mHwnd = mParent.mHwnd;
	HDC				mHDC = nullptr;		//	DC we've setup for opengl
	HGLRC			mHGLRC = nullptr;	//	opengl context
	bool			mHasArbMultiSample = false;	//	is antialiasing supported?

	//	render target
	Soy::Rectx<size_t>	mRect;
};
