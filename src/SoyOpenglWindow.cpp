#include "SoyOpengl.h"
#include "SoyOpenglWindow.h"
//#include "SoyOpenglView.h"
//#include "PopMain.h"
#include "SoyMath.h"

#include <windowsx.h>
#include "SoyWindow.h"
#include "SoyThread.h"

#include "Win32OpenglContext.h"

#include <commctrl.h>
#pragma comment(lib, "Comctl32.lib")
#include <shellapi.h>	//	drag & drop
#pragma comment(lib, "Shell32.lib")


extern "C"
{
	//	custom ColourButton win32 control
#include "SoyLib/src/Win32ColourControl.h"
#include "SoyLib/src/Win32ImageMapControl.h"
}

#include <magic_enum.hpp>


namespace Platform
{
	class TControlClass;
	class TControl;
	class TWindow;
	class TOpenglView;

	class TOpenglContext;

	class TWin32Thread;	//	A window(control?) is associated with a thread so must pump messages in the same thread https://docs.microsoft.com/en-us/windows/desktop/procthread/creating-windows-in-threads

	//	COM interfaces
	class TDragAndDropHandler;

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
		Win32_Register_ColourButton();
		Win32_Register_ImageMap();
		Start();
	}

	virtual void	Wake() override;
	virtual bool	Iteration(std::function<void(std::chrono::milliseconds)> Sleep);

	bool	mWin32Blocking = true;
	DWORD	mThreadId = 0;
};

/*
//	abstracted for now in case we don't want it on every object
class Platform::TDragAndDropHandler : public IDropTarget
{
public:
	std::function<bool(HDROP)>	mOnTryDragDrop;
	std::function<bool(HDROP)>	mOnDragDrop;

	void	HandleDragDrop(IDataObject *pDataObj, std::function<bool(HDROP)>& CallBack);

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
				REFIID riid,
	_COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override;


	virtual HRESULT STDMETHODCALLTYPE DragEnter(
		__RPC__in_opt IDataObject *pDataObj,
		DWORD grfKeyState,
		POINTL pt,
		 __RPC__inout DWORD *pdwEffect) override;

	virtual HRESULT STDMETHODCALLTYPE DragOver(
		DWORD grfKeyState,
		POINTL pt,
		 __RPC__inout DWORD *pdwEffect) override;

	virtual HRESULT STDMETHODCALLTYPE DragLeave(void) override;

	virtual HRESULT STDMETHODCALLTYPE Drop(
		 __RPC__in_opt IDataObject *pDataObj,
		DWORD grfKeyState,
		POINTL pt,
		 __RPC__inout DWORD *pdwEffect) override;
};
*/

class Platform::TControl// : public Platform::TDragAndDropHandler
{
public:
	TControl(const std::string& Name, TControlClass& Class, TControl* Parent, DWORD StyleFlags, DWORD StyleExFlags, Soy::Rectx<int> Rect, TWin32Thread& Thread);
	TControl(const std::string& Name,const char* ClassName,TControl& Parent, DWORD StyleFlags, DWORD StyleExFlags, Soy::Rectx<int> Rect);
	virtual ~TControl();

	//	trigger actions
	void					Repaint();
	void					EnableDragAndDrop();

	//	reflection
	Soy::Rectx<int32_t>		GetClientRect();
	void					SetClientRect(const Soy::Rectx<int32_t>& Rect)
	{
		std::Debug << "todo SetClientRect(" << Rect << ")" << std::endl;
	}

	//	callbacks from windows message loop
	virtual void			OnDestroyed();		//	window handle is being destroyed
	virtual void			OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) {}	//	called from window thread which means we can flush jobs
	virtual void			OnScrolled(bool Horizontal,uint16_t ScrollCommand,uint16_t CurrentScrollPosition);

	//	return true if handled, or false to return default behavouir
	bool			OnMouseEvent(int x, int y, WPARAM Flags,UINT EventMessage);
	bool			OnKeyDown(WPARAM KeyCode, LPARAM Flags);
	bool			OnKeyUp(WPARAM KeyCode, LPARAM Flags);
	bool			OnDragDrop(HDROP DropHandle);

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
	std::function<bool(TControl&, ArrayBridge<std::string>&&)>	mOnDragAndDrop;

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

	virtual void	OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
	virtual void	OnScrolled(bool Horizontal,uint16_t ScrollCommand, uint16_t CurrentScrollPosition) override;

	virtual Soy::Rectx<int32_t>		GetScreenRect() override	{	return GetClientRect();	}
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	virtual bool					IsMinimised() override;
	virtual bool					IsForeground() override;
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



class Platform::TOpenglContext : public  Opengl::TRenderTarget, public Win32::TOpenglContext
{
public:
	TOpenglContext(TControl& Parent,TOpenglParams& Params);
	~TOpenglContext();

	//	render target
	virtual void				Bind() override;
	virtual void				Unbind() override;
	virtual Soy::Rectx<size_t>	GetSize() override;

	//	window stuff
	void			Repaint();
	void			OnPaint();

	std::function<void(Opengl::TRenderTarget&, std::function<void()>)>	mOnRender;

	//	win32::TOpenglContext
	virtual HDC		GetHdc() override { return mHDC; }
	virtual HGLRC	GetHglrc() override { return mHGLRC; }
	virtual HWND	GetHwnd() override { return mHwnd; }

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
	virtual void		SetMinMax(uint16_t Min, uint16_t Max,uint16_t NotchCount) override;
	virtual void		SetValue(uint16_t Value) override;
	virtual uint16_t	GetValue() override;

	virtual void		OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
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

	virtual void		OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;

private:
	bool				mTextChangedByCode = true;
};


class Platform::TTickBox : public TControl, public SoyTickBox
{
public:
	TTickBox(TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetValue(bool Value) override;
	virtual bool		GetValue() override;
	virtual void		SetLabel(const std::string& Label) override;

	virtual void		OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
};


class Platform::TColourButton : public TControl, public SoyColourButton
{
public:
	TColourButton(TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void			SetValue(vec3x<uint8_t> Value) override;
	virtual vec3x<uint8_t>	GetValue() override;

	virtual void			OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
};


class Platform::TImageMap : public TControl, public Gui::TImageMap
{
public:
	TImageMap(TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void			SetImage(const SoyPixelsImpl& Pixels) override;
	virtual void			SetCursorMap(const SoyPixelsImpl& CursorMap, const ArrayBridge<std::string>&& CursorIndexes)override;

	virtual void			OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
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
		Control.OnWindowMessage(message, wParam, lParam);

		switch (message)
		{
			//	gotta allow some things
		case WM_MOVE:
		case WM_SIZE:
		case WM_CREATE:
		case WM_SHOWWINDOW:
			DragAcceptFiles(hwnd, TRUE);
			return 0;

		case WM_ERASEBKGND:
			return Default();

		case WM_DESTROY:
			Control.OnDestroyed();
			return 0;

		case WM_PAINT:
			if (!Control.mOnPaint)
				return Default();

			//	setup paint and call overload
			//	gr: should be checking error returns here
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);
			try
			{
				Control.mOnPaint(Control);
			}
			catch (std::exception& e)
			{
				std::Debug << "Exception OnPaint: " << e.what() << std::endl;
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
			if (Control.OnMouseEvent(x, y, wParam, message))
				return 0;
			return Default();
		}

		case WM_KEYDOWN:
		{
			auto KeyCode = wParam;
			auto Flags = lParam;
			if (Control.OnKeyDown(KeyCode, Flags))
				return 0;
			return Default();
		}

		case WM_KEYUP:
		{
			auto KeyCode = wParam;
			auto Flags = lParam;
			if (Control.OnKeyUp(KeyCode, Flags))
				return 0;
			return Default();
		}

		case WM_COMMAND:
		{
			//	lparam is hwnd to child control
			if (lParam != 0)
			{
				auto SubMessage = HIWORD(wParam);
				auto ControlIdentifier = LOWORD(wParam);
				auto ChildHandle = reinterpret_cast<HWND>(lParam);
				try
				{
					auto& Child = Control.GetChild(ChildHandle);
					Child.OnWindowMessage(SubMessage, wParam, lParam);
				}
				catch (std::exception& e)
				{
					std::Debug << "Exception with WM_COMMAND(0x" << std::hex << SubMessage << std::dec << ")  to child: " << e.what() << std::endl;
				}
			}
			return Default();
		}

		case WM_HSCROLL:
		case WM_VSCROLL:
		{
			//	lparam is hwnd to child control
			if (lParam != 0)
			{
				auto ChildHandle = reinterpret_cast<HWND>(lParam);
				auto& Child = Control.GetChild(ChildHandle);
				Child.OnWindowMessage(message, wParam, lParam);
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

		case WM_DROPFILES:
		{
			auto DropHandle = reinterpret_cast<HDROP>(wParam);
			auto Handled = Control.OnDragDrop(DropHandle);
			if (Handled)
				return true;
			return Default();
		}

		default:
			//std::Debug << "unhandled message " << std::hex << message << " on "<< Control.mName << std::endl;
			break;
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
	//wc.hCursor		= nullptr;//LoadCursor(NULL, IDC_ARROW);
	wc.hCursor		=  LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = BackgroundBrush;
	wc.lpszMenuName	= nullptr;
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

	//	for now, enable drag & drop on everything
	EnableDragAndDrop();
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

	EnableDragAndDrop();
}

Platform::TControl::~TControl()
{
}

void Platform::TControl::EnableDragAndDrop()
{
#if defined(ENABLE_DRAGDROP_HANDLER)
	auto Result = RegisterDragDrop(mHwnd, this);
	Platform::IsOkay(Result, "RegisterDragDrop");
#else
	DragAcceptFiles(mHwnd, true);
#endif
}

bool Platform::TControl::OnDragDrop(HDROP DropHandle)
{
	//	no callback, not handled
	if (!mOnDragAndDrop)
		return false;

	//	https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragqueryfilea
	UINT FileCount = 0;
	{
		char FilenameBuffer[1024];
		FileCount = DragQueryFileA(DropHandle, 0xFFFFFFFF, FilenameBuffer, std::size(FilenameBuffer));
	}

	Array<std::string> Filenames;
	for (auto i = 0; i < FileCount; i++)
	{
		char FilenameBuffer[1024];
		auto FilenameLength = DragQueryFileA(DropHandle, i, FilenameBuffer, std::size(FilenameBuffer));
		std::string Filename(FilenameBuffer, FilenameLength);
		Filenames.PushBack(Filename);
	}

	auto Handled = mOnDragAndDrop(*this,GetArrayBridge(Filenames));
	return Handled;
}


bool Platform::TControl::OnMouseEvent(int x, int y, WPARAM Flags,UINT EventMessage)
{
	auto LeftDown = (Flags & MK_LBUTTON) != 0;
	auto MiddleDown = (Flags & MK_MBUTTON) != 0;
	auto RightDown = (Flags & MK_RBUTTON) != 0;
	auto BackDown = (Flags & MK_XBUTTON1) != 0;
	auto ForwardDown = (Flags & MK_XBUTTON2) != 0;
	auto* pMouseEvent = &mOnMouseMove;
	switch(EventMessage)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:	
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
		pMouseEvent = &mOnMouseDown;
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		pMouseEvent = &mOnMouseUp;
		break;

	case WM_MOUSEMOVE:	
		pMouseEvent = &mOnMouseMove;	
		break;
	}

	auto Button = SoyMouseButton::None;
	if ( LeftDown )			Button = SoyMouseButton::Left;
	else if ( RightDown )	Button = SoyMouseButton::Right;
	else if (MiddleDown)	Button = SoyMouseButton::Middle;
	else if (BackDown)		Button = SoyMouseButton::Back;
	else if (ForwardDown)	Button = SoyMouseButton::Forward;

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

	auto ParentHwnd = GetParent(mHwnd);
	POINT Position = { 0 };

	//	gr: this applies the scroll, which it shouldn't, we're treating client rect like a virtual canvas
	::MapWindowPoints(mHwnd, ParentHwnd, &Position, 1 );
	RectWin.left += Position.x;
	RectWin.top += Position.y;
	RectWin.right += Position.x;
	RectWin.bottom += Position.y;

	/*	this is bad, but I think we can continue, need to cope with scrolling
	//	from MapWindowPoints()
	if ( RectWin.left < 0 || RectWin.top < 0 )
	{
		auto RectSigned = GetRect<int32_t>(RectWin);
		std::stringstream Error;
		Error << "Not expected GetClientRect rect to be < 0; " << RectSigned;
		throw Soy::AssertException(Error.str());
	}
	*/

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


	//	join super class events
	mOnTryDragDrop = [](ArrayBridge<std::string>&)
	{
		return true; 
	};
	mOnDragAndDrop = [this](TControl&, ArrayBridge<std::string>&& Filenames)
	{
		SoyWindow::mOnDragDrop(Filenames);
		return true;
	};
}

void Platform::TWindow::OnWindowMessage(UINT Message, DWORD WParam, DWORD LParam)
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
	//	page is one screen
	auto ClientRect = GetClientRect();
	ClientRect.x = 0;
	ClientRect.y = 0;
	//std::Debug << "UpdateScrollbars( " << ClientRect << ")" << std::endl;

	//	get our client rect with all controls
	auto WindowRect = ClientRect;
	for (auto i = 0; i < mChildren.GetSize(); i++)
	{
		auto& Child = *mChildren[i];
		auto ChildRect = Child.GetClientRect();
		WindowRect.Accumulate(ChildRect.Left(), ChildRect.Top());
		WindowRect.Accumulate(ChildRect.Right(), ChildRect.Bottom());
	}

	SCROLLINFO ScrollInfoVert;
	ScrollInfoVert.cbSize = sizeof(ScrollInfoVert);
	ScrollInfoVert.fMask = SIF_RANGE | SIF_PAGE;
	ScrollInfoVert.nPage = ClientRect.GetHeight() / 4;	//	full page breaks the scrolling, capping wrong
	ScrollInfoVert.nMin = WindowRect.Top();
	ScrollInfoVert.nMax = WindowRect.Bottom();
	SetScrollInfo(mHwnd, SB_VERT, &ScrollInfoVert, true);

	SCROLLINFO ScrollInfoHorz;
	ScrollInfoHorz.cbSize = sizeof(ScrollInfoHorz);
	ScrollInfoHorz.fMask = SIF_RANGE | SIF_PAGE;
	ScrollInfoHorz.nPage = ClientRect.GetWidth() / 4;	//	full page breaks the scrolling, capping wrong
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

	/*
	std::Debug << "Scroll ";
	std::Debug << " nMin=" << ScrollInfo.nMin;
	std::Debug << " nMax=" << ScrollInfo.nMax;
	std::Debug << " nPage=" << ScrollInfo.nPage;
	std::Debug << " nPos=" << ScrollInfo.nPos;
	std::Debug << " nTrackPos=" << ScrollInfo.nTrackPos;
	std::Debug << std::endl;
	*/
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


Soy::Rectx<size_t> Platform::TOpenglContext::GetSize()
{
	//	rect is now correct for window, but for opengl context, it should still be at 0,0
	auto Rect = mRect;
	Rect.x = 0;
	Rect.y = 0;
	return Rect;
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
	//return;

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
		try
		{
			RenderTarget.Unbind();
			Opengl::IsOkay("UnlockContext RenderTarget.Unbind", false);
			Context.Unlock();
		}
		catch (std::exception& e)
		{
			std::Debug << "UnlockContext unbind failed (" << e.what() << "), hail mary context unlock" << std::endl;
			Context.Unlock();
			//	rethrow?
			//throw;
		}
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
		std::Debug << "Window OnRender Exception: " << e.what() << std::endl;
		try
		{
			UnlockContext();
		}
		catch (std::exception&e)
		{
			//std::Debug << "OnRender UnlockContext exception " << e.what() << std::endl;
		}
		return;
		/*
		//	gr: if there's an exception here, it might be the LockContext, rather than the render...
		//		so we're just failing again...
		try
		{
			LockContext();
			Opengl::ClearColour(Soy::TRgb(0, 0, 1));
			std::Debug << "Window OnRender Exception: " << e.what() << std::endl;
		}
		catch (std::exception& e)
		{
			//	okay, lock is the problem
			UnlockContext();
			return;
		}
		*/
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
	//	this can be called at the same time that JS is assigning events
	//	so don't assume they're non-null
	//	do they need a lock?
	auto CreateControls = [&]()
	{
		bool Resizable = true;
		mWindow.reset(new Platform::TWindow(Name, Rect, *mWindowThread, Resizable ));
		mWindowContext.reset(new Platform::TOpenglContext(*mWindow, Params));

		auto OnRender = [this](Opengl::TRenderTarget& RenderTarget, std::function<void()> LockContext)
		{
			if (!mOnRender)
				return;
			mOnRender(RenderTarget, LockContext);
		};
		mWindowContext->mOnRender = OnRender;

		mWindow->mOnDestroy = [this](Platform::TControl& Control)
		{
			this->OnClosed();
		};

		//	gr: can I use std::bind?
		auto& Win = static_cast<Platform::TControl&>(*mWindow);
		Win.mOnMouseDown = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button)	{	if ( this->mOnMouseDown ) this->mOnMouseDown(Pos, Button); };
		Win.mOnMouseUp = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button)		{	if ( mOnMouseUp ) this->mOnMouseUp(Pos, Button); };
		Win.mOnMouseMove = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button)	{	if ( mOnMouseMove ) this->mOnMouseMove(Pos, Button); };
		Win.mOnKeyDown = [this](Platform::TControl& Control, SoyKeyButton::Type Key)	{	if (mOnKeyDown )	this->mOnKeyDown(Key); };
		Win.mOnKeyUp = [this](Platform::TControl& Control, SoyKeyButton::Type Key)		{	if (mOnKeyUp )		this->mOnKeyUp(Key); };
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
	//	gr: do this at a higher(c++) level so the check is cross platform
	if (!mEnableRenderWhenMinimised)
	{
		if (IsMinimised())
			return true;
	}

	if (!mEnableRenderWhenBackground)
	{
		if (!IsForeground())
			return true;
	}

	//	trigger repaint!
	if (mWindowContext)
	{
		mWindowContext->Repaint();
	}

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

bool TOpenglWindow::IsMinimised()
{
	if (!mWindow)
		throw Soy::AssertException("TOpenglWindow::IsMinimised missing window");

	return mWindow->IsMinimised();
}

bool TOpenglWindow::IsForeground()
{
	if (!mWindow)
		throw Soy::AssertException("TOpenglWindow::IsForeground missing window");

	return mWindow->IsForeground();
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

std::shared_ptr<Win32::TOpenglContext> TOpenglWindow::GetWin32Context()
{
	auto Win32Context = std::dynamic_pointer_cast<Win32::TOpenglContext>(mWindowContext);
	return Win32Context;
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


bool Platform::TWindow::IsMinimised()
{
	return IsIconic(mHwnd);
}

bool Platform::TWindow::IsForeground()
{
	//	check in case we're invalid AND the foreground window is null (ie, other process)
	if (!mHwnd)
		return false;

	//	this post signifies why we use GetForegroundWindow() and not GetActiveWindow() (it will be null from a background thread)
	//	https://stackoverflow.com/a/28643729/355753
	auto ForegroundWindow = GetForegroundWindow();
	return mHwnd == ForegroundWindow;
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

std::shared_ptr<SoyLabel> Platform::GetLabel(SoyWindow& Parent,const std::string& Name)
{
	//	todo: search window for elements with a specific ID so we can create from resource forms
	Soy_AssertTodo();
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

std::shared_ptr<SoyColourButton> Platform::CreateColourButton(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	auto& ParentControl = dynamic_cast<Platform::TWindow&>(Parent);
	auto& Thread = ParentControl.mThread;
	std::shared_ptr<SoyColourButton> Control;

	auto Create = [&]()
	{
		Control.reset(new Platform::TColourButton(ParentControl, Rect));
	};
	Soy::TSemaphore Wait;
	Thread.PushJob(Create, Wait);
	Wait.Wait();

	return Control;
}


std::shared_ptr<Gui::TImageMap> Platform::CreateImageMap(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	auto& ParentControl = dynamic_cast<Platform::TWindow&>(Parent);
	auto& Thread = ParentControl.mThread;
	std::shared_ptr<Gui::TImageMap> Control;

	auto Create = [&]()
	{
		Control.reset(new Platform::TImageMap(ParentControl, Rect));
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

void Platform::TSlider::SetMinMax(uint16_t Min, uint16_t Max,uint16_t NotchCount)
{
	WPARAM Redraw = true;
	auto Result = SendMessage(mHwnd, TBM_SETRANGE, Redraw, MAKELONG(Min, Max));
	//Platform::IsOkay(Result, "TBM_SETRANGE");

	LPARAM PageSize = std::max<LPARAM>(1, (Max - Min) / 10);
	Result = SendMessage(mHwnd, TBM_SETPAGESIZE, 0, PageSize);
	//Platform::IsOkay(Result, "TBM_SETPAGESIZE");

	//	set ticks
	//	with auto tic's we can set frequency, but TBM_SETTIC  lets us specify exact ones... see what OSX can do!
	WPARAM Frequency = 0;
	if (NotchCount != 0)
	{
		Frequency = (Max - Min) / NotchCount;
	}
	Result = SendMessage(mHwnd, TBM_SETTICFREQ, Frequency, 0);
	Platform::IsOkay(static_cast<HRESULT>(Result), "TBM_SETTICFREQ");
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

namespace TrackbarNotification
{
	enum Name
	{
		TrackbarNotification_BOTTOM = TB_BOTTOM,
		TrackbarNotification_ENDTRACK = TB_ENDTRACK,
		TrackbarNotification_LINEDOWN = TB_LINEDOWN,
		TrackbarNotification_LINEUP = TB_LINEUP,
		TrackbarNotification_PAGEDOWN = TB_PAGEDOWN,
		TrackbarNotification_PAGEUP = TB_PAGEUP,
		TrackbarNotification_THUMBPOSITION = TB_THUMBPOSITION,
		TrackbarNotification_THUMBTRACK = TB_THUMBTRACK,
		TrackbarNotification_TOP = TB_TOP,
	};
}


void Platform::TSlider::OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam)
{
	//	A trackbar notifies its parent window of user actions by sending the parent a WM_HSCROLL or WM_VSCROLL message. 
	//	A trackbar with the TBS_HORZ style sends WM_HSCROLL messages.
	//	A trackbar with the TBS_VERT style sends WM_VSCROLL messages. 
	//	The low-order word of the wParam parameter of WM_HSCROLL or WM_VSCROLL contains the notification code.
	//	For the TB_THUMBPOSITION and TB_THUMBTRACK notification codes,
	//	the high-order word of the wParam parameter specifies the position of the slider. 
	//	For all other notification codes, the high-order word is zero; send the TBM_GETPOS message to determine the slider position. 
	//	The lParam parameter is the handle to the trackbar.
	if (EventMessage == WM_HSCROLL || EventMessage == WM_VSCROLL)
	{
		auto Notification = LOWORD(WParam);
		auto NotifcationName = magic_enum::enum_name(static_cast<TrackbarNotification::Name>(Notification));
		//std::Debug << "Trackbar notification : " << NotifcationName << std::endl;
		auto FinalValue = Notification == TB_ENDTRACK;
		this->OnChanged(FinalValue);
	}

	TControl::OnWindowMessage(EventMessage, WParam, LParam);
}



const DWORD Label_StyleExFlags = 0;
const DWORD Label_StyleFlags = WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_LEFTNOWORDWRAP;
//	gr: removed SS_SIMPLE as it doesn't clear the whole box when repainting

Platform::TLabel::TLabel(TControl& Parent, Soy::Rectx<int32_t>& Rect) :
	TControl("Label", "STATIC", Parent, Label_StyleFlags, Label_StyleExFlags, Rect)
{
}

void Platform::TLabel::SetValue(const std::string& Value)
{
	auto Success = SetWindowTextA(mHwnd, Value.c_str());
	if (!Success)
		Platform::ThrowLastError("SetWindowTextA");
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
	//	SetWindowText is called by the constructor, (we currently miss the cmd)
	//	but just in case, initialise to true, and after construction turn it off
	mTextChangedByCode = false;
}

void Platform::TTextBox::SetValue(const std::string& Value)
{
	//	need a flag to say WE have caused this
	//	this call is synchronous, so we can
	mTextChangedByCode = true;
	SetWindowTextA(mHwnd, Value.c_str());
	mTextChangedByCode = false;
}

std::string Platform::TTextBox::GetValue()
{
	//	length... without terminator
	auto Length = GetWindowTextLengthA(mHwnd);
	//	gr: this can be an error
	if (Length == 0)
		return std::string();
	if (Length < 0)
	{
		std::stringstream Error;
		Error << "GetWindowTextLengthA() on text box returned < 0 (" << Length << ")";
		Platform::ThrowLastError(Error.str());
	}

	//	length + terminator
	Array<char> StringBuffer(Length+1);
	Length = GetWindowTextA(mHwnd, StringBuffer.GetArray(), StringBuffer.GetSize());
	StringBuffer.SetSize(Length, true);
	StringBuffer.PushBack('\0');

	std::string String(StringBuffer.GetArray(), Length);
	return String;
}

void Platform::TTextBox::OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam)
{
	//	EN_CHANGE value changed (docs say by user, but is being triggered from code)
	//	EN_UPDATE updated to screen
	if (EventMessage == EN_CHANGE)
	{
		if (!mTextChangedByCode)
			OnChanged();
		return;
	}

	//std::Debug << "Textboxmessage " << std::hex << EventMessage << std::dec << std::endl;
	TControl::OnWindowMessage(EventMessage, WParam, LParam);
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



void Platform::TTickBox::OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam)
{
	if (EventMessage == BN_CLICKED)
	{
		this->OnChanged();
	}

	TControl::OnWindowMessage(EventMessage,WParam,LParam);
}






const DWORD ColourButton_StyleExFlags = 0;
const DWORD ColourButton_StyleFlags = WS_CHILD | WS_VISIBLE | WS_TABSTOP;


Platform::TColourButton::TColourButton(TControl& Parent, Soy::Rectx<int32_t>& Rect) :
	TControl("ColourButton", COLOURBUTTON_CLASSNAME, Parent, ColourButton_StyleFlags, ColourButton_StyleExFlags, Rect)
{
}

void Platform::TColourButton::SetValue(vec3x<uint8_t> Value)
{
	ColourButton_SetColour(mHwnd, Value.x, Value.y, Value.z,false);
}

vec3x<uint8_t> Platform::TColourButton::GetValue()
{
	//	get new value
	uint8_t Rgb[3];
	ColourButton_GetColour(mHwnd, Rgb);
	return vec3x<uint8_t>(Rgb[0], Rgb[1], Rgb[2]);
}

void Platform::TColourButton::OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam)
{
	if (EventMessage == COLOURBUTTON_COLOURCHANGED)
	{
		this->OnChanged(true);
	}
	/*
	if (EventMessage == COLOURBUTTON_COLOURDIALOGCLOSED)
	{
		this->OnChanged(true);
	}
	*/
	TControl::OnWindowMessage(EventMessage, WParam, LParam);
}



const DWORD ImageMap_StyleExFlags = 0;
const DWORD ImageMap_StyleFlags = WS_CHILD | WS_VISIBLE | WS_TABSTOP;


Platform::TImageMap::TImageMap(TControl& Parent, Soy::Rectx<int32_t>& Rect) :
	TControl("ImageMap", IMAGEMAP_CLASSNAME, Parent, ImageMap_StyleFlags, ImageMap_StyleExFlags, Rect)
{
}

void Platform::TImageMap::OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam)
{
	/*
	if (EventMessage == COLOURBUTTON_COLOURDIALOGCLOSED)
	{
		this->OnChanged(true);
	}
	*/
	TControl::OnWindowMessage(EventMessage, WParam, LParam);
}


void Platform::TImageMap::SetImage(const SoyPixelsImpl& Pixels)
{
	const SoyPixelsImpl* RgbPixels = &Pixels;
	SoyPixels ConvertedPixels;
	if (Pixels.GetFormat() != SoyPixelsFormat::BGR)
	{
		Soy::TScopeTimerPrint Timer("TImageMap::SetImage Conversion to BGR", 1);
		ConvertedPixels.Copy(Pixels);
		ConvertedPixels.SetFormat(SoyPixelsFormat::BGR);
		RgbPixels = &ConvertedPixels;
	}

	auto* PixelBuffer = RgbPixels->GetPixelsArray().GetArray();
	auto Meta = RgbPixels->GetMeta();
	ImageMap_SetImage(mHwnd, PixelBuffer, Meta.GetWidth(), Meta.GetHeight());
}


LPCSTR GetCursorName(SoyCursor::Type Cursor)
{
	switch (Cursor)
	{
	case SoyCursor::ArrowAndWait:	return IDC_APPSTARTING;
	case SoyCursor::Arrow:			return IDC_ARROW;
	case SoyCursor::Cross:			return IDC_CROSS;
	case SoyCursor::Hand:			return IDC_HAND;
	case SoyCursor::Help:			return IDC_HELP;
	case SoyCursor::TextCursor:		return IDC_IBEAM;
	case SoyCursor::NotAllowed:		return IDC_NO;
	case SoyCursor::ResizeAll:		return IDC_SIZEALL;
	case SoyCursor::ResizeVert:		return IDC_SIZENS;
	case SoyCursor::ResizeHorz:		return IDC_SIZEWE;
	case SoyCursor::ResizeNorthEast:return IDC_SIZENESW;
	case SoyCursor::ResizeNorthWest:return IDC_SIZENWSE;
	case SoyCursor::UpArrow:		return IDC_UPARROW;
	case SoyCursor::Wait:			return IDC_WAIT;
	}

	std::stringstream Error;
	Error << __PRETTY_FUNCTION__ << " Unhandled cursor type " << Cursor;
	throw Soy::AssertException(Error);
}

HCURSOR GetCursor(SoyCursor::Type Cursor)
{
	LPCSTR Name = GetCursorName(Cursor);
	auto Handle = LoadCursorA(nullptr, Name);
	return Handle;
}

//	we use strings because cursors can be named in EXE resources
HCURSOR GetCursor(const std::string& CursorName)
{
	//	see if it's predefined
	auto CursorMaybe = magic_enum::enum_cast<SoyCursor::Type>(CursorName);
	if (CursorMaybe.has_value())
	{
		return GetCursor(CursorMaybe.value());
	}

	//	try and load from exe resource
	auto Cursor = LoadCursorA(Platform::Private::InstanceHandle, CursorName.c_str());
	if (!Cursor)
		Platform::ThrowLastError(std::string("Failed to LoadCursor(") + CursorName);

	return Cursor;
}

void Platform::TImageMap::SetCursorMap(const SoyPixelsImpl& CursorPixelMap, const ArrayBridge<std::string>&& CursorIndexes)
{
	Array<HCURSOR> CursorIndexMap;
	for (auto i = 0; i < CursorIndexes.GetSize(); i++)
	{
		auto CursorName = CursorIndexes[i];
		auto Cursor = GetCursor(CursorName);
		CursorIndexMap.PushBack(Cursor);
	}

	//	for initial effeciency
	if (CursorPixelMap.GetFormat() != SoyPixelsFormat::Greyscale)
	{
		std::stringstream Error;
		Error << __PRETTY_FUNCTION__ << " requires cursor map in Greyscale, not " << CursorPixelMap.GetFormat();
		throw Soy::AssertException(Error);
	}
	
	//	turn into a map of cursor handles
	Array<HCURSOR> CursorMap;
	auto& Indexes = CursorPixelMap.GetPixelsArray();
	for (auto i=0;	i< Indexes.GetSize();	i++ )
	{
		auto Index = Indexes[i];
		//	check range
		auto Cursor = CursorIndexMap[Index];
		CursorMap.PushBack(Cursor);
	}

	ImageMap_SetCursorMap(mHwnd, CursorMap.GetArray(), CursorPixelMap.GetWidth(), CursorPixelMap.GetHeight());
}


#if defined(ENABLE_DRAGDROP_HANDLER)
HRESULT Platform::TDragAndDropHandler::DragEnter(IDataObject *pDataObj,DWORD grfKeyState,POINTL pt,__RPC__inout DWORD *pdwEffect)
{
	if (!mOnTryDragDrop)
	{
		*pdwEffect = DROPEFFECT_NONE;
		return E_NOTIMPL;
	}
	//	extract drop handle
	try
	{
		if (!HandleDragDrop(pDataObj, mTryDragDrop))
		{
			*pdwEffect = DROPEFFECT_NONE;
			return E_INVALIDARG;
		}
		
		*pdwEffect = DROPEFFECT_COPY;
		return S_OK;
	}
	catch (std::exception& Exception)
	{
		*pdwEffect = DROPEFFECT_NONE;
		return E_INVALIDARG;
	}
}

virtual HRESULT Platform::TDragAndDropHandler::DragOver(DWORD grfKeyState,POINTL pt,__RPC__inout DWORD *pdwEffect)
{
	return E_NOTIMPL;
}

virtual HRESULT Platform::TDragAndDropHandler::DragLeave(void)
{
	return E_NOTIMPL;
}

virtual HRESULT  Platform::TDragAndDropHandler::Drop( __RPC__in_opt IDataObject *pDataObj,DWORD grfKeyState,POINTL pt, __RPC__inout DWORD *pdwEffect)
{
	*pdwEffect = DROPEFFECT_COPY;
	return S_OK;
}
#endif
