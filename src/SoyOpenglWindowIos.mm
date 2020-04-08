#include "SoyOpenglWindow.h"


TOpenglWindow::TOpenglWindow(const std::string& Name,const Soy::Rectx<int32_t>& Rect,TOpenglParams Params) :
	SoyWorkerThread		( Soy::GetTypeName(*this), Params.mAutoRedraw ? SoyWorkerWaitMode::Sleep : SoyWorkerWaitMode::Wake ),
	mName				( Name ),
	mParams				( Params )
{
	Soy_AssertTodo();
}

TOpenglWindow::~TOpenglWindow()
{
}

bool TOpenglWindow::IsValid()
{
	Soy_AssertTodo();
}

bool TOpenglWindow::Iteration()
{
	Soy_AssertTodo();
}

std::shared_ptr<Opengl::TContext> TOpenglWindow::GetContext()
{
	Soy_AssertTodo();
}

void TOpenglWindow::OnClosed()
{
	Soy_AssertTodo();
}

std::chrono::milliseconds TOpenglWindow::GetSleepDuration()
{
	return std::chrono::milliseconds( 1000/mParams.mRefreshRate );
}


Soy::Rectx<int32_t> TOpenglWindow::GetScreenRect()
{
	Soy_AssertTodo();
}


void TOpenglWindow::EnableScrollBars(bool Horz,bool Vert)
{
	Soy_AssertTodo();
}



void TOpenglWindow::SetFullscreen(bool Fullscreen)
{
	Soy_AssertTodo();
}


bool TOpenglWindow::IsFullscreen()
{
	Soy_AssertTodo();
}

bool TOpenglWindow::IsMinimised()
{
	Soy_AssertTodo();
}

bool TOpenglWindow::IsForeground()
{
	Soy_AssertTodo();
}


