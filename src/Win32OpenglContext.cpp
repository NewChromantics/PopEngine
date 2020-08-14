#include "Win32OpenglContext.h"


void Win32::TOpenglContext::Lock()
{
	//	osx does base context lock first
	TContext::Lock();

	try
	{
		auto mHDC = GetHdc();
		auto mHGLRC = GetHglrc();

		//	switch to this thread
		if (!wglMakeCurrent(mHDC, mHGLRC))
			throw Soy::AssertException("wglMakeCurrent failed");
	}
	catch (...)
	{
		Unlock();
		throw;
	}
}

void Win32::TOpenglContext::Unlock()
{
	auto mHDC = GetHdc();
	if (!wglMakeCurrent(mHDC, nullptr))
		throw Soy::AssertException("wglMakeCurrent unbind failed");

	TContext::Unlock();
}
