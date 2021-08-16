#include "SoyGui.h"
#include <stdlib.h>
#include "SoyWindowLinux.h"

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

	Window.reset( new Platform::TWindow( Name, Rect ) );

	return Window;
}

std::shared_ptr<Gui::TRenderView> Platform::GetRenderView(SoyWindow& ParentWindow, const std::string& Name)
{
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
