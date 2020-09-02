#include "SoyGui.h"
#include "esUtil.h"

class Platform::TWindow : public SoyWindow
{
public:
	TWindow( const std::string& Name );
	
	virtual Soy::Rectx<int32_t>		GetScreenRect() override;

	ESContext											mESContext;

	void													Render(ESContext mESContext, auto Frame);
};

Platform::TWindow::TWindow(const std::string& Name)
{
	esInitContext( &mESContext );

	esCreateWindow( &mESContext, Name, 320, 240, ES_WINDOW_ALPHA );
}

std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect,bool Resizable)
{
	std::shared_ptr<SoyWindow> Window;

	Window.reset( new Platform::TWindow(Name) );

	return Window;
}

Soy::Rectx<int32_t> Platform::TWindow::GetScreenRect()
{
	Soy_AssertTodo();
}

void Platform::TWindow::Render( auto Frame )
{
	esRegisterDrawFunc(&mESContext, Frame);

	esMainLoop(&mESContext);
}