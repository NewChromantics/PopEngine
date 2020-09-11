#pragma once

#include <windowsx.h>
//	gr: this class was supposed to skip opengl requirement...
#include <SoyOpenglContext.h>

//	to allow access to some win32 specific opengl things, 
//	make an interface class
namespace Win32
{
	class TOpenglContext;
}

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

