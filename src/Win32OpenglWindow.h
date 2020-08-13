#pragma once

#include <windowsx.h>

//	to allow access to some win32 specific opengl things, 
//	make an interface class
namespace Win32
{
	class TOpenglContext;
}

class Win32::TOpenglContext
{
public:
	virtual HDC		GetHdc() = 0;
	virtual HGLRC	GetHglrc() = 0;
	virtual HWND	GetHwnd() = 0;
};
