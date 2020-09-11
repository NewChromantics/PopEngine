#pragma once


#include "SoyTypes.h"

#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>	//	drag & drop


namespace Platform
{
#if defined(TARGET_WINDOWS)
	namespace Private
	{
		extern HINSTANCE InstanceHandle;
	}
#endif
}
