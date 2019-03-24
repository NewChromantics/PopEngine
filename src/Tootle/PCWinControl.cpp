#include "PCWinControl.h"
#include <TootleCore/TLDebug.h>
#include <TootleCore/TString.h>
#include <TootleCore/TLCore.h>
#include <winuser.h>	//	updated core SDK
#include "PCWinWindow.h"
#include "PCApp.h"	//	access to g_HInstance

#if defined(_MSC_EXTENSIONS)
#include <TootleCore/PC/PCDebug.h>
#endif

//	globals
//------------------------------------------------
namespace Win32
{
	TPtr<Win32::TWinControlFactory>	g_pFactory;

	LRESULT CALLBACK			Win32CallBack(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );
	u32							GWinControl::g_MouseWheelMsg = 0;
	HWND						g_HWnd = NULL;

	THeapArray<const TChar*>		g_ClassCreatedList;	//	array of strings of classes we've created
};


//	Definitions
//------------------------------------------------



//------------------------------------------------
//	init win32
//------------------------------------------------
SyncBool Win32::Init()
{
	//	create factory if it doesnt exist
	if ( !g_pFactory )
	{
		g_pFactory = new Win32::TWinControlFactory;
		if ( !g_pFactory )
			return SyncFalse;
	}

	return SyncTrue;
}


//------------------------------------------------
//	shutdown win32
//------------------------------------------------
SyncBool Win32::Shutdown()
{
	//	no more factory all cleaned up
	if ( !g_pFactory )
		return SyncTrue;

	SyncBool Result = g_pFactory->ShutdownObjects();

	//	still shutting down
	if ( Result == SyncWait )
		return Result;

	//	free factory
	g_pFactory = NULL;

	//	empty global array
	g_ClassCreatedList.Empty(TRUE);

	return Result;
}




//------------------------------------------------
//	win control factory/manager
//------------------------------------------------
Win32::GWinControl* Win32::TWinControlFactory::CreateObject(TRefRef InstanceRef,TRefRef TypeRef)
{
	if ( TypeRef == "Window" )
		return new Win32::GWindow( InstanceRef );

	return NULL;
	//return new Win32::GWinControl( InstanceRef );
}




Win32::GWinControl::GWinControl(TRefRef InstanceRef) :
	m_Ref				( InstanceRef ),
	m_Hwnd				( NULL ),
	m_OwnerHwnd			( NULL ),
	m_ClientPos			( int2(0,0) ),
	m_ClientSize		( int2(40,40) ),
	m_StyleFlags		( 0x0 ),
	m_Closed			( FALSE ),
	m_HasExplicitText	( FALSE )
{
}



Win32::GWinControl::~GWinControl()
{
	//	destroy control
	Destroy();

	//	null entry in children
	for ( u32 c=0;	c<m_ChildControls.GetSize();	c++ )
	{
		if ( m_ChildControls[c]->m_pOwnerControl == this )
		{
			m_ChildControls[c]->m_pOwnerControl = NULL;
		}
	}

	//	remove from parents list
	if ( m_pOwnerControl )
	{
		s32 Index = m_pOwnerControl->m_ChildControls.FindIndex(this);
		if ( Index != -1 )
			m_pOwnerControl->m_ChildControls.RemoveAt( (u32)Index );
	}
}


void Win32::GWinControl::PosToScreen(int2& ClientPos)
{
	//	convert client pos to screen
	POINT p;
	p.x = ClientPos.x;
	p.y = ClientPos.y;
	ClientToScreen( m_Hwnd, &p );
	ClientPos.x = p.x;
	ClientPos.y = p.y;
}


void Win32::GWinControl::ScreenToPos(int2& ScreenPos)
{
	//	convert screen pos to client pos
	POINT p;
	p.x = ScreenPos.x;
	p.y = ScreenPos.y;
	ScreenToClient( m_Hwnd, &p );
	ScreenPos.x = p.x;
	ScreenPos.y = p.y;
}



Bool Win32::GWinControl::Init(TPtr<GWinControl>& pOwner, u32 Flags)
{
	//	check we havent already created a contorl
	if ( m_Hwnd != NULL )
	{
		TLDebug_Break("Control already created");
		return FALSE;
	}


	STARTUPINFO startupinfo;
	startupinfo.cb=0;
	GetStartupInfo(&startupinfo);

	// Filled the structure?
	if(startupinfo.cb)
	{
		// This structure is used when the first call to ShowWindow() is done.
		// Check to see if the startupinfo will set the size?  
		// if so prevent this as we will explicitly set the size of the window ourselves.

		if(startupinfo.dwFlags & STARTF_USESIZE)
		{
			//switch of fthe flag
			startupinfo.dwFlags &= ~STARTF_USESIZE;
		}
	}
	
	//	create control
	Flags |= AdditionalStyleFlags();
	m_StyleFlags	= Flags;

//	HMENU hMenu		= (HMENU)ControlID;
	HMENU hMenu		= (HMENU)NULL;
	HINSTANCE hInstance = static_cast<HINSTANCE>( TLGui::Platform::g_HInstance );
	HWND OwnerHwnd	= pOwner ? pOwner->m_Hwnd : m_OwnerHwnd;

	//	reset handle
	m_Hwnd = NULL;

	//	set owner
	m_pOwnerControl = pOwner;

	TString RefString;
	m_Ref.GetString( RefString );
	m_HasExplicitText = FALSE;

	//	get resulting hwnd, m_Hwnd is set from the WM_CREATE callback
	HWND ResultHwnd = CreateWindowEx( StyleExFlags(), ClassName(), RefString.GetData(), StyleFlags(), m_ClientPos.x, m_ClientPos.y, m_ClientSize.x, m_ClientSize.y, OwnerHwnd, hMenu, hInstance, (void*)&m_Ref );

	// [19-09-08] DB - HACK - set the global HWND
	if( Win32::g_HWnd == NULL)
		Win32::g_HWnd = ResultHwnd;

	//	if control doesnt get a WM_CREATE (if its a standard windows control) call it
	if ( ResultHwnd != NULL && m_Hwnd == NULL )
	{
		//	grab our ptr
		TPtr<Win32::GWinControl> pThis = g_pFactory->GetInstance( m_Ref );
		OnWindowCreate( pThis, ResultHwnd );
	}

	//	failed
	if ( m_Hwnd == NULL || ResultHwnd == NULL || m_Hwnd != ResultHwnd )
	{
		TLDebug::Platform::CheckWin32Error();
		g_pFactory->RemoveInstance( m_Ref );
		return FALSE;
	}

	// Force an update/refresh of the window
	//if ( !SetWindowPos( m_Hwnd, WindowOrder, m_ClientPos.x, m_ClientPos.x, m_ClientSize.x, m_ClientSize.y, SWP_FRAMECHANGED ) )
	//	TLDebug::Platform::CheckWin32Error();

	// This will reset the window restore information so when the window is created it will *always* be 
	// at the size we create it at
	WINDOWPLACEMENT wndpl;
	if(GetWindowPlacement(ResultHwnd, &wndpl))
	{
		wndpl.rcNormalPosition.right = 0;
		wndpl.rcNormalPosition.bottom = 0;
		SetWindowPlacement(ResultHwnd, &wndpl);
	}
    

	//	control has been created
	OnCreate();

	return TRUE;
}

//-------------------------------------------------------------------------
//	callback after a window has been created
//-------------------------------------------------------------------------
/*static*/void Win32::GWinControl::OnWindowCreate(TPtr<GWinControl>& pControl,HWND Hwnd)
{
	if ( !pControl )
	{
		TLDebug_Break("WM_CREATE callback with invalid control pointer");
		return;
	}

	//	set handle
	pControl->m_Hwnd = Hwnd;

	//	update styles added by windows
	pControl->GetStyleFlags();

	//	set member values now we successfully created the window
	if ( pControl->m_pOwnerControl )
	{
		pControl->m_pOwnerControl->m_ChildControls.Add( pControl );
	}
}


Bool Win32::GWinControl::CreateClass()
{
	//	if class already created we dont need to create it
	if ( g_ClassCreatedList.Find( ClassName() ) )
		return TRUE;

	WNDCLASS wc;
	ZeroMemory(&wc,sizeof(wc));
	wc.style		= ClassStyle();
	wc.lpfnWndProc	= Win32::Win32CallBack; 
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hInstance	= static_cast<HINSTANCE>(TLGui::Platform::g_HInstance);
	wc.hIcon		= GetIconHandle();
	wc.hCursor		= NULL;//LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetBackgroundBrush();
	wc.lpszMenuName	= NULL;
	wc.lpszClassName = ClassName();

	if (!RegisterClass(&wc))
	{
		TLDebug::Platform::CheckWin32Error();
		return FALSE;
	}

	//	add to list of created-classes
	g_ClassCreatedList.Add( ClassName() );

	return TRUE;
}

/*static*/Bool Win32::GWinControl::DestroyClass(const TChar* pClassName)
{
	if ( !UnregisterClass( pClassName, static_cast<HINSTANCE>( TLGui::Platform::g_HInstance ) ) )
	{
		TLDebug::Platform::CheckWin32Error();
		return FALSE;
	}

	//	remove class from created-class list
	s32 ClassIndex = g_ClassCreatedList.FindIndex( pClassName );
	g_ClassCreatedList.RemoveAt( ClassIndex );

	return TRUE;
}

Bool Win32::GWinControl::DestroyClass()
{
	return DestroyClass( ClassName() );
}

Bool Win32::GWinControl::ClassExists()
{
	WNDCLASS wc;
	return GetClassInfo( static_cast<HINSTANCE>( TLGui::Platform::g_HInstance ) , ClassName(), &wc ) != 0;
}


void Win32::GWinControl::Destroy()
{
	OnDestroy();

	//	destroy the control if we still have a handle
	if ( m_Hwnd )
	{
		//	HACK - unassign global hwnd
		if( Win32::g_HWnd == m_Hwnd)
			Win32::g_HWnd = NULL;

		//	gr: recurses? clear hwnd then destroy
		HWND Hwnd = m_Hwnd;
		//m_Hwnd = NULL;

		if ( ! DestroyWindow( Hwnd ) )
		{
			TLDebug::Platform::CheckWin32Error();
		}

		m_Hwnd = NULL;
	}

	//	remove from global control list
	if ( Win32::g_pFactory )
		Win32::g_pFactory->RemoveInstance( m_Ref );
}



void Win32::GWinControl::Show(Bool Show)
{
	if ( m_Hwnd )
	{
		ShowWindow( m_Hwnd, Show ? SW_SHOW : SW_HIDE );
		GetStyleFlags();
	}
}

Bool Win32::GWinControl::SetText(const TString& Text,Bool IsExplicitText)
{
	//	if this is automated text (eg. from ref) and we've already manually
	//	set the text, then don't overwrite it
	if ( !IsExplicitText && m_HasExplicitText )
		return FALSE;

	if ( !m_Hwnd )
		return FALSE;

	SetWindowText( m_Hwnd, Text.GetData() );
	m_HasExplicitText = IsExplicitText;
	return TRUE;
}

//-----------------------------------------------------
//	change ref. update text if neccessary
//-----------------------------------------------------
void Win32::GWinControl::SetRef(const TRef& Ref)				
{	
	m_Ref = Ref;

	//	update text
	if ( !m_HasExplicitText )
	{
		TString RefString;
		Ref.GetString( RefString );

		SetText( RefString.GetData(), FALSE );
	}
}


//-------------------------------------------------------------------------
//	redraw window
//-------------------------------------------------------------------------
void Win32::GWinControl::Refresh()
{
	if ( m_Hwnd)
	{
		//	invalidate whole window for redrawing
		if ( InvalidateRgn( m_Hwnd, NULL, TRUE ) )
		{
			UpdateWindow( m_Hwnd );
		}
		else
		{
			TLDebug::Platform::CheckWin32Error();
		}
	}

	//	refresh is manually called, so update scrollbars
	UpdateScrollBars();
}


//-------------------------------------------------------------------------
//	set new pos and dimensions at once
//-------------------------------------------------------------------------
void Win32::GWinControl::SetDimensions(int2 Pos, int2 Size)
{
	//	update our pos
	m_ClientPos = Pos;

	//	update our size
	m_ClientSize = Size;

	//	apply the changes
	UpdateDimensions();
}


//-------------------------------------------------------------------------
//	set new pos and dimensions at once
//-------------------------------------------------------------------------
void Win32::GWinControl::SetDimensions(const Type4<s32>& PosSizes)
{
	//	update our pos
	m_ClientPos = int2( PosSizes.x, PosSizes.y );

	//	update our size
	m_ClientSize = int2( PosSizes.Width(), PosSizes.Height() );

	//	apply the changes
	UpdateDimensions();
}


//-------------------------------------------------------------------------
//	set new position
//-------------------------------------------------------------------------
void Win32::GWinControl::Move(int2 Pos)
{
	//	update our pos
	m_ClientPos = Pos;

	//	apply the changes
	UpdateDimensions();
}


//-------------------------------------------------------------------------
//	set new width/height
//-------------------------------------------------------------------------
void Win32::GWinControl::Resize(int2 Size)
{
	//	gr: client size is not window size
	//	update our size
	m_ClientSize = Size;

	//	apply the changes
	UpdateDimensions();
}


//-------------------------------------------------------------------------
//	update window dimensions to current client size settings
//-------------------------------------------------------------------------
void Win32::GWinControl::UpdateDimensions()
{
	//	convert client rect to window rect
	RECT WindowRect;
	WindowRect.left		= m_ClientPos.x;
	WindowRect.right	= m_ClientPos.x + m_ClientSize.x;
	WindowRect.top		= m_ClientPos.y;
	WindowRect.bottom	= m_ClientPos.y + m_ClientSize.y;

	if ( !AdjustWindowRectEx( &WindowRect, StyleFlags(), HasMenu(), StyleExFlags() ) )
	{
		TLDebug::Platform::CheckWin32Error();
		return;
	}

	//	set new window size
//	Resize( int2( ClientRect.right-ClientRect.left, ClientRect.bottom-ClientRect.top ) );
	if ( m_Hwnd )
	{
		if ( !MoveWindow( m_Hwnd, WindowRect.left, WindowRect.top, WindowRect.right-WindowRect.left, WindowRect.bottom-WindowRect.top, TRUE ) )
		{
			TLDebug::Platform::CheckWin32Error();
		}
	}
}


void Win32::GWinControl::GetStyleFlags()
{
	m_StyleFlags = 0x0;

	//	use GetWindowLongPtr in newer platform SDK
	#if(WINVER >= 0x0500)
		m_StyleFlags |= GetWindowLongPtr( m_Hwnd, GWL_STYLE );
		m_StyleFlags |= GetWindowLongPtr( m_Hwnd, GWL_EXSTYLE );
	#else
		m_StyleFlags |= GetWindowLong( m_Hwnd, GWL_STYLE );
		m_StyleFlags |= GetWindowLong( m_Hwnd, GWL_EXSTYLE );
	#endif
}
	
void Win32::GWinControl::SetNewStyleFlags(u32 Flags)
{
	m_StyleFlags = Flags;

	if ( !SetWindowLong( m_Hwnd, GWL_STYLE, StyleFlags() ) )
		TLDebug::Platform::CheckWin32Error();

	if ( !SetWindowLong( m_Hwnd, GWL_EXSTYLE, StyleExFlags() ) )
		TLDebug::Platform::CheckWin32Error();

	HWND WindowOrder = HWND_NOTOPMOST;
	if ( StyleExFlags() & GWinControlFlags::AlwaysOnTop )
		WindowOrder = HWND_TOPMOST;

	//	update/refresh window
	if ( !SetWindowPos( m_Hwnd, WindowOrder, 0, 0, 0, 0, SWP_SHOWWINDOW|SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE ) )
		TLDebug::Platform::CheckWin32Error();

}

void Win32::GWinControl::SetStyleFlags(u32 Flags)
{
	//	add flags to our current set
	SetNewStyleFlags( m_StyleFlags|Flags );
}

void Win32::GWinControl::ClearStyleFlags(u32 Flags)
{
	//	remove flags from our current flags
	SetNewStyleFlags( m_StyleFlags&(~Flags) );
}


Win32::GMenuSubMenu* Win32::GWinControl::GetChildSubMenu(HMENU HMenu,TPtr<GWinControl>& pControl)	
{
	GMenuSubMenu* pSubMenu = NULL;
	pControl = NULL;

	for ( u32 i=0;	i<m_ChildControls.GetSize();	i++ )
	{
		pSubMenu = m_ChildControls[i]->GetSubMenu( HMenu );
		if ( pSubMenu )
		{
			pControl = m_ChildControls[i];
			break;
		}
	}

	return pSubMenu;
}



Win32::GMenuItem* Win32::GWinControl::GetChildMenuItem(u16 ItemID, TPtr<GWinControl>& pControl)	
{
	GMenuItem* pMenuItem = NULL;
	pControl = NULL;

	for ( u32 i=0;	i<m_ChildControls.GetSize();	i++ )
	{
		pMenuItem = m_ChildControls[i]->GetMenuItem( ItemID );
		if ( pMenuItem )
		{
			pControl = m_ChildControls[i];
			break;
		}
	}

	return pMenuItem;
}


//-------------------------------------------------------------------------
//	update window's scrollbar info if applicable
//-------------------------------------------------------------------------
void Win32::GWinControl::UpdateScrollBars()
{
	//	only needed if window is setup
	if ( !Hwnd() )
		return;

	//	
	SCROLLINFO ScrollInfo;
	ScrollInfo.cbSize = sizeof(SCROLLINFO);

	//	update vert scroll bar
	if ( StyleFlags() & GWinControlFlags::VertScroll )
	{
		int Min = 0;
		int Max = 0;
		int Jump = 0;
		int Pos = 0;
		Bool Enable = GetVertScrollProperties( Min, Max, Jump, Pos );

		ScrollInfo.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS; 
		if ( Enable )
		{
			ScrollInfo.nMin		= Min;
			ScrollInfo.nMax		= Max;
			ScrollInfo.nPage	= Jump;
			ScrollInfo.nPos		= Pos;
			ScrollInfo.fMask |= SIF_DISABLENOSCROLL;	//	disable if invalid/useless properties (eg. min==max)
		}
		else
		{
			//	remove scroll bar by invalidating properies and NOT setting SIF_DISABLENOSCROLL flag
			ScrollInfo.nMin		= 0;
			ScrollInfo.nMax		= 0;
			ScrollInfo.nPage	= 0;
			ScrollInfo.nPos		= 0;
			ScrollInfo.fMask &= ~SIF_DISABLENOSCROLL;
		}

		SetScrollInfo( Hwnd(), SB_VERT, &ScrollInfo, TRUE );
	}

	//	update horz scroll bar
	if ( StyleFlags() & GWinControlFlags::HorzScroll )
	{
		int Min = 0;
		int Max = 0;
		int Jump = 0;
		int Pos = 0;
		Bool Enable = GetHorzScrollProperties( Min, Max, Jump, Pos );

		ScrollInfo.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS; 
		if ( Enable )
		{
			ScrollInfo.nMin		= Min;
			ScrollInfo.nMax		= Max;
			ScrollInfo.nPage	= Jump;
			ScrollInfo.nPos		= Pos;
			ScrollInfo.fMask |= SIF_DISABLENOSCROLL;	//	disable if invalid/useless properties (eg. min==max)
		}
		else
		{
			//	remove scroll bar by invalidating properies and NOT setting SIF_DISABLENOSCROLL flag
			ScrollInfo.nMin		= 0;
			ScrollInfo.nMax		= 0;
			ScrollInfo.nPage	= 0;
			ScrollInfo.nPos		= 0;
			ScrollInfo.fMask &= ~SIF_DISABLENOSCROLL;
		}

		SetScrollInfo( Hwnd(), SB_HORZ, &ScrollInfo, TRUE );
	}
}

//-------------------------------------------------------------------------
//	load a resource and turn it into a brush
//-------------------------------------------------------------------------
HBRUSH Win32::GWinControl::GetBrushFromResource(int Resource)
{
	HBITMAP HBitmap = NULL;
	HBRUSH HBrush = NULL;
	
	//	didnt load from external file, try loading internal resource
	HBitmap = (HBITMAP)LoadImage( static_cast<HINSTANCE>(TLGui::Platform::g_HInstance), MAKEINTRESOURCE(Resource), IMAGE_BITMAP, 0, 0, 0x0 );
	if ( HBitmap ) 
	{
		HBrush = CreatePatternBrush( HBitmap );
		DeleteObject( HBitmap );
		return HBrush;
	}

	return HBrush;
}
	
//-------------------------------------------------------------------------
//	setup a timer
//-------------------------------------------------------------------------
void Win32::GWinControl::StartTimer(int TimerID,int Time)
{
	if ( !SetTimer( m_Hwnd, TimerID, Time, NULL ) )
	{
		TLDebug::Platform::CheckWin32Error();
	}
}

//-------------------------------------------------------------------------
//	stop a registered timer
//-------------------------------------------------------------------------
void Win32::GWinControl::StopTimer(int TimerID)
{
	if ( !KillTimer( m_Hwnd, TimerID ) )
	{
		TLDebug::Platform::CheckWin32Error();
	}
}



int Win32::GWinControl::HandleNotifyMessage(u32 message, NMHDR* pNotifyData)
{
	switch ( message )
	{
		/*
		case NM_CLICK:		if ( OnButtonDown( GMouse::Left, int2(-1,-1) ) )	return 0;	break;
		case NM_DBLCLK:		if ( OnDoubleClick( GMouse::Left, int2(-1,-1) ) )	return 0;	break;
		case NM_RCLICK:		if ( OnButtonDown( GMouse::Right, int2(-1,-1) ) )	return 0;	break;
		case NM_RDBLCLK:	if ( OnDoubleClick( GMouse::Right, int2(-1,-1) ) )	return 0;	break;
*/
		case 0:
		default:
			//GDebug::Print("Unhandled Notify Message 0x%04x\n",message);		
			break;
/*
		GWinTreeView* pTree = (GWinTreeView*)pControl;
		case TVN_BEGINLABELEDIT:
		{
			//	about to edit a label, return 0 to allow change, 1 to reject editing
			NMTVDISPINFO* pEditInfo = (NMTVDISPINFO*)pNotifyData;
			GWinTreeItem* pItem = pTree->FindItem( pEditInfo->item.hItem );
			Bool AllowEdit = pTree->AllowItemEdit( pItem, pEditInfo );
			return AllowEdit ? 0 : 1;
		}
		break;

		case TVN_ENDLABELEDIT:
		{
			//	editing label has finished. return FALSE to reject change
			NMTVDISPINFO* pEditInfo = (NMTVDISPINFO*)pNotifyData;

			//	if we have a null string, it was cancelled anyway
			if ( pEditInfo->item.pszText == NULL )
				return FALSE;

			GWinTreeItem* pItem = pTree->FindItem( pEditInfo->item.hItem );
			return pTree->FinishItemEdit( pItem, pEditInfo );
		}
		break;

		case TVN_SELCHANGED:
			pTree->Selected( (GWinTreeView*)pControl, (NMTREEVIEW*)pNotifyData );
			break;
*/
	};
	

	return 0;
}

void Win32::GWinControl::OnActivate()
{
#pragma message("move this and make this call the gui window somehow and make the TLGui the only thing that sends out engine messages")
/*
	TLMessaging::TMessage Message("OnWindowChanged");
	Message.ExportData("State", TRef("Activate"));
	PublishMessage(Message);
	*/
}

void Win32::GWinControl::OnDeactivate()
{	
#pragma message("move this and make this call the gui window somehow and make the TLGui the only thing that sends out engine messages")
/*
	TLMessaging::TMessage Message("OnWindowChanged");
	Message.ExportData("State", TRef("Deactivate"));
	PublishMessage(Message);
*/
}







LRESULT CALLBACK Win32::Win32CallBack(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	//	convert mouse wheel registered message
	if ( message == Win32::GWinControl::g_MouseWheelMsg && Win32::GWinControl::g_MouseWheelMsg != 0 )
		message = WM_MOUSEWHEEL;

	//	gr: no longer need this
/*
	//	handle user messages (WM_USER...WM_APP...inclusive)
	if ( message >= WM_USER && message < 0xC000 )
	{
		TLCore::Platform::HandleWin32Message( message, wParam, lParam );
		return 0;
	}
*/
	//	get control this message is for
	TPtr<GWinControl> pControl = Win32::g_pFactory ? Win32::g_pFactory->GetInstance( hwnd ) : NULL;
	//GDebug_Print("Callback %x. app %x control %x\n", message, GApp::g_pApp, pControl );

	//	check if control wants to handle the message
	if ( pControl )
	{
		u32 ControlHandleResult = 0;
		if ( pControl->HandleMessage( message, wParam, lParam, ControlHandleResult ) )
		{
			return ControlHandleResult;
		}
	}

	//	misc vars
	int VertScroll = -1;

	//	handle windows messages
	switch( message )
	{
		case WM_PARENTNOTIFY:
		{
			if ( pControl )
			{
				switch ( wParam )
				{
					/*
					case WM_LBUTTONDOWN:	if ( pControl->OnButtonDown(	GMouse::Left, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
					case WM_LBUTTONUP:		if ( pControl->OnButtonUp(		GMouse::Left, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
					case WM_LBUTTONDBLCLK:	if ( pControl->OnDoubleClick(	GMouse::Left, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;

					case WM_RBUTTONDOWN:	if ( pControl->OnButtonDown(	GMouse::Right, int2( LOWORD(lParam), HIWORD(lParam) ) )	)	return 0;	break;
					case WM_RBUTTONUP:		if ( pControl->OnButtonUp(		GMouse::Right, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
					case WM_RBUTTONDBLCLK:	if ( pControl->OnDoubleClick(	GMouse::Right, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;

					case WM_MBUTTONDOWN:	if ( pControl->OnButtonDown(	GMouse::Middle, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
					case WM_MBUTTONUP:		if ( pControl->OnButtonUp(		GMouse::Middle, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
					case WM_MBUTTONDBLCLK:	if ( pControl->OnDoubleClick(	GMouse::Middle, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
					*/
					case 0:
					default:
						break;
				}
			}
		}
		break;

		case WM_NOTIFY:	//	control notify message
		{
			NMHDR* pNotifyData = (NMHDR*)lParam;
			pControl = g_pFactory->GetInstance( pNotifyData->hwndFrom );
			if ( !pControl )
				return 0;
			return pControl->HandleNotifyMessage( pNotifyData->code, pNotifyData );
		}
		break;

		case WM_MOVE:	//	moved
			if ( pControl )
			{
				pControl->m_ClientPos.x = LOWORD( lParam );
				pControl->m_ClientPos.y = HIWORD( lParam );
				pControl->OnMove();
				return 0;
			}
			break;
	
			/*
		case WM_WINDOWPOSCHANGING:
			{
				WINDOWPOS* pWindowData = (WINDOWPOS*)(lParam);

				// Overwtie the window size data, otherwise it will be the previous 
				// set size and position for the window.
				if(pWindowData)
				{
					pWindowData->cx = pControl->m_ClientSize.x;
					pWindowData->cy = pControl->m_ClientSize.y;
				}
			}
			break;
			*/
		case WM_SIZE:	//	resized
			if ( pControl )
			{
				u32 Size = LOWORD( lParam );
				if(Size == 320 || Size == 480)
				{
					pControl->m_ClientSize.x = Size;
					pControl->m_ClientSize.y = HIWORD( lParam );
					pControl->OnResize();
					pControl->UpdateScrollBars();
				}
				return 0;
			}
			break;

		case WM_VSCROLL:	if ( VertScroll == -1 )	VertScroll = TRUE;	//	scrolled vertical
		case WM_HSCROLL:	if ( VertScroll == -1 )	VertScroll = FALSE;	//	scrolled horizontal
			if ( pControl && VertScroll != -1 )
			{
				SCROLLINFO ScrollInfo;
				ScrollInfo.cbSize = sizeof( SCROLLINFO );
				ScrollInfo.fMask	= SIF_ALL;
				GetScrollInfo( pControl->Hwnd(), VertScroll?SB_VERT:SB_HORZ, &ScrollInfo );

				//	Save the position for comparison later on
				int CurrentPos = ScrollInfo.nPos;
				int NewPos = CurrentPos;

				switch (LOWORD (wParam))
				{
					case SB_TOP:		NewPos = ScrollInfo.nMin;		break;	//	home keyboard key
					case SB_BOTTOM:		NewPos = ScrollInfo.nMax;		break;	//	end keyboard key
					case SB_LINEUP:		NewPos -= 1;					break;	//	arrow up
					case SB_LINEDOWN:	NewPos += 1;					break;	//	arrow down
					case SB_PAGEUP:		NewPos -= ScrollInfo.nPage;		break;	//	jumped up
					case SB_PAGEDOWN:	NewPos += ScrollInfo.nPage;		break;	//	jumped down
					case SB_THUMBTRACK:	NewPos = ScrollInfo.nTrackPos;	break;	//	dragged to pos
				}

				//	Set the position and then retrieve it.  Due to adjustments
				//	by Windows it may not be the same as the value set
				ScrollInfo.nPos = NewPos;
				ScrollInfo.fMask = SIF_POS;
				SetScrollInfo( pControl->Hwnd(), VertScroll?SB_VERT:SB_HORZ, &ScrollInfo, TRUE );
				GetScrollInfo( pControl->Hwnd(), VertScroll?SB_VERT:SB_HORZ, &ScrollInfo );
				//	If the position has changed, scroll window and update it
				if ( ScrollInfo.nPos != CurrentPos )
				{
					if ( VertScroll )
						pControl->OnScrollVert( ScrollInfo.nPos );
					else
						pControl->OnScrollHorz( ScrollInfo.nPos );
				}

				//	redraw control
				pControl->Refresh();

				return 0;
			}
			break;

		case WM_INITMENUPOPUP:	//	submenu popped up
			if ( pControl )
			{
				GMenuSubMenu* pSubMenu = pControl->GetSubMenu( (HMENU)wParam );
				TPtr<GWinControl> pMenuControl = pControl;

				//	sub menu not in this control, check the children's menus
				if ( !pSubMenu )
				{
					pSubMenu = pControl->GetChildSubMenu( (HMENU)wParam, pMenuControl );

					if ( !pSubMenu )
					{
						TLDebug_Print("Could not find sub menu");
						return 1;
					}
				}

				pMenuControl->OnMenuPopup( pSubMenu );
				return 0;	//	was processed by the app
			}
			break;


		case WM_NCDESTROY:
		case WM_DESTROY:
			//	control destroyed
			break;

		case WM_QUIT:
			// Flag the core to quit
			TLCore::Quit();

			return 0;
			break;

		case WM_CLOSE:
			if ( pControl )
			{
				pControl->OnClose();
				TLCore::Quit();
				return 0;
			}
			break;


		case WM_CREATE:
		{
			CREATESTRUCT* pCreateStruct = (CREATESTRUCT*)lParam;
			TRef* pRef = (TRef*)pCreateStruct->lpCreateParams;
			TPtr<Win32::GWinControl> pCreateControl = g_pFactory->GetInstance( *pRef );
			GWinControl::OnWindowCreate( pCreateControl, hwnd );
			return 0;
		}
		break;


		//	window command
		case WM_COMMAND:
		{
			Bool MenuCommand = (HIWORD(wParam) == 0);

			if ( MenuCommand )
			{
				if ( !pControl )
					return 0;
				
				//	get the menu item and the control it belongs to (if any)
				GMenuItem* pMenuItem = pControl->GetMenuItem( LOWORD(wParam) );

				//	child menu not in this control, check the children's menus
				if ( !pMenuItem )
				{
					TPtr<GWinControl> pChildControl;
					pMenuItem = pControl->GetChildMenuItem( LOWORD(wParam), pChildControl );

					if ( !pMenuItem )
					{
						TLDebug_Print("Could not find menu item");
						return 1;
					}

					pControl = pChildControl;
				}

				pControl->OnMenuClick( pMenuItem );

				//	was processed by the app
				return 0;
			}
			else
			{
				//	not implemented any other controls yet
			}
		}
		break;
/*
		case WM_MOUSEWHEEL:
		{
			//	get the wheel scroll amount
			s16 Scroll = HIWORD( wParam );

			if ( Scroll < 0 )
			{
				g_Mouse.m_WheelScrolledUp = TRUE;
				if ( pControl )	
					if ( pControl->OnButtonDown( GMouse::WheelDown, int2( LOWORD(-1), HIWORD(-1) ) ) )
						return 0;
			}
			else
			{
				g_Mouse.m_WheelScrolledDown = TRUE;
				if ( pControl )	
					if ( pControl->OnButtonDown( GMouse::WheelUp, int2( LOWORD(-1), HIWORD(-1) ) ) )
						return 0;
			}
		}
		break;


		case WM_LBUTTONDOWN:	if ( pControl )	if ( pControl->OnButtonDown(	GMouse::Left, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
		case WM_LBUTTONUP:		if ( pControl )	if ( pControl->OnButtonUp(		GMouse::Left, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
		case WM_LBUTTONDBLCLK:	if ( pControl )	if ( pControl->OnDoubleClick(	GMouse::Left, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;

		case WM_RBUTTONDOWN:	if ( pControl )	if ( pControl->OnButtonDown(	GMouse::Right, int2( LOWORD(lParam), HIWORD(lParam) ) )	)	return 0;	break;
		case WM_RBUTTONUP:		if ( pControl )	if ( pControl->OnButtonUp(		GMouse::Right, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
		case WM_RBUTTONDBLCLK:	if ( pControl )	if ( pControl->OnDoubleClick(	GMouse::Right, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;

		case WM_MBUTTONDOWN:	if ( pControl )	if ( pControl->OnButtonDown(	GMouse::Middle, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
		case WM_MBUTTONUP:		if ( pControl )	if ( pControl->OnButtonUp(		GMouse::Middle, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;
		case WM_MBUTTONDBLCLK:	if ( pControl )	if ( pControl->OnDoubleClick(	GMouse::Middle, int2( LOWORD(lParam), HIWORD(lParam) ) ) )	return 0;	break;

 */
		case WM_PAINT:
			if ( pControl )
			{
				if ( pControl->OnPaint() )
				{
					return 0;
				}
			}
			break;

		case WM_ERASEBKGND:
			if ( pControl )
			{
				if ( !pControl->OnEraseBackground() )
				{
					return 0;
				}
			}
			break;

		case WM_GETMINMAXINFO:
		case WM_NCCREATE:
			//	*need* to handle these with defwndproc
			break;
	
		case WM_SHOWWINDOW:
			if ( pControl )
			{
				if ( wParam == (WPARAM)TRUE )
				{
					if ( pControl->OnShow() )
						return 0;
				}
				else
				{
					if ( pControl->OnHide() )
						return 0;
				}
			}
			break;

		case WM_MOUSEMOVE:
		case WM_NCMOUSEMOVE:
			if ( pControl )
			{
				if ( pControl->OnMouseMove( int2( LOWORD(lParam), HIWORD(lParam) ), (message == WM_MOUSEMOVE) ) )
				{
					return 0;
				}
			}
			break;
	
		case WM_TIMER:
			if ( pControl )
			{
				pControl->OnTimer( wParam );
				return 0;
			}
			break;

		case WM_ACTIVATE:
			if(pControl)
			{
				if (WA_INACTIVE == wParam)
					pControl->OnDeactivate();
				else
					pControl->OnActivate();
			}
			break;

		case WM_SETCURSOR:
		case WM_NCACTIVATE:
		case WM_GETTEXT:
		case WM_ACTIVATEAPP:
		case WM_KILLFOCUS:
		case WM_MOVING:
		case WM_WINDOWPOSCHANGING:
		case WM_WINDOWPOSCHANGED:
		case WM_CAPTURECHANGED:
		case WM_EXITSIZEMOVE:
		case WM_SYNCPAINT:
		case WM_NCPAINT:
		case WM_SETFOCUS:
		case WM_GETICON:
		case WM_MOUSEACTIVATE:
		case WM_SYSCOMMAND:
		case WM_ENTERSIZEMOVE:
		case WM_DROPFILES:
		case WM_NCHITTEST:
/*		case WM_NCLBUTTONDOWN:
		case WM_NCLBUTTONUP:
		case WM_NCLBUTTONDBLCLK:
		case WM_NCRBUTTONDOWN:
		case WM_NCRBUTTONUP:
		case WM_NCRBUTTONDBLCLK:
		case WM_NCMBUTTONDOWN:
		case WM_NCMBUTTONUP:
		case WM_NCMBUTTONDBLCLK:
*/
		case WM_ENTERMENULOOP:
		case WM_EXITMENULOOP:
		case WM_MENUSELECT:
		case WM_INITMENU:
		case WM_ENTERIDLE:
		case WM_CONTEXTMENU:
			//	do DefWindProc
			break;

		default:
			//GDebug_Print("Unhandled WM Message 0x%04x\n",message);		
			break;
	};


	return DefWindowProc(hwnd, message, wParam, lParam) ;
}






