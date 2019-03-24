/*
 *  PCGui.cpp
 *  TootleGui
 *
 *  Created by Graham Reeves on 17/02/2010.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include "PCGui.h"
#include "PCWinControl.h"
#include "PCWindow.h"	//	platform::window


#pragma comment( lib, "user32.lib" )
#pragma comment( lib, "gdi32.lib" )
#pragma comment( lib, "kernel32.lib" )
#pragma comment( lib, "winmm.lib" )	//	required for [multimedia] time functions

//	gr: changed these ignore-default-libs to ignore the defualt or release version 
//	based on the DEBUG build of this lib (tootleGUI). Gets rid of the linker warning
//	"defaultlib "library" conflicts with use of other libs" in debug/release builds configs
#if defined(_DEBUG)
	#pragma comment(linker, "/NODEFAULTLIB:msvcrt.lib") 
	#pragma comment(linker, "/NODEFAULTLIB:libcmt.lib")
#else // release/final
	#pragma comment(linker, "/NODEFAULTLIB:msvcrtd.lib") 
	#pragma comment(linker, "/NODEFAULTLIB:libcmtd.lib")
#endif

//#pragma comment( lib, "libc.lib" )
#pragma comment( lib, "comdlg32.lib" )
#pragma comment( lib, "winspool.lib" )
#pragma comment( lib, "shell32.lib" )
#pragma comment( lib, "comctl32.lib" )
#pragma comment( lib, "ole32.lib" )
#pragma comment( lib, "oleaut32.lib" )
#pragma comment( lib, "uuid.lib" )
#pragma comment( lib, "rpcrt4.lib" )
#pragma comment( lib, "advapi32.lib" )

//	gr: I've removed these libs as we're using winsock 2 (ws2) in TLNetwork.
//		I didn't get any linker errors, so presumably they're not needed?
//#pragma comment( lib, "wsock32.lib" )
//#pragma comment( lib, "wininet.lib" )


//----------------------------------------------------
//	init platform implementation
//----------------------------------------------------
SyncBool TLGui::Platform::Init()
{
	return Win32::Init();
}


//----------------------------------------------------
//	shutdown platform implementation
//----------------------------------------------------
SyncBool TLGui::Platform::Shutdown()
{
	return Win32::Shutdown();
}


//---------------------------------------------------------------
//	get mouse position relative to this window
//---------------------------------------------------------------
int2 TLGui::Platform::GetScreenMousePosition(TLGui::TWindow& Window,u8 MouseIndex)
{
	//	get cursor pos
	POINT MousePos;
	GetCursorPos( &MousePos );

	//	convert from screen to client space
	int2 ScreenPos( MousePos.x, MousePos.y );
	TLGui::Platform::Window& PlatformWindow = static_cast<TLGui::Platform::Window&>( Window );
	PlatformWindow.m_pWindow->ScreenToPos( ScreenPos );

	return ScreenPos;
}



//----------------------------------------------------------
//	get the desktop dimensions. note: need a window so we can decide which desktop?
//----------------------------------------------------------
void TLGui::Platform::GetDesktopSize(Type4<s32>& DesktopSize)
{
	DesktopSize.x = 0;
	DesktopSize.y = 0;
	DesktopSize.Width() = GetSystemMetrics(SM_CXSCREEN);
	DesktopSize.Height() = GetSystemMetrics(SM_CYSCREEN);
}

