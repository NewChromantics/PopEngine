#include "SoyGui.h"
#include <stdlib.h>
#include "SoyWindowLinux.h"
#include <magic_enum.hpp>

#include "EglContext.h"

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>

namespace Egl
{
	void		IsOkay(const char* Context);
	std::string	GetString(EGLint);

	/*
	enum Error_t : uint16_t
	{
		#define DECLARE_ERROR(e)	e_##e = e
		DECLARE_ERROR(EGL_BAD_ACCESS),
		DECLARE_ERROR(EGL_BAD_ALLOC),
		DECLARE_ERROR(EGL_BAD_ATTRIBUTE),
		DECLARE_ERROR(EGL_BAD_CONTEXT),
		DECLARE_ERROR(EGL_BAD_CURRENT_SURFACE),
		DECLARE_ERROR(EGL_BAD_MATCH),
		DECLARE_ERROR(EGL_BAD_NATIVE_PIXMAP),
		DECLARE_ERROR(EGL_BAD_NATIVE_WINDOW),
		DECLARE_ERROR(EGL_BAD_PARAMETER),
		DECLARE_ERROR(EGL_BAD_SURFACE),
		DECLARE_ERROR(EGL_NONE),
		DECLARE_ERROR(EGL_NON_CONFORMANT_CONFIG),
		DECLARE_ERROR(EGL_NOT_INITIALIZED),
		#undef DECLARE_ERROR
	};
	*/
}
/*
namespace magic_enum::customize {
template <>
struct enum_range<Egl::Error_t> {
    static constexpr int min = 0;
    static constexpr int max = 64;
};
} // namespace magic_enum
*/

std::string Egl::GetString(EGLint Egl_Value)
{
	switch(Egl_Value)
	{
	#define DECLARE_ERROR(e)	case e: return #e
		DECLARE_ERROR(EGL_BAD_DISPLAY);
		DECLARE_ERROR(EGL_BAD_ACCESS);
		DECLARE_ERROR(EGL_BAD_ALLOC);
		DECLARE_ERROR(EGL_BAD_ATTRIBUTE);
		DECLARE_ERROR(EGL_BAD_CONTEXT);
		DECLARE_ERROR(EGL_BAD_CURRENT_SURFACE);
		DECLARE_ERROR(EGL_BAD_MATCH);
		DECLARE_ERROR(EGL_BAD_NATIVE_PIXMAP);
		DECLARE_ERROR(EGL_BAD_NATIVE_WINDOW);
		DECLARE_ERROR(EGL_BAD_PARAMETER);
		DECLARE_ERROR(EGL_BAD_SURFACE);
		DECLARE_ERROR(EGL_NONE);
		DECLARE_ERROR(EGL_NON_CONFORMANT_CONFIG);
		DECLARE_ERROR(EGL_NOT_INITIALIZED);
		#undef DECLARE_ERROR
	};

	std::stringstream String;
	String << "<EGL_ 0x" << std::hex << Egl_Value << std::dec << ">";
	return String.str();
}

void Egl::IsOkay(const char* Context)
{
	auto Error = eglGetError();
	if ( Error == EGL_SUCCESS )
		return;

	//auto EglError = magic_enum::enum_name(static_cast<Error_t>(Error));
	auto EglError = GetString(Error);

	std::stringstream Debug;
	Debug << "EGL error " << EglError << " in " << Context;
	throw Soy::AssertException(Debug);
}


EglRenderView::EglRenderView(SoyWindow& Parent) :
	mWindow	( dynamic_cast<EglWindow&>(Parent) )
{
}

void GetConfigAttributes(ArrayBridge<EGLint>&& Attribs,Soy::Rectx<int32_t>& Rect,const TOpenglParams& OpenglParams)
{
	//	todo: use OpenglParams
	//	EGL_RENDER_BUFFER = EGL_SINGLE_BUFFER, EGL_BACK_BUFFER
	auto DepthSize = 24;
	auto AntialiasSamples = 1;	//	MSAA... passes? sample size?

	//Attribs.PushBackArray({EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT});
	
	if ( Rect.w > 0 )
		Attribs.PushBackArray({EGL_WIDTH,Rect.w});

	if ( Rect.h > 0 )
		Attribs.PushBackArray({EGL_HEIGHT,Rect.h});

//	if ( DepthBits )
//		Attribs.PushBackArray({EGL_DEPTH_SIZE,DepthSize});

//	if ( AntialiasSamples )
//		Attribs.PushBackArray({EGL_SAMPLES,AntialiasSamples});


/*
 cfgAttrs[cfgAttrIndex++] = (glversion == 2) ? EGL_OPENGL_ES2_BIT
                                                : EGL_OPENGL_ES_BIT;
    ctxAttrs[ctxAttrIndex++] = EGL_CONTEXT_CLIENT_VERSION;
    ctxAttrs[ctxAttrIndex++] = glversion;
// Request a minimum of 1 bit each for red, green, blue, and alpha
    // Setting these to anything other than DONT_CARE causes the returned
    //   configs to be sorted with the largest bit counts first.
    cfgAttrs[cfgAttrIndex++] = EGL_RED_SIZE;
    cfgAttrs[cfgAttrIndex++] = 1;
    cfgAttrs[cfgAttrIndex++] = EGL_GREEN_SIZE;
    cfgAttrs[cfgAttrIndex++] = 1;
    cfgAttrs[cfgAttrIndex++] = EGL_BLUE_SIZE;
    cfgAttrs[cfgAttrIndex++] = 1;
    cfgAttrs[cfgAttrIndex++] = EGL_ALPHA_SIZE;
    cfgAttrs[cfgAttrIndex++] = 1;
	*/

	Attribs.PushBack(EGL_NONE);
}

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

EglWindow::EglWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect )
{
	mContext.reset( new EglContext() );

	//	test, assuming context is still current etc
	int Iterations = 60 * 1;
    for ( int i=0;  i<Iterations; i++ )
    {
        Context.PrePaint();

        float Time = (float)i / (float)Iterations;
        glClearColor(Time,1.0f-Time,0,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glFinish();
       
        Context.PostPaint();
    }

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

EglWindow::~EglWindow()
{
	try
	{
		/*
		if ( mContext )
		{
			eglDestroyContext( mDisplay, mContext);
	        Egl::IsOkay("eglDestroyContext");
			mContext = EGL_NO_CONTEXT;
		}

		if ( mDisplay )
		{
			eglTerminate(mDisplay);
			Egl::IsOkay("eglTerminate");
			mDisplay = EGL_NO_DISPLAY;
		}
		*/
	}
	catch(std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " exception: " << e.what() << std::endl;
	}
}

Soy::Rectx<int32_t> EglWindow::GetScreenRect()
{
	int Width,Height;
	mContext->GetDisplaySize(Width,Height);
	return Soy::Rectx<int32_t>(0,0,Width,Height);
	
}

Platform::TWindow::TWindow(const std::string& Name, Soy::Rectx<int32_t>& Rect)
{
#if defined(ENABLE_OPENGL)
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
#else
	Soy_AssertTodo();
#endif
}

std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect,bool Resizable)
{
	std::shared_ptr<SoyWindow> Window;

	Window.reset( new EglWindow( Name, Rect ) );

	return Window;
}

std::shared_ptr<Gui::TRenderView> Platform::GetRenderView(SoyWindow& ParentWindow, const std::string& Name)
{
	return std::shared_ptr<Gui::TRenderView>( new EglRenderView(ParentWindow) );
	Soy_AssertTodo();
}

Soy::Rectx<int32_t> Platform::TWindow::GetScreenRect()
{
#if defined(ENABLE_OPENGL)
	return Soy::Rectx<size_t>( 0, 0, mESContext.screenWidth, mESContext.screenHeight );
#else
	Soy_AssertTodo();
#endif
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
