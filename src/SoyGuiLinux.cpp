#include "SoyGui.h"
#include <stdlib.h>
#include "SoyGuiLinux.h"
#include <magic_enum.hpp>





#if defined(ENABLE_EGL)
EglRenderView::EglRenderView(SoyWindow& Parent) :
	mWindow	( dynamic_cast<EglWindow&>(Parent) )
{
}
#endif

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

#if defined(ENABLE_EGL)
EglWindow::EglWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect )
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
EglWindow::~EglWindow()
{
}
#endif


#if defined(ENABLE_EGL)
Soy::Rectx<int32_t> EglWindow::GetScreenRect()
{
	uint32_t Width=0,Height=0;
	mContext->GetDisplaySize(Width,Height);
	return Soy::Rectx<int32_t>(0,0,Width,Height);
}
#endif

Platform::TWindow::TWindow(const std::string& Name, Soy::Rectx<int32_t>& Rect)
{
/*old egl
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
	*/
	Soy_AssertTodo();
}

std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect,bool Resizable)
{
	std::shared_ptr<SoyWindow> Window;

#if defined(ENABLE_EGL)
	Window.reset( new EglWindow( Name, Rect ) );
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


Soy::Rectx<int32_t> Platform::TWindow::GetScreenRect()
{
	/*old egl
#if defined(ENABLE_OPENGL)
	return Soy::Rectx<size_t>( 0, 0, mESContext.screenWidth, mESContext.screenHeight );
#else*/
	Soy_AssertTodo();
}

void Platform::TWindow::SetFullscreen(bool Fullscreen)
{
	Soy_AssertTodo();
}

bool Platform::TWindow::IsFullscreen()
{
	return true;
}

bool Platform::TWindow::IsMinimised()
{
	Soy_AssertTodo();
}

bool Platform::TWindow::IsForeground()
{
	Soy_AssertTodo();
}

void Platform::TWindow::EnableScrollBars(bool Horz,bool Vert)
{
	//Soy_AssertTodo();
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
