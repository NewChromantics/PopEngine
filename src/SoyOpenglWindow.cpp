#include "SoyOpengl.h"
#include "SoyOpenglWindow.h"
//#include "SoyOpenglView.h"
//#include "PopMain.h"
#include "SoyMath.h"

#include <windowsx.h>
#include "SoyWindow.h"
#include "SoyThread.h"

#include <commctrl.h>
#pragma comment(lib, "Comctl32.lib")

namespace Platform
{
	class TControlClass;
	class TControl;
	class TWindow;
	class TOpenglView;

	class TOpenglContext;

	class TWin32Thread;	//	A window(control?) is associated with a thread so must pump messages in the same thread https://docs.microsoft.com/en-us/windows/desktop/procthread/creating-windows-in-threads


	namespace Private
	{
		HINSTANCE InstanceHandle = nullptr;
	}

	UINT	g_MouseWheelMsg = 0;
	LRESULT CALLBACK	Win32CallBack(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	void		Loop(bool Blocking,std::function<void()> OnQuit);
	void		Loop(std::function<bool()> CanBlock,std::function<void()> OnQuit);	

	template<typename COORDTYPE>
	Soy::Rectx<COORDTYPE>	GetRect(RECT Rect);
}


SoyKeyButton::Type KeyCodeToSoyButton(WPARAM KeyCode)
{
	if ( KeyCode >= 0x41 && KeyCode <= 0x5A )
	{
		auto Index = KeyCode - 0x41;
		return 'a' + Index;
	}

	if ( KeyCode >= 0x30 && KeyCode <= 0x39 )
	{
		auto Index = KeyCode - 0x30;
		return '0' + Index;
	}

	//	https://docs.microsoft.com/en-us/windows/desktop/inputdev/virtual-key-codes
	switch ( KeyCode )
	{
	case VK_SPACE:	return ' ';
	}

	std::stringstream Error;
	Error << "Unhandled VK_KEYCODE 0x" << std::hex << KeyCode << std::dec;
	throw Soy::AssertException(Error);
}

template<typename COORDTYPE>
Soy::Rectx<COORDTYPE> Platform::GetRect(RECT Rect)
{
	Soy::Rectx<COORDTYPE> SoyRect;
	SoyRect.x = Rect.left;
	SoyRect.y = Rect.top;
	SoyRect.w = Rect.right - Rect.left;
	SoyRect.h = Rect.bottom - Rect.top;
	return SoyRect;
}

void Platform::EnumScreens(std::function<void(TScreenMeta&)> EnumScreen)
{
	//	should lock this
	static std::function<void(TScreenMeta&)>* pEnumScreen = nullptr;
	pEnumScreen = &EnumScreen;
	MONITORENUMPROC EnumWrapper = [](HMONITOR MonitorHandle,HDC hdc,LPRECT Rect,LPARAM Data)->BOOL
	{
		MONITORINFOEXA MonitorMetaA;
		MonitorMetaA.cbSize = sizeof(MonitorMetaA);
		if ( !GetMonitorInfoA(MonitorHandle, &MonitorMetaA) )
			Platform::ThrowLastError("GetMonitorInfoA(MONITORINFOEXA) failed");

		MONITORINFO MonitorMetaB;
		MonitorMetaB.cbSize = sizeof(MonitorMetaB);
		if ( !GetMonitorInfoA(MonitorHandle, &MonitorMetaB) )
			Platform::ThrowLastError("GetMonitorInfoA(MONITORINFO) failed");
		
		TScreenMeta Screen;
		Screen.mFullRect = GetRect<int32_t>(MonitorMetaA.rcMonitor);
		Screen.mWorkRect = GetRect<int32_t>(MonitorMetaA.rcWork);

		//	gr: this name is supposed to be unique
		Screen.mName = std::string(MonitorMetaA.szDevice, sizeof(MonitorMetaA.szDevice));
		if ( Screen.mName.length() == 0 )
			throw Soy::AssertException("Platform::EnumScreens has monitor with no name");

		auto& EnumScreen = *pEnumScreen;
		EnumScreen(Screen);
		return TRUE;
	};

	HDC hdc = nullptr;
	LPCRECT ClipRect = nullptr;
	LPARAM Data = 0;
	if ( !EnumDisplayMonitors(hdc, ClipRect, EnumWrapper, Data) )
		Platform::ThrowLastError("EnumDisplayMonitors");
}



class Platform::TWin32Thread : public SoyWorkerJobThread
{
public:
	TWin32Thread(const std::string& Name, bool Win32Blocking=true) :
		mWin32Blocking(Win32Blocking),
		SoyWorkerJobThread(Name, Win32Blocking ? SoyWorkerWaitMode::NoWait : SoyWorkerWaitMode::Wake)
	{
		//	gotta do this somewhere, this is typically before any extra controls get created
		InitCommonControls();
		Start();
	}

	virtual void	Wake() override;
	virtual bool	Iteration(std::function<void(std::chrono::milliseconds)> Sleep);

	bool	mWin32Blocking = true;
	DWORD	mThreadId = 0;
};



class Platform::TControl
{
public:
	TControl(const std::string& Name, TControlClass& Class, TControl* Parent, DWORD StyleFlags, DWORD StyleExFlags, Soy::Rectx<int> Rect, TWin32Thread& Thread);
	TControl(const std::string& Name,const char* ClassName,TControl& Parent, DWORD StyleFlags, DWORD StyleExFlags, Soy::Rectx<int> Rect);
	virtual ~TControl();

	//	trigger actions
	void					Repaint();

	//	reflection
	Soy::Rectx<int32_t>		GetClientRect();
	void					SetClientRect(const Soy::Rectx<int32_t>& Rect)
	{
		std::Debug << "todo SetClientRect(" << Rect << ")" << std::endl;
	}

	//	callbacks from windows message loop
	virtual void			OnDestroyed();		//	window handle is being destroyed
	virtual void			OnWindowMessage(UINT EventMessage) {}	//	called from window thread which means we can flush jobs
	virtual void			OnScrolled(bool Horizontal,uint16_t ScrollCommand,uint16_t CurrentScrollPosition);

	//	return true if handled, or false to return default behavouir
	bool			OnMouseEvent(int x, int y, WPARAM Flags,UINT EventMessage);
	bool			OnKeyDown(WPARAM KeyCode, LPARAM Flags);
	bool			OnKeyUp(WPARAM KeyCode, LPARAM Flags);

	virtual TWin32Thread&	GetJobQueue()
	{
		return mThread;
	}

	TControl&		GetChild(HWND Handle);
	virtual void	AddChild(TControl& Child);

public:
	std::function<void(TControl&)>	mOnPaint;
	std::function<void(TControl&,const TMousePos&,SoyMouseButton::Type)>	mOnMouseDown;
	std::function<void(TControl&,const TMousePos&,SoyMouseButton::Type)>	mOnMouseMove;
	std::function<void(TControl&,const TMousePos&,SoyMouseButton::Type)>	mOnMouseUp;
	std::function<void(TControl&,SoyKeyButton::Type)>	mOnKeyDown;
	std::function<void(TControl&,SoyKeyButton::Type)>	mOnKeyUp;

	std::function<void(TControl&)>	mOnDestroy;	//	todo: expand this to OnClose to allow user to stop it from closing
	
	HWND			mHwnd = nullptr;
	std::string		mName;
	TWin32Thread&	mThread;
	uint64_t		mChildIdentifier = 0;
	static uint64_t	gChildIdentifier;

	Array<TControl*>	mChildren;
};

uint64_t Platform::TControl::gChildIdentifier = 9000;


class Platform::TWindow : public TControl, public SoyWindow
{
public:
	TWindow(const std::string& Name, Soy::Rectx<int> Rect, TWin32Thread& Thread,bool Resizable);

	virtual void	OnWindowMessage(UINT EventMessage) override;
	virtual void	OnScrolled(bool Horizontal,uint16_t ScrollCommand, uint16_t CurrentScrollPosition) override;

	virtual Soy::Rectx<int32_t>		GetScreenRect() override	{	return GetClientRect();	}
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	virtual void					EnableScrollBars(bool Horz, bool Vert) override;

	void			UpdateScrollbars();
	virtual void	AddChild(TControl& Child) override;

private:
	//	for saving/restoring fullscreen mode
	std::shared_ptr<WINDOWPLACEMENT>	mSavedWindowPlacement;	//	saved state before going fullscreen
	LONG				mSavedWindowFlags = 0;
	LONG				mSavedWindowExFlags = 0;

public:
	std::shared_ptr<TWin32Thread>		mOwnThread;
};



class Platform::TOpenglContext : public Opengl::TContext, public  Opengl::TRenderTarget
{
public:
	TOpenglContext(TControl& Parent,TOpenglParams& Params);
	~TOpenglContext();

	//	context
	virtual void	Lock() override;
	virtual void	Unlock() override;

	//	render target
	virtual void				Bind() override;
	virtual void				Unbind() override;
	virtual Soy::Rectx<size_t>	GetSize() override {	return mRect;	}
	
	//	window stuff
	void			Repaint();
	void			OnPaint();

	std::function<void(Opengl::TRenderTarget&, std::function<void()>)>	mOnRender;

	//	context stuff
	TControl&		mParent;	//	control we're bound to
	HWND&			mHwnd = mParent.mHwnd;
	HDC				mHDC = nullptr;		//	DC we've setup for opengl
	HGLRC			mHGLRC = nullptr;	//	opengl context
	bool			mHasArbMultiSample = false;	//	is antialiasing supported?

	//	render target
	Soy::Rectx<size_t>	mRect;
};


class Platform::TSlider : public TControl, public SoySlider
{
public:
	TSlider(TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetMinMax(uint16_t Min, uint16_t Max) override;
	virtual void		SetValue(uint16_t Value) override;
	virtual uint16_t	GetValue() override;

	virtual void		OnWindowMessage(UINT EventMessage) override;
};

class Platform::TLabel : public TControl, public SoyLabel
{
public:
	TLabel(TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;
};


class Platform::TTextBox : public TControl, public SoyTextBox
{
public:
	TTextBox(TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;
};


class Platform::TTickBox : public TControl, public SoyTickBox
{
public:
	TTickBox(TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetValue(bool Value) override;
	virtual bool		GetValue() override;
	virtual void		SetLabel(const std::string& Label) override;

	virtual void		OnWindowMessage(UINT EventMessage) override;
};

class Platform::TControlClass
{
public:
	TControlClass(const std::string& Name,UINT Style);
	~TControlClass();

	const char*	ClassName() const {		return mClassName.c_str();	}

	WNDCLASS	mClass;
	std::string	mClassName;
	DWORD		mStyleEx = 0;
	UINT&		mStyle = mClass.style;
};




void Platform::Loop(bool Blocking,std::function<void()> OnQuit)
{
	auto CanBlock = [=]()
	{
		return Blocking;
	};
	Loop(CanBlock, OnQuit);
}

void Platform::Loop(std::function<bool()> CanBlock,std::function<void()> OnQuit)
{
	//	gr: always do one iteration
	do
	{
		MSG msg;
		if ( CanBlock() )
		{
			if ( !GetMessage(&msg, NULL, 0, 0) )
				break;
		}
		else
		{
			if ( !PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
				break;
		}

		if ( msg.message == WM_QUIT )
			if ( OnQuit )
				OnQuit();

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		//	no more messages, and we've got updates to do so break out and let the app loop
		if ( !PeekMessage(&msg,NULL,0,0,PM_NOREMOVE) )
		{
			//	no more messages, break out here
		}
	} 
	while ( CanBlock() );
}


LRESULT CALLBACK Platform::Win32CallBack(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	//	this may be null if it hasn't been setup yet as we will in WM_CREATE
	auto* pControl = reinterpret_cast<TControl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	auto Default = [&]()
	{
		return DefWindowProc(hwnd, message, wParam, lParam);
	};

	try
	{
		//	handle isn't bound to a control of ours
		if ( !pControl )
		{
			//	setup our user data
			if ( message == WM_NCCREATE )
			{
				auto* CreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
				auto* This = CreateStruct->lpCreateParams;
				auto ThisLong = reinterpret_cast<LONG_PTR>(This);
				SetWindowLongPtr(hwnd, GWLP_USERDATA, ThisLong);
				return Default();
			}

			if ( message == WM_QUIT )
			{
				std::Debug << "Got WM_QUIT on control callback" << std::endl;
			}

			return Default();
		}

		//	callbacks
		TControl& Control = *pControl;
		Control.OnWindowMessage(message);

		switch ( message )
		{
			//	gotta allow some things
		case WM_MOVE:
		case WM_SIZE:
		case WM_CREATE:
		case WM_SHOWWINDOW:
			return 0;

		case WM_ERASEBKGND:
			return Default();

		case WM_DESTROY:
			Control.OnDestroyed();
			return 0;

		case WM_PAINT:
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);
			if ( Control.mOnPaint )
			{
				try
				{
					Control.mOnPaint(Control);
				} catch ( std::exception& e )
				{
					std::Debug << "Exception OnPaint: " << e.what() << std::endl;
				}
			}
			else
			{
				//	do default/fallback paint?
			}
			EndPaint(hwnd, &ps);
			return 0;

			//	*need* to handle these with defwndproc
		case WM_GETMINMAXINFO:
			break;

		case WM_QUIT:
			std::Debug << "Got WM_QUIT on control callback, with control" << std::endl;
			break;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
		{
			auto x = GET_X_LPARAM(lParam);
			auto y = GET_Y_LPARAM(lParam);
			if ( Control.OnMouseEvent(x, y, wParam, message) )
				return 0;
			return Default();
		}

		case WM_KEYDOWN:
		{
			auto KeyCode = wParam;
			auto Flags = lParam;
			if ( Control.OnKeyDown(KeyCode, Flags) )
				return 0;
			return Default();
		}

		case WM_KEYUP:
		{
			auto KeyCode = wParam;
			auto Flags = lParam;
			if ( Control.OnKeyUp(KeyCode, Flags) )
				return 0;
			return Default();
		}

		case WM_COMMAND:
			//	lparam is hwnd to child control
			if (lParam != 0)
			{
				auto SubMessage = HIWORD(wParam);
				auto ControlIdentifier = LOWORD(wParam);
				auto ChildHandle = reinterpret_cast<HWND>(lParam);
				auto& Child = Control.GetChild(ChildHandle);
				Child.OnWindowMessage(SubMessage);
			}
			return Default();

		case WM_HSCROLL:
		case WM_VSCROLL:
			//	lparam is hwnd to child control
			if (lParam != 0)
			{
				auto ChildHandle = reinterpret_cast<HWND>(lParam);
				auto& Child = Control.GetChild(ChildHandle);
				Child.OnWindowMessage(message);
			}
			else
			{
				//	window is being scrolled
				auto CurrentPosition = HIWORD(wParam);
				auto Command = LOWORD(wParam);
				auto Horz = (message == WM_HSCROLL);
				Control.OnScrolled(Horz, Command, CurrentPosition);
			}
			return Default();
		}

		return Default();
	}
	catch(std::exception& e)
	{
		std::Debug << "Exception in win32 callback: " << e.what() << std::endl;
		return Default();
	}
}




Platform::TControlClass::TControlClass(const std::string& Name, UINT ClassStyle) :
	mClassName	( Name )
{
	auto* NameStr = mClassName.c_str();
	HICON IconHandle = LoadIcon(NULL, IDI_APPLICATION);
	HBRUSH BackgroundBrush = GetSysColorBrush(COLOR_3DFACE);
	
	auto& wc = mClass;
	ZeroMemory(&wc,sizeof(wc));
	wc.style		= ClassStyle;
	wc.lpfnWndProc	= Win32CallBack; 
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hInstance = Platform::Private::InstanceHandle;
	wc.hIcon		= IconHandle;
	wc.hCursor		= NULL;//LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = BackgroundBrush;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName = NameStr;

	if (!RegisterClass(&wc))
	{
		Platform::IsOkay("Register class");
		throw Soy::AssertException("Failed to register class");
	}
}

Platform::TControlClass::~TControlClass()
{
	auto* Name = mClassName.c_str();

	if ( !UnregisterClass( Name, Platform::Private::InstanceHandle ) )
	{
		try
		{
			Platform::IsOkay("Register class");
			throw Soy::AssertException("Error unregistering class");
		}
		catch(std::exception& e)
		{
			std::Debug << "Error unregistering class " << e.what() << std::endl;
		}
	}
}


Platform::TControl::TControl(const std::string& Name,TControlClass& Class,TControl* Parent,DWORD StyleFlags,DWORD StyleExFlags,Soy::Rectx<int> Rect,TWin32Thread& Thread) :
	mName	( Name ),
	mThread	( Thread )
{
	const char* ClassName = Class.ClassName();
	const char* WindowName = mName.c_str();
	void* UserData = this;
	HWND ParentHwnd = Parent ? Parent->mHwnd : nullptr;
	auto Instance = Platform::Private::InstanceHandle;
	HMENU Menu = nullptr;

	if ( !mThread.IsLockedToThisThread() )
		throw Soy::AssertException("Should be creating control on our thread");

	mHwnd = CreateWindowEx(StyleExFlags, ClassName, WindowName, StyleFlags, Rect.x, Rect.y, Rect.GetWidth(), Rect.GetHeight(), ParentHwnd, Menu, Instance, UserData);
	Platform::IsOkay("CreateWindow");
	if ( !mHwnd )
		throw Soy::AssertException("Failed to create window");
}


Platform::TControl::TControl(const std::string& Name,const char* ClassName,TControl& Parent, DWORD StyleFlags, DWORD StyleExFlags, Soy::Rectx<int> Rect) :
	mName				( Name ),
	mThread				( Parent.mThread ),
	mChildIdentifier	( gChildIdentifier++ )
{
	const char* WindowName = mName.c_str();
	void* UserData = this;
	HWND ParentHwnd = Parent.mHwnd;
	auto Instance = Platform::Private::InstanceHandle;
	
	//	id for children controls
	HMENU Menu = reinterpret_cast<HMENU>(mChildIdentifier);

	if (!mThread.IsLockedToThisThread())
		throw Soy::AssertException("Should be creating control on our thread");

	mHwnd = CreateWindowEx(StyleExFlags, ClassName, WindowName, StyleFlags, Rect.x, Rect.y, Rect.GetWidth(), Rect.GetHeight(), ParentHwnd, Menu, Instance, UserData);
	Platform::IsOkay("CreateWindow");
	if (!mHwnd)
		throw Soy::AssertException("Failed to create window");

	//	set the default gui font
	auto Font = reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT));
	SendMessage(mHwnd, WM_SETFONT, Font, 0);

	Parent.AddChild(*this);
}

Platform::TControl::~TControl()
{
}


bool Platform::TControl::TControl::OnMouseEvent(int x, int y, WPARAM Flags,UINT EventMessage)
{
	auto LeftDown = (Flags & MK_LBUTTON) != 0;
	auto MiddleDown = (Flags & MK_LBUTTON) != 0;
	auto RightDown = (Flags & MK_LBUTTON) != 0;
	auto* pMouseEvent = &mOnMouseMove;
	switch(EventMessage)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:	
	case WM_MBUTTONDOWN:
		pMouseEvent = &mOnMouseDown;	
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
		pMouseEvent = &mOnMouseUp;
		break;

	case WM_MOUSEMOVE:	
		pMouseEvent = &mOnMouseMove;	
		break;
	}

	auto Button = SoyMouseButton::None;
	if ( LeftDown )			Button = SoyMouseButton::Left;
	else if ( RightDown )	Button = SoyMouseButton::Right;
	else if ( MiddleDown )	Button = SoyMouseButton::Middle;

	//	x/y relateive to client area
	//std::Debug << "mouse event: " << x << "," << y << std::endl;
	TMousePos MousePos(x, y);
	if ( pMouseEvent )
	{
		auto& Event = *pMouseEvent;
		if ( Event )
			Event( *this, MousePos, Button);
	}

	//	carry on with default behaviour
	return false;
}

bool Platform::TControl::TControl::OnKeyDown(WPARAM KeyCode, LPARAM Flags)
{
	try
	{
		auto KeyButton = KeyCodeToSoyButton(KeyCode);
		if ( mOnKeyDown )
			mOnKeyDown(*this, KeyButton);
	}
	catch ( std::exception& e )
	{
		std::Debug << "OnKeyDown error: " << e.what() << std::endl;
	}
	return false;
}

bool Platform::TControl::TControl::OnKeyUp(WPARAM KeyCode, LPARAM Flags)
{
	try
	{
		auto KeyButton = KeyCodeToSoyButton(KeyCode);
		if ( mOnKeyUp )
			mOnKeyUp(*this, KeyButton);
	}
	catch ( std::exception& e )
	{
		std::Debug << "OnKeyUp error: " << e.what() << std::endl;
	}
	return false;
}



void Platform::TControl::TControl::OnScrolled(bool Horizontal,uint16_t ScrollCommand, uint16_t CurrentScrollPosition)
{
	std::Debug << "Control OnScrolled(" << ScrollCommand << "," << CurrentScrollPosition << ") ignored" << std::endl;
}

void Platform::TControl::TControl::OnDestroyed()
{
	//	do callback, then cleanup references
	if ( mOnDestroy )
	{
		try
		{
			mOnDestroy(*this);
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception OnDestroy of control " << this->mName << ": " << e.what() << std::endl;
		}
	}

	//	clear references
	mHwnd = nullptr;
}


Soy::Rectx<int32_t> Platform::TControl::TControl::GetClientRect()
{
	RECT RectWin;
	if ( !::GetClientRect(mHwnd, &RectWin) )
		Platform::IsOkay("GetClientRect");

	if ( RectWin.left < 0 || RectWin.top < 0 )
	{
		auto RectSigned = GetRect<int32_t>(RectWin);
		std::stringstream Error;
		Error << "Not expected GetClientRect rect to be < 0; " << RectSigned;
		throw Soy::AssertException(Error.str());
	}

	auto Rect = GetRect<size_t>(RectWin);
	return Rect;
}

Platform::TControl& Platform::TControl::GetChild(HWND Handle)
{
	for (auto c = 0; c < mChildren.GetSize(); c++)
	{
		auto& Child = *mChildren[c];
		if (Child.mHwnd != Handle)
			continue;
		return Child;
	}

	std::stringstream Error;
	Error << "No child with handle " << Handle;
	throw Soy::AssertException(Error);
}

void Platform::TControl::AddChild(TControl& Child)
{
	mChildren.PushBack(&Child);
}




Platform::TControlClass& GetWindowClass()
{
	static std::shared_ptr<Platform::TControlClass> gWindowClass;
	if ( gWindowClass )
		return *gWindowClass;

	UINT ClassStyle = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

	gWindowClass.reset(new Platform::TControlClass("Window", ClassStyle));
	return *gWindowClass;
}

Platform::TControlClass& GetOpenglViewClass()
{
	static std::shared_ptr<Platform::TControlClass> gClass;
	if ( gClass )
		return *gClass;

	UINT ClassStyle = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

	gClass.reset(new Platform::TControlClass("OpenGL", ClassStyle));
	return *gClass;
}

//	gr: without an edge/border, we get a flicker argh
//	gr: only occcurs with double buffering
//const DWORD WindowStyleExFlags = WS_EX_CLIENTEDGE;
const DWORD WindowStyleExFlags = 0;
const DWORD WindowStyleFlags_Resizable = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
const DWORD WindowStyleFlags = WindowStyleFlags_Resizable & ~WS_THICKFRAME;


Platform::TWindow::TWindow(const std::string& Name,Soy::Rectx<int> Rect,TWin32Thread& Thread, bool Resizable) :
	TControl	( Name, GetWindowClass(), nullptr, Resizable ? WindowStyleFlags_Resizable : WindowStyleFlags, WindowStyleExFlags, Rect, Thread )
{
	auto ShowState = SW_SHOW;

	/*
	// Force an update/refresh of the window
	//if ( !SetWindowPos( m_Hwnd, WindowOrder, m_ClientPos.x, m_ClientPos.x, m_ClientSize.x, m_ClientSize.y, SWP_FRAMECHANGED ) )
	//	TLDebug::Platform::CheckWin32Error();
	*/

	//	restore saved window position/state
	WINDOWPLACEMENT WindowPlacement;
	WindowPlacement.length = sizeof(WindowPlacement);

	// This will reset the window restore information so when the window is created it will *always* be 
	// at the size we create it at
	if ( GetWindowPlacement( mHwnd, &WindowPlacement ) )
	{
		ShowState = WindowPlacement.showCmd;

		std::Debug << WindowPlacement.rcNormalPosition.left << "," <<
			WindowPlacement.rcNormalPosition.top << "," <<
			WindowPlacement.rcNormalPosition.right << "," <<
			WindowPlacement.rcNormalPosition.bottom << "," << std::endl;
		//	restore
		//WindowPlacement.rcNormalPosition.right = 0;
		//WindowPlacement.rcNormalPosition.bottom = 0;
		//SetWindowPlacement( mHwnd, &WindowPlacement );
	}

	//	show window now it's configured
	ShowWindow(mHwnd, ShowState);
}

void Platform::TWindow::OnWindowMessage(UINT Message)
{
	switch (Message)
	{
	case WM_SIZE:	
		UpdateScrollbars();
		break;
	}
}


void Platform::TWindow::AddChild(TControl& Child)
{
	TControl::AddChild(Child);
	UpdateScrollbars();
}

void Platform::TWindow::UpdateScrollbars()
{
	//	get our client rect with all controls
	Soy::Rectx<uint32_t> WindowRect(0, 0, 600, 2000);

	//	page is one screen
	auto ClientRect = GetClientRect();
	std::Debug << "UpdateScrollbars( " << ClientRect << ")" << std::endl;

	SCROLLINFO ScrollInfoVert;
	ScrollInfoVert.cbSize = sizeof(ScrollInfoVert);
	ScrollInfoVert.fMask = SIF_RANGE | SIF_PAGE;
	ScrollInfoVert.nPage = ClientRect.GetHeight();
	ScrollInfoVert.nMin = WindowRect.Top();
	ScrollInfoVert.nMax = WindowRect.Bottom();
	SetScrollInfo(mHwnd, SB_VERT, &ScrollInfoVert, true);

	SCROLLINFO ScrollInfoHorz;
	ScrollInfoHorz.cbSize = sizeof(ScrollInfoHorz);
	ScrollInfoHorz.fMask = SIF_RANGE | SIF_PAGE;
	ScrollInfoHorz.nPage = ClientRect.GetWidth();
	ScrollInfoHorz.nMin = WindowRect.Left();
	ScrollInfoHorz.nMax = WindowRect.Right();
	SetScrollInfo(mHwnd, SB_HORZ, &ScrollInfoHorz, true);
	
}

void Platform::TWindow::OnScrolled(bool Horizontal,uint16_t ScrollCommand,uint16_t CurrentScrollPosition)
{
	//	init page size
	SCROLLINFO ScrollInfo;
	ScrollInfo.cbSize = sizeof(ScrollInfo);
	/*
	ScrollInfo.fMask = SIF_PAGE;
	ScrollInfo.nPage = 50;
	SetScrollInfo(mHwnd, SB_VERT, &ScrollInfo, true);
	*/

	auto ScrollBar = Horizontal ? SB_HORZ : SB_VERT;

	//SCROLLINFO ScrollInfo;
	ScrollInfo.cbSize = sizeof(ScrollInfo);
	ScrollInfo.fMask = SIF_ALL;
	GetScrollInfo(mHwnd, ScrollBar, &ScrollInfo);

	auto OldPos = ScrollInfo.nPos;

	//	no page setup, jump 10% by default
	if (ScrollInfo.nPage == 0 )
		ScrollInfo.nPage = (ScrollInfo.nMax - ScrollInfo.nMin) / 10;

	//	some commands we'll just reset params
	switch (ScrollCommand)
	{
	case SB_TOP:		ScrollInfo.nPos = ScrollInfo.nMin;	break;
	case SB_BOTTOM:		ScrollInfo.nPos = ScrollInfo.nMax;	break;
	case SB_LINEUP:		ScrollInfo.nPos -= 1;	break;
	case SB_LINEDOWN:	ScrollInfo.nPos += 1;	break;
	case SB_PAGEUP:		ScrollInfo.nPos -= ScrollInfo.nPage;	break;
	case SB_PAGEDOWN:	ScrollInfo.nPos += ScrollInfo.nPage;	break;
	case SB_THUMBTRACK:	ScrollInfo.nPos = ScrollInfo.nTrackPos;	break;
	}
	if (ScrollInfo.nPos < ScrollInfo.nMin)
		ScrollInfo.nPos = ScrollInfo.nMin;
	if (ScrollInfo.nPos > ScrollInfo.nMax)
		ScrollInfo.nPos = ScrollInfo.nMax;


	std::Debug << "Scroll ";
	std::Debug << " nMin=" << ScrollInfo.nMin;
	std::Debug << " nMax=" << ScrollInfo.nMax;
	std::Debug << " nPage=" << ScrollInfo.nPage;
	std::Debug << " nPos=" << ScrollInfo.nPos;
	std::Debug << " nTrackPos=" << ScrollInfo.nTrackPos;
	std::Debug << std::endl;

	auto Redraw = true;
	ScrollInfo.fMask = SIF_POS;
	SetScrollInfo(mHwnd, ScrollBar, &ScrollInfo, Redraw);

	auto DeltaX = Horizontal ? ScrollInfo.nPos - OldPos : 0;
	auto DeltaY = !Horizontal ? ScrollInfo.nPos - OldPos : 0;

	bool InverseScroll = true;
	if (InverseScroll)
	{
		DeltaX = -DeltaX;
		DeltaY = -DeltaY;
	}
	
	//	scroll the window
	auto Flags = SW_ERASE | SW_INVALIDATE;
	//	gr: using ScrollWindowEx makes controls reset positions on click, resize and smears.
	//		I think something like the client rect needs to be setup properly...
	//Result = ScrollWindowEx(mHwnd, DeltaX, DeltaY, nullptr, nullptr, nullptr, nullptr, Flags);
	auto Result = ScrollWindow(mHwnd, DeltaX, DeltaY, nullptr, nullptr);
	if (Result == ERROR)
		Platform::ThrowLastError("ScrollWindowEx");
		
	/*
	{
		SCROLLINFO ScrollInfoHorz;
		ScrollInfoHorz.cbSize = sizeof(ScrollInfoHorz);
		ScrollInfoHorz.fMask = SIF_ALL;
		GetScrollInfo(mHwnd, SB_HORZ, &ScrollInfoHorz);

		SCROLLINFO ScrollInfoVert;
		ScrollInfoVert.cbSize = sizeof(ScrollInfoVert);
		ScrollInfoVert.fMask = SIF_ALL;
		GetScrollInfo(mHwnd, SB_VERT, &ScrollInfoVert);

		auto Hdc = GetDC(mHwnd);
		if (!SetWindowOrgEx(Hdc, ScrollInfoHorz.nPos, ScrollInfoVert.nPos, nullptr))
			Platform::ThrowLastError("SetWindowOrgEx");
	}
	*/
	//	repaint (we want to erase background really)
	UpdateWindow(mHwnd);

}


Platform::TOpenglContext::TOpenglContext(TControl& Parent,TOpenglParams& Params) :
	Opengl::TRenderTarget	( Parent.mName ),
	mParent					( Parent )
{
	BYTE ColorDepth = 24;
	DWORD Flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;

	//	gr: no double buffering stops flicker
	//	none of these flags help
	/*
	#define PFD_SWAP_EXCHANGE           0x00000200
	#define PFD_SWAP_COPY               0x00000400
	#define PFD_SWAP_LAYER_BUFFERS      0x00000800
	Flags |= PFD_DOUBLEBUFFER;
	*/

	//	make the pixel format descriptor
	PIXELFORMATDESCRIPTOR pfd =				// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),		// Size Of This Pixel Format Descriptor
		1,									// Version Number
		Flags,					// Must Support Double Buffering
		ColorDepth,									// Select Our Color Depth
		0, 0, 0, 0, 0, 0,					// Color Bits Ignored
		0,									// No Alpha Buffer
		0,									// Shift Bit Ignored
		0,									// No Accumulation Buffer
		0, 0, 0, 0,							// Accumulation Bits Ignored
		16,									// 16Bit Z-Buffer (Depth Buffer)  
		1,									//	use stencil buffer
		0,									// No Auxiliary Buffer
		PFD_MAIN_PLANE,						// Main Drawing Layer
		0,									// Reserved
		0, 0, 0								// Layer Masks Ignored
	};

	uint32_t PixelFormat = 0;

	//	get the existing hdc of the window
	mHDC = GetDC(mParent.mHwnd);
	if ( !mHDC )
		throw Soy::AssertException("Failed to get HDC");

	//	store size
	//m_Size = Window.m_ClientSize;

	/*
	//	if multisample is supported, use a multisample pixelformat
	//	SyncBool MultisampleSupport = OpenglExtensions::IsHardwareSupported(OpenglExtensions::GHardware_ARBMultiSample);
	SyncBool MultisampleSupport = SyncFalse;

	if ( MultisampleSupport == SyncTrue )
	{
	//		PixelFormat = OpenglExtensions::GetArbMultisamplePixelFormat();
	m_HasArbMultiSample = TRUE;
	}
	else*/
	{
		//	check we can use this pfd
		PixelFormat = ChoosePixelFormat(mHDC, &pfd);
		if ( !PixelFormat )
			throw Soy::AssertException("Failed to choose pixel format X");//%d\n", PixelFormat));
		//m_HasArbMultiSample = FALSE;
	}

	//	set it to the pfd
	if ( !SetPixelFormat(mHDC, PixelFormat, &pfd) )
		throw Soy::AssertException("Failed to set pixel format");//%d\n", PixelFormat));

	//	make and get the windows gl context for the hdc
	//	gr: this should be wrapped in a context
	mHGLRC = wglCreateContext(mHDC);
	if ( !mHGLRC )
		throw Soy::AssertException("Failed to create context");

	//	init opengl context
	Lock();
	this->Init();
	Unlock();

	auto OnPaint = [this](TControl& Control)
	{
		this->OnPaint();
	};
	mParent.mOnPaint = OnPaint;
}

Platform::TOpenglContext::~TOpenglContext()
{
	mParent.mOnPaint = nullptr;
	wglDeleteContext(mHGLRC);
	ReleaseDC(mHwnd, mHDC);
}

void Platform::TOpenglContext::Lock()
{
	//	osx does base context lock first
	TContext::Lock();

	try
	{
		//	switch to this thread
		if ( !wglMakeCurrent(mHDC, mHGLRC) )
			throw Soy::AssertException("wglMakeCurrent failed");
	}
	catch(...)
	{
		Unlock();
		throw;
	}
}

void Platform::TOpenglContext::Unlock()
{
	if ( !wglMakeCurrent(mHDC, nullptr) )
		throw Soy::AssertException("wglMakeCurrent unbind failed");

	TContext::Unlock();
}

void Platform::TOpenglContext::Repaint()
{
	mParent.Repaint();
}

void Platform::TControl::Repaint()
{
	//	tell parent to repaint
	//	update window triggers a WM_PAINT, if we've invalidated rect
	//	redrawwindow does it for us

	//	https://stackoverflow.com/a/2328013
	/*
	if ( !UpdateWindow(mParent.mHwnd) )
		Platform::IsOkay("UpdateWindow failed");
	*/
	const RECT * UpdateRect = nullptr;
	HRGN UpdateRegion = nullptr;
	auto Flags = RDW_INTERNALPAINT;//	|RDW_INVALIDATE
	//	gr: not sure if this is threadsafe
	auto DoPaint = [=]()
	{
		if ( !RedrawWindow(mHwnd, UpdateRect, UpdateRegion, Flags) )
			Platform::IsOkay("RedrawWindow failed");
	};
	//GetJobQueue().PushJob(DoPaint);
	DoPaint();
}

void Platform::TOpenglContext::Bind()
{
	//throw Soy::AssertException("Bind default render target");
}

void Platform::TOpenglContext::Unbind()
{
	//throw Soy::AssertException("Unbind default render target");
}



/*
class GlViewRenderTarget : public Opengl::TRenderTarget
{
public:
	GlViewRenderTarget(const std::string& Name) :
		TRenderTarget	( Name )
	{
	}

	virtual Soy::Rectx<size_t>	GetSize() override	{	return mRect;	}
	virtual void				Bind() override;
	virtual void				Unbind() override;

	Soy::Rectx<size_t>			mRect;
};
*/
void Platform::TOpenglContext::OnPaint()
{
	//	better place to flush the queue?
	this->Flush(*this);

	auto& Control = mParent;
	auto& Context = *this;
	Soy::Rectx<size_t> BoundsRect = Control.GetClientRect();
	auto& RenderTarget = *this;
	

	//	render
	//	much of this code should be copy+pasta from osx as its independent logic
	bool DoneLock = false;
	auto LockContext = [&]
	{
		if ( DoneLock )
			return;
		Context.Lock();
		DoneLock = true;
		Opengl::IsOkay("pre drawRect flush",false);
		//	do parent's minimal render
		//	gr: reset state here!
		RenderTarget.mRect = BoundsRect;
		RenderTarget.Bind();
	};
	auto UnlockContext = [&]
	{
		RenderTarget.Unbind();
		Opengl::IsOkay("Post drawRect flush",false);
		Context.Unlock();
	};

	/*
	if ( !mParent )
	{
		auto ContextLock = SoyScope( LockContext, UnlockContext );
		Opengl::ClearColour( Soy::TRgb(1,0,0) );
		return;
	}
	*/
	try
	{
		if ( !mOnRender )
			throw Soy::AssertException("No OnRender callback");

		mOnRender( RenderTarget, LockContext );
	}
	catch(std::exception& e)
	{
		LockContext();
		Opengl::ClearColour( Soy::TRgb(0,0,1) );
		std::Debug << "Window OnRender Exception: " << e.what() << std::endl;
	}

	//	in case lock hasn't been done
	LockContext();

	try
	{
		//	flip
		if ( !SwapBuffers(mHDC) )
			std::Debug << "Failed to SwapBuffers(): " << Platform::GetLastErrorString() <<  std::endl;
		
		UnlockContext();
	}
	catch(std::exception& e)
	{
		UnlockContext();
		std::Debug << e.what() << std::endl;
	}
}


TOpenglWindow::TOpenglWindow(const std::string& Name,const Soy::Rectx<int32_t>& Rect,TOpenglParams Params) :
	SoyWorkerThread		( Soy::GetTypeName(*this), Params.mAutoRedraw ? SoyWorkerWaitMode::Sleep : SoyWorkerWaitMode::Wake ),
	mName				( Name ),
	mParams				( Params )
{
	mWindowThread.reset(new Platform::TWin32Thread(std::string("OpenWindow::") + Name));
	
	//	need to create on the correct thread
	auto CreateControls = [&]()
	{
		bool Resizable = true;
		mWindow.reset(new Platform::TWindow(Name, Rect, *mWindowThread, Resizable ));
		mWindowContext.reset(new Platform::TOpenglContext(*mWindow, Params));

		auto OnRender = [this](Opengl::TRenderTarget& RenderTarget, std::function<void()> LockContext)
		{
			mOnRender(RenderTarget, LockContext);
		};
		mWindowContext->mOnRender = OnRender;

		mWindow->mOnDestroy = [this](Platform::TControl& Control)
		{
			this->OnClosed();
		};

		auto& Win = static_cast<Platform::TControl&>(*mWindow);
		Win.mOnMouseDown = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	this->mOnMouseDown(Pos, Button); };
		Win.mOnMouseUp = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	this->mOnMouseUp(Pos, Button); };
		Win.mOnMouseMove = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	this->mOnMouseMove(Pos, Button); };
		Win.mOnKeyDown = [this](Platform::TControl& Control, SoyKeyButton::Type Key) {	this->mOnKeyDown(Key); };
		Win.mOnKeyUp = [this](Platform::TControl& Control, SoyKeyButton::Type Key) {	this->mOnKeyUp(Key); };
	};
	Soy::TSemaphore Wait;
	mWindowThread->PushJob(CreateControls, Wait);
	Wait.Wait("Win32 thread");

	//	start thread so we auto redraw & run jobs
	Start();
}

TOpenglWindow::~TOpenglWindow()
{
	//	stop thread
	WaitToFinish();
}

void TOpenglWindow::OnClosed()
{
	SoyWindow::OnClosed();
	WaitToFinish();
	mWindowContext.reset();
	mWindow.reset();
}

bool TOpenglWindow::IsValid()
{
	return mWindow != nullptr;
}

bool TOpenglWindow::Iteration()
{
	//	repaint!
	if ( mWindowContext )
		mWindowContext->Repaint();
	return true;
}

std::shared_ptr<Opengl::TContext> TOpenglWindow::GetContext()
{
	return mWindowContext;
}

std::chrono::milliseconds TOpenglWindow::GetSleepDuration()
{
	return std::chrono::milliseconds( 1000/mParams.mRefreshRate );
}

Soy::Rectx<int32_t> TOpenglWindow::GetScreenRect()
{
	auto pWindow = mWindow;
	if ( !pWindow )
		throw Soy::AssertException("GetScreenRect() No window");
	auto& Window = *pWindow;

	auto Rect = Window.GetClientRect();
	return Rect;
}


bool TOpenglWindow::IsFullscreen()
{
	if ( !mWindow )
		throw Soy::AssertException("TOpenglWindow::IsFullscreen missing window");

	return mWindow->IsFullscreen();
}


void TOpenglWindow::EnableScrollBars(bool Horz,bool Vert)
{
	if (!mWindow)
		throw Soy::AssertException("TOpenglWindow::SetFullscreen missing window");
	mWindow->EnableScrollBars(Horz, Vert);
}


void TOpenglWindow::SetFullscreen(bool Fullscreen)
{
	if ( !mWindow )
		throw Soy::AssertException("TOpenglWindow::SetFullscreen missing window");

	//	this needs to be done on the main thread
	auto DoFullScreen = [=]()
	{
		this->mWindow->SetFullscreen(Fullscreen);
	};
	mWindow->GetJobQueue().PushJob(DoFullScreen);
	mWindow->Repaint();
}


void Platform::TWindow::SetFullscreen(bool Fullscreen)
{
	//	don't change current setup
	if ( IsFullscreen() == Fullscreen )
		return;

	//	wierd error:
	//	If the previous value is zero and the function succeeds,
	//	the return value is zero, but the function does not clear the last error information. 
	//	To determine success or failure, clear the last error information by calling SetLastError with 0, 
	//	then call SetWindowLongPtr. Function failure will be indicated by a return value of zero and a 
	//	GetLastError result that is nonzero.
	auto DoSetWindowLongPtr = [](HWND hWnd,int nIndex,LONG_PTR dwNewLong)
	{
		::SetLastError(0);
		auto Result = SetWindowLongPtrA(hWnd, nIndex, dwNewLong);
		//	last setting, 0 might be last state or error
		if ( Result != 0 )
			return;

		auto LastError = ::GetLastError();
		//	no error
		if ( LastError == 0 )
			return;
		
		//	is error
		::SetLastError(LastError);
		Platform::ThrowLastError("SetWindowLong");
	};


	//	if going fullscreen, save placement then go fullscreen
	if ( Fullscreen )
	{
		//	save placement
		mSavedWindowPlacement.reset(new WINDOWPLACEMENT);
		auto& Placement = *mSavedWindowPlacement;
		Placement.length = sizeof(Placement);
		if ( !GetWindowPlacement( mHwnd, &Placement ) )
			Platform::ThrowLastError("GetWindowPlacement failed");
		//	flags is never set, and it's not the style!
		mSavedWindowFlags = GetWindowLongPtrA(mHwnd, GWL_STYLE);
		mSavedWindowExFlags = GetWindowLongPtrA(mHwnd, GWL_EXSTYLE);
				
		//	get monitor size for window
		HMONITOR MonitorHandle = MonitorFromWindow(mHwnd, MONITOR_DEFAULTTONEAREST);
		if ( MonitorHandle == nullptr )
			Platform::ThrowLastError("SetFullscreen(true) Failed to get monitor for window");
		MONITORINFOEXA MonitorMeta;
		MonitorMeta.cbSize = sizeof(MonitorMeta);
		if ( !GetMonitorInfoA( MonitorHandle, &MonitorMeta) )
			Platform::ThrowLastError("GetMonitorInfoA failed");

		//	set size
		//	if any border, doesnt go fullscreen
		//	if no border, flicker!
		auto Border = 0;
		auto x = MonitorMeta.rcMonitor.left;
		auto y = MonitorMeta.rcMonitor.top;
		auto w = MonitorMeta.rcMonitor.right - x;
		auto h = MonitorMeta.rcMonitor.bottom - y - Border;
		bool Repaint = false;
		if ( !MoveWindow(mHwnd, x, y, w, h, Repaint) )
		{
			std::stringstream Error;
			Error << "Failed to MoveMonitor( " << x << "," << y << "," << w << "," << h << ")";
			Platform::ThrowLastError(Error.str());
		}

		
		//	set flags
		auto NewFlags = WS_VISIBLE | WS_POPUP;
		//	WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
		DoSetWindowLongPtr(mHwnd, GWL_STYLE, NewFlags);

		//auto NewExFlags = mSavedWindowExFlags & ~(WS_EX_CLIENTEDGE);
		auto NewExFlags = mSavedWindowExFlags;
		DoSetWindowLongPtr(mHwnd, GWL_EXSTYLE, NewExFlags);

	}
	else
	{
		//	restore last placement
		if ( !mSavedWindowPlacement )
			throw Soy::AssertException("SetFullscreen(false) No saved window placement to restore. Need a backup solution");

		WINDOWPLACEMENT Placement;
		Placement = *mSavedWindowPlacement;
		mSavedWindowPlacement.reset();

		if ( !SetWindowPlacement(mHwnd, &Placement) )
			Platform::ThrowLastError("SetWindowPlacement failed");

		DoSetWindowLongPtr(mHwnd, GWL_STYLE, mSavedWindowFlags);
		DoSetWindowLongPtr(mHwnd, GWL_EXSTYLE, mSavedWindowExFlags);
	}
}

bool Platform::TWindow::IsFullscreen()
{
	auto Flags = GetWindowLongPtrA(mHwnd, GWL_STYLE);
	//	borderless window style
	//	there's not a more sophisticated approach it seems...
	if ( Flags & WS_POPUP )
		return true;

	return false;
}


void Platform::TWindow::EnableScrollBars(bool Horz, bool Vert)
{
	//	https://stackoverflow.com/a/285757/355753
	//	gr: setwindowlong doesn't apply to scrollbars

	//	should we se SB_BOTH?

	if (!ShowScrollBar(mHwnd, SB_HORZ, Horz))
		Platform::ThrowLastError("ShowScrollBar SB_HORZ");

	if (!ShowScrollBar(mHwnd, SB_VERT, Vert))
		Platform::ThrowLastError("ShowScrollBar SB_VERT");
	
}




void Platform::TWin32Thread::Wake()
{
	//	need to send a message to unblock windows message queue (if its blocking)
	//	post message to the thread
	//	if thread id is 0, we haven't done an iteration yet
	//	if we get the "invalid thread" error, there's a chance we've started the thread, and set the value, but GetMessage() or PeekMessage() hasn't been called yet
	UINT WakeMessage = WM_USER;
	WPARAM WakeWParam = 0;
	LPARAM WakeLParam = 0;
	auto ThreadId = mThreadId;

	if ( ThreadId == 0 )
	{
		std::Debug << "Thread hasn't iterated yet" << std::endl;
	}
	else if ( !PostThreadMessageA(ThreadId, WakeMessage, WakeWParam, WakeLParam) )
	{
		auto Error = Platform::GetLastErrorString();
		std::Debug << "PostThreadMessageA Wake() to " << ThreadId << "/" << mThreadId << " error: " << Error << std::endl;
	}

	SoyWorkerJobThread::Wake();
}

bool Platform::TWin32Thread::Iteration(std::function<void(std::chrono::milliseconds)> Sleep)
{
	if ( !SoyWorkerJobThread::Iteration(Sleep) )
		return false;

	//	pump jobs
	//	pump windows queue & block
	bool Continue = true;
	auto OnQuit = [&]()
	{
		Continue = false;
	};
	auto CanBlock = [this]()
	{
		//	if we haven't set the thread id yet, we haven't called Get/PeekMessage, so let it run through and setup the message loop thread id
		if ( mThreadId == 0 )
			return false;

		if ( this->HasJobs() )
			return false;
		return true;
	};
	Platform::Loop( CanBlock, OnQuit );

	//	in order to wake the thread, we need the thread id windows uses to pass messages
	if ( mThreadId == 0 )
		mThreadId = ::GetCurrentThreadId();

	return Continue;
}



std::shared_ptr<Gui::TColourPicker>	Platform::CreateColourPicker(vec3x<uint8_t> InitialColour)
{
	throw Soy::AssertException("Colour picker not yet supported on windows");
}


std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name, Soy::Rectx<int32_t>& Rect,bool Resizable)
{
	std::shared_ptr<TWin32Thread> Thread(new TWin32Thread(Name));
	std::shared_ptr<SoyWindow> Window;

	auto Create = [&]()
	{
		Window.reset(new Platform::TWindow(Name, Rect, *Thread, Resizable ));
		auto& PlatformWindow = *dynamic_cast<Platform::TWindow*>(Window.get());
		PlatformWindow.mOwnThread = Thread;
	};
	Soy::TSemaphore Wait;
	Thread->PushJob(Create, Wait);
	Wait.Wait();

	return Window;
}

std::shared_ptr<SoySlider> Platform::CreateSlider(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	auto& ParentControl = dynamic_cast<Platform::TWindow&>(Parent);
	auto& Thread = ParentControl.mThread;
	std::shared_ptr<SoySlider> Control;

	auto Create = [&]()
	{
		Control.reset(new Platform::TSlider(ParentControl, Rect));
	};
	Soy::TSemaphore Wait;
	Thread.PushJob(Create, Wait);
	Wait.Wait();

	return Control;
}

std::shared_ptr<SoyTextBox> Platform::CreateTextBox(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	auto& ParentControl = dynamic_cast<Platform::TWindow&>(Parent);
	auto& Thread = ParentControl.mThread;
	std::shared_ptr<SoyTextBox> Control;

	auto Create = [&]()
	{
		Control.reset(new Platform::TTextBox(ParentControl, Rect));
	};
	Soy::TSemaphore Wait;
	Thread.PushJob(Create, Wait);
	Wait.Wait();

	return Control;
}

std::shared_ptr<SoyLabel> Platform::CreateLabel(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	auto& ParentControl = dynamic_cast<Platform::TWindow&>(Parent);
	auto& Thread = ParentControl.mThread;
	std::shared_ptr<SoyLabel> Control;

	auto Create = [&]()
	{
		Control.reset(new Platform::TLabel(ParentControl, Rect));
	};
	Soy::TSemaphore Wait;
	Thread.PushJob(Create, Wait);
	Wait.Wait();

	return Control;
}

std::shared_ptr<SoyTickBox> Platform::CreateTickBox(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	auto& ParentControl = dynamic_cast<Platform::TWindow&>(Parent);
	auto& Thread = ParentControl.mThread;
	std::shared_ptr<SoyTickBox> Control;

	auto Create = [&]()
	{
		Control.reset(new Platform::TTickBox(ParentControl, Rect));
	};
	Soy::TSemaphore Wait;
	Thread.PushJob(Create, Wait);
	Wait.Wait();

	return Control;
}



const DWORD Slider_StyleExFlags = 0;
const DWORD Slider_StyleFlags = WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS;
//const DWORD Slider_StyleFlags = WS_CHILD | WS_VISIBLE | /*TBS_AUTOTICKS |*/ TBS_ENABLESELRANGE;

Platform::TSlider::TSlider(TControl& Parent, Soy::Rectx<int32_t>& Rect) :
	TControl("Slider", TRACKBAR_CLASS, Parent, Slider_StyleFlags, Slider_StyleExFlags, Rect)
{
}

void Platform::TSlider::SetMinMax(uint16_t Min, uint16_t Max)
{
	WPARAM Redraw = true;
	auto Result = SendMessage(mHwnd, TBM_SETRANGE, Redraw, MAKELONG(Min, Max));
	//Platform::IsOkay(Result, "TBM_SETRANGE");

	LPARAM PageSize = std::max<LPARAM>(1, (Max - Min) / 10);
	Result = SendMessage(mHwnd, TBM_SETPAGESIZE, 0, PageSize);
	//Platform::IsOkay(Result, "TBM_SETPAGESIZE");
}


void Platform::TSlider::SetValue(uint16_t Value)
{
	WPARAM Redraw = true;
	//auto Result = SendMessage(mHwnd, TBM_SETSEL, Redraw, MAKELONG(iMin, iMax));
	auto Result = SendMessage(mHwnd, TBM_SETPOS, Redraw, Value);
	//Platform::IsOkay(Result, "TBM_SETPOS");
}


uint16_t Platform::TSlider::GetValue()
{
	auto Pos = SendMessage(mHwnd, TBM_GETPOS, 0, 0);
	return Pos;
}

void Platform::TSlider::OnWindowMessage(UINT EventMessage)
{
	//	A trackbar notifies its parent window of user actions by sending the parent a WM_HSCROLL or WM_VSCROLL message. A trackbar with the TBS_HORZ style sends WM_HSCROLL messages. A trackbar with the TBS_VERT style sends WM_VSCROLL messages. The low-order word of the wParam parameter of WM_HSCROLL or WM_VSCROLL contains the notification code. For the TB_THUMBPOSITION and TB_THUMBTRACK notification codes, the high-order word of the wParam parameter specifies the position of the slider. For all other notification codes, the high-order word is zero; send the TBM_GETPOS message to determine the slider position. The lParam parameter is the handle to the trackbar.
	if (EventMessage == WM_HSCROLL || EventMessage == WM_VSCROLL)
	{
		this->OnChanged();
	}

	TControl::OnWindowMessage(EventMessage);
}



const DWORD Label_StyleExFlags = 0;
const DWORD Label_StyleFlags = WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_SIMPLE;


Platform::TLabel::TLabel(TControl& Parent, Soy::Rectx<int32_t>& Rect) :
	TControl("Label", "STATIC", Parent, Label_StyleFlags, Label_StyleExFlags, Rect)
{
}

void Platform::TLabel::SetValue(const std::string& Value)
{
	SetWindowTextA(mHwnd, Value.c_str());
}

std::string Platform::TLabel::GetValue()
{
	throw Soy_AssertException("todo");
}



const DWORD TextBox_StyleExFlags = 0;
const DWORD TextBox_StyleFlags = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT;


Platform::TTextBox::TTextBox(TControl& Parent, Soy::Rectx<int32_t>& Rect) :
	TControl("TextBox", "EDIT", Parent, TextBox_StyleFlags, TextBox_StyleExFlags, Rect)
{
}

void Platform::TTextBox::SetValue(const std::string& Value)
{
	SetWindowTextA(mHwnd, Value.c_str());
}

std::string Platform::TTextBox::GetValue()
{
	throw Soy_AssertException("todo");
}



const DWORD TickBox_StyleExFlags = 0;
const DWORD TickBox_StyleFlags = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
//BS_3STATE
//BS_AUTO3STATE
//BS_CHECKBOX
//BS_AUTOCHECKBOX

Platform::TTickBox::TTickBox(TControl& Parent, Soy::Rectx<int32_t>& Rect) :
	TControl("TickBox", "BUTTON", Parent, TickBox_StyleFlags, TickBox_StyleExFlags, Rect)
{
}

void Platform::TTickBox::SetValue(bool Value)
{
	auto Checked = Value ? BST_CHECKED : BST_UNCHECKED;	//	BST_INDETERMINATE
	Button_SetCheck(mHwnd, Checked);
}

bool Platform::TTickBox::GetValue()
{
	auto Checked = Button_GetCheck(mHwnd);
	return Checked == BST_CHECKED;
}

void Platform::TTickBox::SetLabel(const std::string& Label)
{
	//	is this blocking? safe to use stack variable?
	auto* LabelStr = Label.c_str();
	Button_SetText(mHwnd, LabelStr);
}



void Platform::TTickBox::OnWindowMessage(UINT EventMessage)
{
	if (EventMessage == BN_CLICKED)
	{
		this->OnChanged();
	}

	TControl::OnWindowMessage(EventMessage);
}
