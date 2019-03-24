#include "SoyOpengl.h"
#include "SoyOpenglWindow.h"
//#include "SoyOpenglView.h"
//#include "PopMain.h"


namespace Platform
{
	class TControlClass;
	class TControl;
	class TWindow;
	class TOpenglContext;

	namespace Private
	{
		HINSTANCE InstanceHandle = nullptr;
	}

	UINT	g_MouseWheelMsg = 0;
	LRESULT CALLBACK	Win32CallBack(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	void		Loop(bool Blocking,std::function<void()> OnQuit);
}

void Platform::Loop(bool Blocking,std::function<void()> OnQuit)
{
	while ( true )
	{
		MSG msg;
		if ( Blocking )
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
}


LRESULT CALLBACK Platform::Win32CallBack(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		//	gotta allow some things
	case WM_MOVE:
	case WM_SIZE:
	case WM_CREATE:
	case WM_PAINT:
	case WM_ERASEBKGND:
	case WM_SHOWWINDOW:
		return 0;

		//	*need* to handle these with defwndproc
	case WM_GETMINMAXINFO:
	case WM_NCCREATE:
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam) ;
}


class Platform::TControl
{
public:
	TControl(const std::string& Name,TControlClass& Class,TControl* Parent);

	HWND		mHwnd;
	std::string	mName;
};

class Platform::TWindow : TControl
{
public:
	TWindow(const std::string& Name);
};


class Platform::TOpenglContext : public Opengl::TContext
{
public:
	TOpenglContext(TControl& Parent);

	virtual void	Lock() override;
	virtual void	Unlock() override;
};



class Platform::TControlClass
{
public:
	TControlClass(const std::string& Name, uint16_t Style);
	~TControlClass();

	const char*	ClassName() const {		return mClassName.c_str();	}

	WNDCLASS	mClass;
	std::string	mClassName;
};

Platform::TControlClass::TControlClass(const std::string& Name, uint16_t Style) :
	mClassName	( Name )
{
	auto* NameStr = mClassName.c_str();
	HICON IconHandle = LoadIcon(NULL, IDI_APPLICATION);
	HBRUSH BackgroundBrush = GetSysColorBrush(COLOR_WINDOW);
	
	auto& wc = mClass;
	ZeroMemory(&wc,sizeof(wc));
	wc.style		= Style;
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


Platform::TControl::TControl(const std::string& Name,TControlClass& Class,TControl* Parent) :
	mName	( Name )
{
	DWORD StyleFlags = 0x0;
	const char* ClassName = Class.ClassName();
	const char* WindowName = mName.c_str();
	uint16_t StyleExFlags = 0;
	Soy::Rectx<int> Rect(0, 0, 100, 100);
	void* Data = this;
	HWND ParentHwnd = Parent ? Parent->mHwnd : nullptr;
	auto Instance = Platform::Private::InstanceHandle;
	HMENU Menu = nullptr;

	mHwnd = CreateWindowEx(StyleExFlags, ClassName, WindowName, StyleFlags, Rect.x, Rect.y, Rect.GetWidth(), Rect.GetHeight(), ParentHwnd, Menu, Instance, Data);
	Platform::IsOkay("CreateWindow");
	if ( !mHwnd )
		throw Soy::AssertException("Failed to create window");

	/*
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
    */
}

Platform::TControlClass& GetWindowClass()
{
	static std::shared_ptr<Platform::TControlClass> gWindowClass;
	if ( gWindowClass )
		return *gWindowClass;

	uint16_t Style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	Style |= WS_OVERLAPPEDWINDOW;
	Style |= WS_EX_CLIENTEDGE;

	gWindowClass.reset(new Platform::TControlClass("Window", Style));
	return *gWindowClass;
}

Platform::TWindow::TWindow(const std::string& Name) :
	TControl	( Name, GetWindowClass(), nullptr )
{
	ShowWindow(mHwnd, SW_SHOW);
}



TOpenglWindow::TOpenglWindow(const std::string& Name,Soy::Rectf Rect,TOpenglParams Params) :
	SoyWorkerThread		( Soy::GetTypeName(*this), Params.mAutoRedraw ? SoyWorkerWaitMode::Sleep : SoyWorkerWaitMode::Wake ),
	mName				( Name ),
	mParams				( Params )
{
	mWindow.reset(new Platform::TWindow(Name));

}

TOpenglWindow::~TOpenglWindow()
{
}
	
bool TOpenglWindow::IsValid()
{
	throw Soy::AssertException("todo");
}

bool TOpenglWindow::Iteration()
{
	return true;
}

std::shared_ptr<Opengl::TContext> TOpenglWindow::GetContext()
{
	throw Soy::AssertException("todo");
}

std::chrono::milliseconds TOpenglWindow::GetSleepDuration()
{
	return std::chrono::milliseconds( 1000/mParams.mRefreshRate );
}

Soy::Rectx<int32_t> TOpenglWindow::GetScreenRect()
{
	throw Soy::AssertException("todo");
	//	this must be called on the main thread, so we use the cache from the render target
	//return mView->GetScreenRect();
	//return mView->mRenderTarget.GetSize();
}

