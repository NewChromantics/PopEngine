#include "PCWinWindow.h"
//#include "PCOpenglExt.h"

/*
namespace Win32
{
	u32	g_MultisamplePixelFormat = 0;
}
*/

//---------------------------------------------------------
//	create window class and create win32 control
//---------------------------------------------------------
Bool Win32::GWindow::Init(TPtr<GWinControl>& pOwner, u32 Flags)
{
	//	window needs to create class first
	if ( ! CreateClass() )
	{
		return NULL;
	}

	return GWinControl::Init( pOwner, Flags );
}

