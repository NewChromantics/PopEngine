/*
 *  MacGui.h
 *  TootleGui
 *
 *  Created by Graham Reeves on 17/02/2010.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */
#include "../TLGui.h"


#ifndef _MSC_EXTENSIONS
	#error PC file should not be included in ansi builds
#endif

//	include windows stuff
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501	// specify the minimum windows OS we are supporting (0x0501 == Windows XP)

#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef UNICODE
#define UNICODE
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <typeinfo>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>


namespace TLGui
{
	namespace Platform
	{
		SyncBool		Init();
		SyncBool		Shutdown();

		int2			GetScreenMousePosition(TLGui::TWindow& Window,u8 MouseIndex);
		void			GetDesktopSize(Type4<s32>& DesktopSize);	//	get the desktop dimensions. note: need a window so we can decide which desktop?	
	}

}


