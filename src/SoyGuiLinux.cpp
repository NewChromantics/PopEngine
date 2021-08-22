#include "SoyGui.h"
#include <stdlib.h>
#include "SoyGuiLinux.h"
#include <magic_enum.hpp>
#include <thread>


int OnXError(Display* Display,XErrorEvent* Event)
{
	std::string ErrorString;

	if ( !Event )
	{
		ErrorString = "<null error event>";
	}
	else
	{
		char ErrorBuffer[400] = {0};
		auto Return = XGetErrorText(Display, Event->error_code, ErrorBuffer, std::size(ErrorBuffer) );
		ErrorString = ErrorBuffer;
	}
	std::Debug << "X error " << ErrorString << std::endl;

	//	docs say return is ignored
	//	https://linux.die.net/man/3/xseterrorhandler
	return 0;
}

WindowX11::WindowX11( const std::string& Name, Soy::Rectx<int32_t>& Rect )
{
	//	https://stackoverflow.com/a/64449940/355753
	//	open default screen
	//	nullptr opens whatever DISPLAY env var is...
	//	fails if that isn't set... so we could just display straight to :0
	//const char* DisplayName = nullptr;
	const char* DisplayName = ":0";
	mDisplay = XOpenDisplay(DisplayName);
	if ( !mDisplay )
		throw std::runtime_error("Failed to open X display");
	
	auto OldErrorHandler = XSetErrorHandler(OnXError);

	if ( !XInitThreads() )
		throw std::runtime_error("Failed to init X threads");

	auto RootWindow = DefaultRootWindow(mDisplay);
	if ( !RootWindow )
		throw std::runtime_error("Failed to get root window of X display");

	XSetWindowAttributes WindowAttributes = {0};
	WindowAttributes.event_mask = 
    StructureNotifyMask |
    ExposureMask        |
    PointerMotionMask   |
    KeyPressMask        |
    KeyReleaseMask      |
    ButtonPressMask     |
    ButtonReleaseMask;

	int BorderWidth = 0;
	int Depth = 0;
	int Class = 0;
	Visual* visual = nullptr;
	unsigned long ValueMask = 0;
	Rect.w = 640;
	Rect.h = 480;
	auto Screen = DefaultScreen(mDisplay);
	mWindow = XCreateSimpleWindow( mDisplay, RootWindow, Rect.x, Rect.y, Rect.w, Rect.h, BorderWidth, BlackPixel(mDisplay, Screen), WhitePixel(mDisplay, Screen));
	//mWindow = XCreateWindow( mDisplay, RootWindow, Rect.x, Rect.y, Rect.w, Rect.h, BorderWidth, Depth, Class, visual, ValueMask, &WindowAttributes );
	if ( !mWindow )
		throw std::runtime_error("Failed to create X window");

	auto Result = XSelectInput( mDisplay, mWindow, WindowAttributes.event_mask );

	auto wm_destroy_window = XInternAtom( mDisplay,"WM_DELETE_WINDOW", false);

	//	set window name
    Result = XStoreName( mDisplay, mWindow, "MyWindow" );
	std::Debug << "XStoreName returned " << Result << std::endl;

	//	show window
	Result = XMapWindow( mDisplay, mWindow );
	std::Debug << "XMapWindow returned " << Result << std::endl;

	//	start the event thread
	auto EventThread = [this]()
	{
		return this->EventThreadIteration();
	};
	mEventThread.reset( new SoyThreadLambda("X11 event thread",EventThread));
}

bool WindowX11::EventThreadIteration()
{
	//	this makes window appear!
	//	I think because it does an initial flush
	//XPending(mDisplay);

	//	no pending events
	//	should we call XNextEvent() to block?
	if ( !XPending(mDisplay) )
	{
		//std::Debug << "No pending events, sleeping" << std::endl;
		std::this_thread::sleep_for( std::chrono::milliseconds(500) );
		return true;
	}

	XEvent Event;
	XNextEvent( mDisplay, &Event );
	std::Debug << "Got event " << Event.type << std::endl;
	
	if ( Event.type == ConfigureNotify )
	{
		auto& NewConfiguration = Event.xconfigure;
		Soy::Rectx<int> Rect( NewConfiguration.x, NewConfiguration.y, NewConfiguration.width, NewConfiguration.height );

		std::Debug << "Window recognfigured " << Rect << std::endl;
	}

	if ( Event.type == ClientMessage )
	{
		std::Debug << "Window ClientMessage" << std::endl;
	/*
		if (event.xclient.message_type ==
		XInternAtom(demoState.platform->XDisplay,"WM_PROTOCOLS", True) &&
		(Atom)event.xclient.data.l[0] == XInternAtom(demoState.platform->XDisplay,"WM_DELETE_WINDOW", True))
		close()
		*/
	}

	auto SendMouseEvent = [&](SoyMouseEvent::Type EventType,SoyMouseButton::Type Button) 
	{
		Window RootWindow,ChildWindow;
		vec2x<int32_t> RootPos;
		vec2x<int32_t> WindowPos;
		unsigned int ModifierKeysAndPointerButtons = 0;
		if ( !XQueryPointer( mDisplay, mWindow, &RootWindow, &ChildWindow, &RootPos.x, &RootPos.y, &WindowPos.x, &WindowPos.y, &ModifierKeysAndPointerButtons ) )
		{
			//	the pointer is not on the same screen as the specified window
			std::Debug << "Pointer not on screen" << std::endl;
			return;	
		}
		//std::Debug << "QueryPointer buttons/modifers " << ModifierKeysAndPointerButtons << std::endl;

		if ( !mOnMouseEvent )
			return;

		Gui::TMouseEvent MouseEvent;
		MouseEvent.mEvent = EventType;
		MouseEvent.mPosition = WindowPos;
		MouseEvent.mButton = Button;
		mOnMouseEvent( MouseEvent );
	};


	auto ButtonToSoyButton = [](int Button)
	{
		if ( Button == 1 )	return SoyMouseButton::Left;
		if ( Button == 2 )	return SoyMouseButton::Middle;
		if ( Button == 3 )	return SoyMouseButton::Right;
		if ( Button == 4 )	return SoyMouseButton::Back;
		if ( Button == 5 )	return SoyMouseButton::Forward;
		std::Debug << "Unhandled button" << Button << std::endl;
		return SoyMouseButton::None;
	};

	if ( Event.type == MotionNotify )
	{
		//	send for each button that's down
		auto ButtonState = Event.xmotion.state;
		BufferArray<SoyMouseButton::Type,5> Buttons;
		if ( ButtonState & Button1Mask )	Buttons.PushBack(ButtonToSoyButton(1));
		if ( ButtonState & Button2Mask )	Buttons.PushBack(ButtonToSoyButton(2));
		if ( ButtonState & Button3Mask )	Buttons.PushBack(ButtonToSoyButton(3));
		if ( ButtonState & Button4Mask )	Buttons.PushBack(ButtonToSoyButton(4));
		if ( ButtonState & Button5Mask )	Buttons.PushBack(ButtonToSoyButton(5));

		//	no buttons down, so just normal move
		if ( Buttons.IsEmpty() )
			Buttons.PushBack( SoyMouseButton::None );

		for ( int i=0;	i<Buttons.GetSize();	i++ )
			SendMouseEvent( SoyMouseEvent::Move, Buttons[i] );
	}


	if ( Event.type == ButtonPress )
	{
		auto Button = ButtonToSoyButton(Event.xbutton.button);
		if ( Button != SoyMouseButton::None )
			SendMouseEvent( SoyMouseEvent::Down, Button );
	}

	if ( Event.type == ButtonRelease )
	{
		auto Button = ButtonToSoyButton(Event.xbutton.button);
		if ( Button != SoyMouseButton::None )
			SendMouseEvent( SoyMouseEvent::Up, Button );
	}

	/*      case KeyPress:
            case KeyRelease:
                if (!keyCB) break;
                XLookupString(&event.xkey, str, 2, &key, NULL);
                if (!str[0] || str[1]) break; // Only care about basic keys
                keyCB(str[0], (event.type == KeyPress) ? 1 : 0);
                break;
*/
	return true;
}

WindowX11::~WindowX11()
{
	mEventThread.reset();

	if ( mWindow )
	{
		XDestroyWindow( mDisplay, mWindow );
		mWindow = 0;
	}

	if ( mDisplay )
	{
		XCloseDisplay( mDisplay );
		mDisplay = nullptr;
	}
}




#if defined(ENABLE_EGL)
EglRenderView::EglRenderView(SoyWindow& Parent) :
	mWindow	( dynamic_cast<EglWindow&>(Parent) )
{
	/*
	auto NativeDisplays = GetNativeDisplays();
	std::Debug << "Got " << NativeDisplays.GetSize() << " native displays" << std::endl;
	NativeDisplays.PushBack(EGL_DEFAULT_DISPLAY);

	auto GetPlatformDisplay = GetEglFunction<decltype(eglGetPlatformDisplayEXT)>("eglGetPlatformDisplayEXT");

	for ( int d=0;	d<NativeDisplays.GetSize() && !mDisplay;	d++ )
	{
		auto Display = NativeDisplays[d];
		//	todo: try{} all the devices
		if ( GetPlatformDisplay )
		{
			EGLint* Attribs = nullptr;
			auto Platform = EGL_PLATFORM_DEVICE_EXT;
			mDisplay = GetPlatformDisplay( Platform, Display, Attribs );
			Egl::IsOkay("GetPlatformDisplay");
		}
		else
		{
			mDisplay = eglGetDisplay(Display);
			Egl::IsOkay("eglGetDisplay");
		}
	}
	*/

	//	gr: using the x11 display fails (same in forums & nvidia demo)
	//auto DisplayType = mWindow.GetDisplay();
	auto DisplayType = EGL_DEFAULT_DISPLAY;
	mDisplay = eglGetDisplay(DisplayType);
	Egl::IsOkay("eglGetDisplay");
	if ( !mDisplay )
		throw Soy::AssertException("Failed to get a display");

	if ( !eglInitialize(mDisplay, nullptr, nullptr) )
		std::Debug << "eglInitialize() returned false" << std::endl;
	Egl::IsOkay("eglInitialize");


	//	gr: some things have binding first
	if ( !eglBindAPI(EGL_OPENGL_ES_API) )
		std::Debug << "eglBindAPI() returned false" << std::endl;
	Egl::IsOkay("eglBindAPI");

	//	attrib[pair]s to filter configs
	EGLint* pConfigAttribs = nullptr;
/*
	EGLint pConfigAttribs[] = 
	{
		EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,
		EGL_NATIVE_RENDERABLE,EGL_TRUE,
		EGL_NONE
	};
	*/
/*
	//	don't have this until we start creating renderview. Maybe window should make a display
	//	and renderview actually does the initialisation, OR just do it all at render context time?
	TOpenglParams OpenglParams;	
	Array<EGLint> ConfigAttribs;
	//GetConfigAttributes( GetArrayBridge(ConfigAttribs), Rect, OpenglParams );
	EGLint* pConfigAttribs = ConfigAttribs.GetArray();
*/
	//	enum configs
	EGLint ConfigCount = -1;
	{
		EGLConfig Stub;
		if ( !eglChooseConfig(mDisplay, pConfigAttribs, &Stub, 1, &ConfigCount) )
			std::Debug << "eglChooseConfig returned false" << std::endl;
	}
	Egl::IsOkay("eglChooseConfig (get count)");
	std::Debug << "Found " << ConfigCount << " display configs" << std::endl;

	Array<EGLConfig> Configs;
	Configs.SetSize(ConfigCount);
	Configs.SetAll(nullptr);
	eglChooseConfig(mDisplay, pConfigAttribs, Configs.GetArray(), Configs.GetSize(), &ConfigCount);
	Egl::IsOkay("eglChooseConfig");
	Configs.SetSize(ConfigCount);
	if ( Configs.IsEmpty() )
		throw Soy::AssertException("No display configs availible");
	for ( auto i=0;	i<Configs.GetSize();	i++ )
	{
		EGLint Width=0,Height=0;
		auto Config = Configs[i];
		eglGetConfigAttrib( mDisplay, Config, EGL_MAX_PBUFFER_WIDTH, &Width );
		eglGetConfigAttrib( mDisplay, Config, EGL_MAX_PBUFFER_HEIGHT, &Height );
		std::Debug << "Display config #" << i << " " << Width << "x" << Height << std::endl;
	}
	auto Config = Configs[0];



	mContext = eglCreateContext(mDisplay, Config, EGL_NO_CONTEXT, nullptr );
	Egl::IsOkay("eglCreateContext");

	auto Window = mWindow.GetWindow();
	mSurface = eglCreateWindowSurface( mDisplay, Config, Window, nullptr  );
	Egl::IsOkay("eglCreateWindowSurface");
	if ( !mSurface )
		throw Soy::AssertException("Failed to create surface");

	//mSurface = eglCreatePbufferSurface(mDisplay, Config, nullptr);
	//Egl::IsOkay("eglCreatePbufferSurface");

	//eglMakeCurrent( mDisplay, mSurface, mSurface, mContext);
	//Egl::IsOkay("eglMakeCurrent");
}
#endif

Soy::Rectx<size_t> EglRenderView::GetSurfaceRect()
{
	eglMakeCurrent( mDisplay, mSurface, mSurface, mContext);
	Egl::IsOkay("eglMakeCurrent");

	EGLint w=0,h=0;
	eglQuerySurface( mDisplay, mSurface, EGL_WIDTH, &w );
	Egl::IsOkay("eglQuerySurface(EGL_WIDTH)");
	eglQuerySurface( mDisplay, mSurface, EGL_HEIGHT, &h );
	Egl::IsOkay("eglQuerySurface(EGL_HEIGHT)");

	eglMakeCurrent( mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	Egl::IsOkay("eglMakeCurrent");

	return Soy::Rectx<size_t>( 0,0, w, h);
}

void EglRenderView::RequestPaint()
{
	//	do we need to tell x11 window to repaint?
	auto Rect = GetSurfaceRect();
	if ( mOnDraw )
		mOnDraw( Rect );
}

void EglRenderView::PrePaint()
{
	auto* CurrentContext = eglGetCurrentContext();
	if ( CurrentContext != mContext )
	{
		//  this will error if current locked to some other thread
		//NvGlDemoLog("Switching context...");
		auto Result = eglMakeCurrent( mDisplay, mSurface, mSurface, mContext );
		if ( Result != EGL_TRUE )
		{
			Egl::IsOkay("eglMakeCurrent returned false");
		}
	}
}

void EglRenderView::PostPaint()
{
	if (eglSwapBuffers( mDisplay, mSurface) != EGL_TRUE) 
		throw std::runtime_error("eglSwapBuffers failed");

	//  unbind context
	auto Result = eglMakeCurrent( mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	Egl::IsOkay("eglMakeCurrent unlock (NO_CONTEXT)");
}

/*
template<typename FUNCTIONTYPE>
std::function<FUNCTIONTYPE> GetEglFunction(const char* Name)
{
	std::function<FUNCTIONTYPE> Function;
	auto* FunctionPointer = eglGetProcAddress(Name);
	if ( !FunctionPointer )
	{
		std::Debug << "EGL function " << Name << " not found" << std::endl;
		return nullptr;
	}

	Function = reinterpret_cast<FUNCTIONTYPE*>(FunctionPointer);
	return Function;
}

Array<EGLNativeDisplayType> GetNativeDisplays()
{
	Array<EGLNativeDisplayType> Displays;

	auto QueryDevices = GetEglFunction<decltype(eglQueryDevicesEXT)>("eglQueryDevicesEXT");
	//	no such function
	if ( !QueryDevices )
		return Displays;

	EGLint DeviceCount = -1;
	auto Result = QueryDevices(0,nullptr,&DeviceCount);
	if ( !Result )
		std::Debug << "eglQueryDevicesEXT (count) returned false";
	Egl::IsOkay("eglQueryDevicesEXT (count)");
	std::Debug << "eglQueryDevicesEXT(count) returned " << DeviceCount << " devices" << std::endl;

	Array<EGLDeviceEXT> Devices;
	Devices.SetSize(DeviceCount);
	Devices.SetAll(nullptr);
	Result = QueryDevices(Devices.GetSize(),Devices.GetArray(),&DeviceCount);
	if ( !Result )
		std::Debug << "eglQueryDevicesEXT returned false";
	
	for ( int i=0;	i<Devices.GetSize();	i++ )
	{
		//	nvidia code just casts this
		EGLNativeDisplayType Display = (EGLNativeDisplayType)Devices[i];
		Displays.PushBack(Display);
	}
	return Displays;
}
*/

#if defined(ENABLE_DRMWINDOW)
WindowDrm::WindowDrm(const std::string& Name,Soy::Rectx<int32_t>& Rect )
{
	Egl::TParams Params;
	Params.WindowOffsetX = Rect.x;
	Params.WindowOffsetY = Rect.y;
	Params.WindowWidth = Rect.w;
	Params.WindowHeight = Rect.h;

	//	EGL system currently requires a res
	if ( !Params.WindowWidth )
		Params.WindowWidth = 640;
	if ( !Params.WindowHeight )
		Params.WindowHeight = 480;

	Params.DisplayWidth = Params.WindowWidth;
	Params.DisplayHeight = Params.WindowHeight;
	mContext.reset( new Egl::TDisplaySurfaceContext(Params) );

	//	this post says it doesnt need X, but I cant get configs without X11 starting
	//	https://forums.developer.nvidia.com/t/egl-without-x11/58733/4
	/*
	auto NativeDisplays = GetNativeDisplays();
	std::Debug << "Got " << NativeDisplays.GetSize() << " native displays" << std::endl;
	NativeDisplays.PushBack(EGL_DEFAULT_DISPLAY);

	auto GetPlatformDisplay = GetEglFunction<decltype(eglGetPlatformDisplayEXT)>("eglGetPlatformDisplayEXT");

	for ( int d=0;	d<NativeDisplays.GetSize() && !mDisplay;	d++ )
	{
		auto Display = NativeDisplays[d];
		//	todo: try{} all the devices
		if ( GetPlatformDisplay )
		{
			EGLint* Attribs = nullptr;
			auto Platform = EGL_PLATFORM_DEVICE_EXT;
			mDisplay = GetPlatformDisplay( Platform, Display, Attribs );
			Egl::IsOkay("GetPlatformDisplay");
		}
		else
		{
			mDisplay = eglGetDisplay(Display);
			Egl::IsOkay("eglGetDisplay");
		}
	}
	*/
/*
	//	default simple version
	mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	Egl::IsOkay("eglGetDisplay");
	if ( !mDisplay )
		throw Soy::AssertException("Failed to get a display");



	if ( !eglInitialize(mDisplay, nullptr, nullptr) )
		std::Debug << "eglInitialize() returned false" << std::endl;
	Egl::IsOkay("eglInitialize");


	//	gr: some things have binding first
	if ( !eglBindAPI(EGL_OPENGL_ES_API) )
		std::Debug << "eglBindAPI() returned false" << std::endl;
	Egl::IsOkay("eglBindAPI");



	//	attrib[pair]s to filter configs
	//EGLint* pConfigAttribs = nullptr;

	EGLint pConfigAttribs[] = {
    EGL_RENDERABLE_TYPE
    ,EGL_OPENGL_BIT
   // ,EGL_DEPTH_SIZE 
   // ,24
    ,EGL_NATIVE_RENDERABLE
    ,EGL_TRUE
    ,EGL_NONE
    };


	//	don't have this until we start creating renderview. Maybe window should make a display
	//	and renderview actually does the initialisation, OR just do it all at render context time?
	TOpenglParams OpenglParams;	
	Array<EGLint> ConfigAttribs;
	GetConfigAttributes( GetArrayBridge(ConfigAttribs), Rect, OpenglParams );
	EGLint* pConfigAttribs = ConfigAttribs.GetArray();

	//	enum configs
	EGLint ConfigCount = -1;
	{
		EGLConfig Stub;
		if ( !eglChooseConfig(mDisplay, pConfigAttribs, &Stub, 1, &ConfigCount) )
			std::Debug << "eglChooseConfig returned false" << std::endl;
	}
	Egl::IsOkay("eglChooseConfig (get count)");
	std::Debug << "Found " << ConfigCount << " display configs" << std::endl;

	Array<EGLConfig> Configs;
	Configs.SetSize(ConfigCount);
	Configs.SetAll(nullptr);
	eglChooseConfig(mDisplay, pConfigAttribs, Configs.GetArray(), Configs.GetSize(), &ConfigCount);
	Egl::IsOkay("eglChooseConfig");
	Configs.SetSize(ConfigCount);
	if ( Configs.IsEmpty() )
		throw Soy::AssertException("No display configs availible");
	for ( auto i=0;	i<Configs.GetSize();	i++ )
	{
		EGLint Width=0,Height=0;
		auto Config = Configs[i];
		eglGetConfigAttrib( mDisplay, Config, EGL_MAX_PBUFFER_WIDTH, &Width );
		eglGetConfigAttrib( mDisplay, Config, EGL_MAX_PBUFFER_HEIGHT, &Height );
		std::Debug << "Display config #" << i << " " << Width << "x" << Height << std::endl;
	}
	auto Config = Configs[0];



	mContext = eglCreateContext(mDisplay, Config, EGL_NO_CONTEXT, NULL);
	Egl::IsOkay("eglCreateContext");

	mSurface = eglCreatePbufferSurface(mDisplay, Config, nullptr);
	Egl::IsOkay("eglCreatePbufferSurface");

	eglMakeCurrent( mDisplay, mSurface, mSurface, mContext);
	Egl::IsOkay("eglMakeCurrent");
	*/
}
#endif



#if defined(ENABLE_EGL)
Soy::Rectx<int32_t> EglWindow::GetScreenRect()
{
	auto mSurface = GetSurface();
    auto mDisplay = GetDisplay();

    //	gr: this query is mostly used for GL size, but surface
	//		could be different size to display?
	//		but I can't see how to get display size (config?)
	//	may need render/surface vs display rect, but for now, its the surface
	if ( !mSurface )
		throw std::runtime_error("EglWindow::GetRenderRec no surface");
	if ( !mDisplay )
		throw std::runtime_error("EglWindow::GetRenderRec no display");

	EGLint w=0,h=0;
	eglQuerySurface( mDisplay, mSurface, EGL_WIDTH, &w );
	Egl::IsOkay("eglQuerySurface(EGL_WIDTH)");
	eglQuerySurface( mDisplay, mSurface, EGL_HEIGHT, &h );
	Egl::IsOkay("eglQuerySurface(EGL_HEIGHT)");

	return Soy::Rectx<int32_t>(0,0,w,h);
}
#endif

/*
Platform::TWindow::TWindow(const std::string& Name, Soy::Rectx<int32_t>& Rect)
{
old egl
	esInitContext( &mESContext );
	if(Name == "null")
	{
	// tsdk: the width and height are set to the 640 x 480 inside this function if the values are empty
		esCreateHeadless( &mESContext, Name.c_str(), Rect.w, Rect.h);
	}
	else
	{
	// tsdk: the width and height are set to the size of the screen inside this function if the values are empty
		esCreateWindow( &mESContext, Name.c_str(), Rect.w, Rect.h, ES_WINDOW_ALPHA );
	}
	
	Soy_AssertTodo();
}*/

std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect,bool Resizable)
{
	std::shared_ptr<SoyWindow> Window;

	//	todo: need to work out if we're running X11 or not
	try
	{
		Window.reset( new WindowX11(Name,Rect) );
		return Window;
	}
	catch(std::exception& e)
	{
		std::Debug << "Failed to create X11 window; " << e.what() << std::endl;
		throw;
	}

#if defined(ENABLE_DRMWINDOW)
	//	try and create DRM window if xserver isnt running
	Window.reset( new WindowDrm(Name,Rect) );
#endif

	return Window;
}

std::shared_ptr<Gui::TRenderView> Platform::GetRenderView(SoyWindow& ParentWindow, const std::string& Name)
{
#if defined(ENABLE_EGL)
	return std::shared_ptr<Gui::TRenderView>( new EglRenderView(ParentWindow) );
#endif
	Soy_AssertTodo();
}



std::shared_ptr<SoySlider> Platform::CreateSlider(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyColourButton> Platform::CreateColourButton(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<Gui::TImageMap> Platform::CreateImageMap(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<Gui::TColourPicker>	Platform::CreateColourPicker(vec3x<uint8_t> InitialColour)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyTextBox> Platform::CreateTextBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyTextBox> Platform::GetTextBox(SoyWindow& Parent,const std::string& Name)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyTickBox> Platform::CreateTickBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyTickBox> Platform::GetTickBox(SoyWindow& Parent, const std::string& Name)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyLabel> Platform::CreateLabel(SoyWindow &Parent, Soy::Rectx<int32_t> &Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyLabel> Platform::GetLabel(SoyWindow& Parent,const std::string& Name)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyButton> Platform::CreateButton(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyButton> Platform::GetButton(SoyWindow& Parent,const std::string& Name)
{
	Soy_AssertTodo();
}

std::shared_ptr<Gui::TImageMap> Platform::GetImageMap(SoyWindow& Parent,const std::string& Name)
{
	Soy_AssertTodo();
}


std::shared_ptr<Gui::TList> Platform::GetList(SoyWindow& Parent, const std::string& Name)
{
	Soy_AssertTodo();
}
