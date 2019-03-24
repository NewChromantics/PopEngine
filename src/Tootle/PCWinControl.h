/*------------------------------------------------

	Base class for creating/controlling win32 windows

-------------------------------------------------*/
#pragma once
/*
#include "PCGui.h"	//	windows headers
#include <TootleCore/TLTypes.h>
#include <TootleCore/TPtrArray.h>
#include <TootleCore/TRef.h>
#include <TootleCore/TClassFactory.h>
#include <TootleCore/TPublisher.h>
#include <COMMCTRL.H>



namespace Win32
{
	class GWinControl;
	class GMenuSubMenu;
	class GMenuItem;
	class TWinControlFactory;
	
	extern TPtr<TWinControlFactory>	g_pFactory;

	namespace GWinControlFlags
	{
		//	style flags
		const u32	Popup						= WS_POPUP;
		const u32	OverlappedWindow			= WS_OVERLAPPEDWINDOW;// &~WS_SIZEBOX);
		const u32	Visible						= WS_VISIBLE;
		const u32	Child						= WS_CHILD;
		const u32	ClipSiblings				= WS_CLIPSIBLINGS;
		const u32	VertScroll					= WS_VSCROLL;
		const u32	HorzScroll					= WS_HSCROLL;
		const u32	TreeView_DrawLines			= TVS_HASLINES;
		const u32	TreeView_DrawButtons		= TVS_HASBUTTONS;
		const u32	TreeView_DrawLinesAtRoot	= TVS_LINESATROOT;
		const u32	TreeView_EnableLabelEdit	= TVS_EDITLABELS;
		const u32	TreeView_DisableDragDrop	= TVS_DISABLEDRAGDROP;
		//	style flag groups
		const u32	TreeView_StyleFlags			= TreeView_DrawLines | TreeView_DrawButtons | TreeView_DrawLinesAtRoot | TreeView_EnableLabelEdit | TreeView_DisableDragDrop;
		const u32	StyleFlags					= ClipSiblings | Popup | OverlappedWindow | Visible | Child | TreeView_StyleFlags | VertScroll | HorzScroll;


		//	ex style flags
		const u32	ClientEdge					= WS_EX_CLIENTEDGE;
		const u32	AlwaysOnTop					= WS_EX_TOPMOST;
		const u32	AllowDragDrop				= WS_EX_ACCEPTFILES;
		const u32	ToolWindowBorder			= WS_EX_TOOLWINDOW;
		//	Ex flags groups
		const u32	TreeView_ExStyleFlags		= 0x0;
		const u32	ExStyleFlags				= ClientEdge | AlwaysOnTop | ToolWindowBorder | AllowDragDrop | TreeView_ExStyleFlags;


	};

	SyncBool		Init();					//	init win32
	SyncBool		Shutdown();				//	shutdown win32
}


//	Types
//------------------------------------------------

//------------------------------------------------
//	gr: Dummy for now...
//------------------------------------------------
class Win32::GMenuSubMenu
{
};

//------------------------------------------------
//	gr: Dummy for now...
//------------------------------------------------
class Win32::GMenuItem
{
};
*/

//------------------------------------------------
//	base win32 control/window class
//------------------------------------------------
class Win32::GWinControl// : public TLMessaging::TPublisher
{
public:
	static u32			g_MouseWheelMsg;	//	mouse wheel's message ID

public:
	int2				m_ClientPos;
	int2				m_ClientSize;

protected:
	HWND				m_Hwnd;
	TRef				m_Ref;
	Bool				m_Closed;
	Bool				m_HasExplicitText;

	TPtr<GWinControl>	m_pOwnerControl;
	HWND				m_OwnerHwnd;		//	may be set, even if m_pOwnerControl is null

private:
	u32						m_StyleFlags;
	TPtrArray<GWinControl>	m_ChildControls;
	
public:
	GWinControl(TRefRef InstanceRef);
	virtual ~GWinControl();

	virtual void			Update()								{	}
	virtual SyncBool		Shutdown()								{	return SyncTrue;	}

	//	control-specific stuff
	virtual const TChar*	ClassName()								{	return TLCharString("Class");	};	//	what class this window creates
	virtual u32				AdditionalStyleFlags()					{	return 0x0;	};		//	extra style flags for this particular type of control
	virtual u16				ClassStyle()							{	return CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;	};
	virtual Bool			ClassExists();

	//	win32 notify messages
	virtual void			OnCreate()									{	};					//	called after being created
	virtual void			OnDestroy()									{	};					//	called just before being destroyed
	virtual void			OnResize()									{	};
	virtual void			OnMove()									{	};
	virtual void			OnClose()									{	m_Closed = TRUE;	}
	virtual void			OnMenuPopup( GMenuSubMenu* pSubMenu )		{	}					//	menu was popped up in this control
	virtual void			OnMenuClick( GMenuItem* pMenuItem )			{	}					//	menu item was selected in this control
	virtual GMenuSubMenu*	GetSubMenu( HMENU HMenu )					{	return NULL;	};	//	find a menu in this control
	virtual GMenuItem*		GetMenuItem( u16 ItemID )					{	return NULL;	};	//	find a menu item in this control
	virtual GMenuSubMenu*	GetChildSubMenu( HMENU HMenu, TPtr<GWinControl>& pControl );		//	find a menu in this control
	virtual GMenuItem*		GetChildMenuItem( u16 ItemID, TPtr<GWinControl>& pControl );		//	find a menu item in this control
	virtual Bool			OnButtonDown(int MouseButton, int2 Pos)		{	return FALSE;	};
	virtual Bool			OnButtonUp(int MouseButton, int2 Pos)		{	return FALSE;	};
	virtual Bool			OnDoubleClick(int MouseButton, int2 Pos)	{	return FALSE;	};
	virtual Bool			OnPaint()									{	return FALSE;	};
	virtual Bool			OnEraseBackground()							{	return TRUE;	};	//	return whether or not to erase the background when requested
	virtual Bool			OnScrollVert(int NewPos)					{	return FALSE;	};	//	vert scroll bar was scrolled/clicked/dragged etc to new pos
	virtual Bool			OnScrollHorz(int NewPos)					{	return FALSE;	};	//	horz scroll bar was scrolled/clicked/dragged etc to new pos
	virtual Bool			OnShow()									{	return FALSE;	};	//	window is now visible
	virtual Bool			OnHide()									{	return FALSE;	};	//	window is now hidden
	virtual Bool			OnMouseMove(int2 MousePos, Bool InClientArea)	{	return FALSE;	};	//	mouse has been moved in window
	virtual void			OnTimer(WPARAM TimerID)						{	};					//	timer caught
	virtual void			OnActivate();
	virtual void			OnDeactivate();

	//	generic win32 stuff
	virtual Bool				CreateClass();													//	create class
	virtual Bool				DestroyClass();
	static Bool					DestroyClass(const TChar* pClassName);
	virtual Bool				Init(TPtr<GWinControl>& pOwner, u32 Flags);		//	create control
	void						Destroy();																		//	destroy control
	virtual Bool				HandleMessage(u32 message, WPARAM wParam, LPARAM lParam, u32& Result)	{	return false;	}	//	message has been sent to this control matching the hwnd
	virtual int					HandleNotifyMessage(u32 message, NMHDR* pNotifyData);		//	message has been sent to this control matching the hwnd
	virtual Bool				HasMenu()								{	return FALSE;	};	//	used for windows to return if they have a main menu
	virtual HICON				GetIconHandle()							{	return LoadIcon( NULL, IDI_APPLICATION );	};
	virtual HBRUSH				GetBackgroundBrush()					{	return GetSysColorBrush(COLOR_WINDOW);	};
	virtual u32					DefaultFlags()							{	return 0x0;	};
	virtual Bool				GetVertScrollProperties(int& Min, int& Max, int& Jump, int& Pos)	{	return FALSE;	};
	virtual Bool				GetHorzScrollProperties(int& Min, int& Max, int& Jump, int& Pos)	{	return FALSE;	};

	//	general win32 interfacing calls which dont need to be overloaded
	inline HWND&			Hwnd()									{	return m_Hwnd;	};
	inline const HWND&		HwndConst() const						{	return m_Hwnd;	};
	void					PosToScreen(int2& ClientPos);								//	convert a pos on this object to a position on the screen
	void					ScreenToPos(int2& ScreenPos);								//	convert a pos in the screen to a relative pos on the object
	void					Show(Bool Show=TRUE);
	void					Hide()									{	Show(FALSE);	};
	Bool					SetText(const TString& Text,Bool IsExplicitText=TRUE);
	void					Resize(int2 Size);						//	set new width/height
	void					Move(int2 Pos);							//	set new position
	void					SetDimensions(int2 Pos, int2 Size);		//	set new pos and dimensions at once
	void					SetDimensions(const Type4<s32>& PosSize);		//	set new pos and dimensions at once
	void					Refresh();
	void					SetStyleFlags(u32 Flags);
	void					ClearStyleFlags(u32 Flags);
	inline u32				HasStyleFlags(u32 Flags)				{	return (m_StyleFlags & Flags);	};
	inline u32				StyleFlags()							{	return (m_StyleFlags&GWinControlFlags::StyleFlags);	};
	inline u32				StyleExFlags()							{	return (m_StyleFlags&GWinControlFlags::ExStyleFlags);	};
	void					UpdateScrollBars();						//	post-resize, updates scroll bar info
	Bool					Visible()								{	return (StyleFlags()&GWinControlFlags::Visible)? TRUE : FALSE;	};
	HBRUSH					GetBrushFromResource(int Resource);		
	void					StartTimer(int TimerID,int Time);		//	setup a timer
	void					StopTimer(int TimerID);					//	stop a registered timer
	static void				OnWindowCreate(TPtr<GWinControl>& pControl,HWND Hwnd);	//	callback after a window has been created
	Bool					IsClosed() const						{	return m_Closed;	}
	const TRef&				GetRef() const							{	return m_Ref;	}
	void					SetRef(const TRef& Ref);

	inline Bool				operator==(const HWND& hwnd) const		{	return HwndConst() == hwnd;	}
	inline Bool				operator==(const TRef& Ref) const		{	return m_Ref == Ref;	}
	inline Bool				operator==(const GWinControl* pControl) const		{	return (this == pControl);	}

protected:
	void					SetNewStyleFlags(u32 Flags);			//	sets new style flags for the window
	void					GetStyleFlags();						//	updates style flags from window
	void					UpdateDimensions();						//	update window dimensions to current client size settings
};

