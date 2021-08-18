#include "SoyGui.h"
#include <stdlib.h>
#include "SoyWindowLinux.h"
#include <magic_enum.hpp>

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
	Debug << "EGL error " << EglError;
	throw Soy::AssertException(Debug);
}


EglRenderView::EglRenderView(SoyWindow& Parent) :
	mWindow	( dynamic_cast<EglWindow&>(Parent) )
{
}

EglWindow::EglWindow(const std::string& Name, Soy::Rectx<int32_t>& Rect )
{
	mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	Egl::IsOkay("eglGetDisplay");

	eglInitialize(mDisplay, nullptr, nullptr);
	Egl::IsOkay("eglInitialize");

	//	enum configs
	EGLint ConfigCount = 0;
	eglChooseConfig(mDisplay, nullptr, &mConfig, 1, &ConfigCount);
	Egl::IsOkay("eglChooseConfig");

	eglBindAPI(EGL_OPENGL_ES_API);
	Egl::IsOkay("eglBindAPI");

	mContext = eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, NULL);
	Egl::IsOkay("eglCreateContext");

	mSurface = eglCreatePbufferSurface(mDisplay, mConfig, nullptr);
	Egl::IsOkay("eglCreatePbufferSurface");

	eglMakeCurrent( mDisplay, mSurface, mSurface, mContext);
	Egl::IsOkay("eglMakeCurrent");
}

EglWindow::~EglWindow()
{
	try
	{
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
	}
	catch(std::exception& e)
	{
		std::Debug << __PRETTY_FUNCTION__ << " exception: " << e.what() << std::endl;
	}
}

Soy::Rectx<int32_t> EglWindow::GetScreenRect()
{
	//	gr: this query is mostly used for GL size, but surface
	//		could be different size to display?
	//		but I can't see how to get display size (config?)
	//	may need render/surface vs display rect, but for now, its the surface
	if ( !mSurface )
		throw Soy::AssertException("EglWindow::GetRenderRec no surface");
	if ( !mDisplay )
		throw Soy::AssertException("EglWindow::GetRenderRec no display");

	EGLint Width = 0;	
	EGLint Height = 0;
	eglQuerySurface( mDisplay, mSurface, EGL_WIDTH, &Width );
	Egl::IsOkay("eglQuerySurface(EGL_WIDTH)");
	eglQuerySurface( mDisplay, mSurface, EGL_HEIGHT, &Height );
	Egl::IsOkay("eglQuerySurface(EGL_HEIGHT)");

	//	gr: definitely no offset? no query option for surfaces
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
