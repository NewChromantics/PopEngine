#include "SoyOpengl.h"
#include "SoyOpenglWindow.h"
//#include "SoyOpenglView.h"
//#include "PopMain.h"
#include "SoyMath.h"

#include "SoyGui_Win32.h"
#include "SoyWin32.h"
#include "SoyWindow.h"
#include "SoyThread.h"

#include "SoyOpenglContext_Win32.h"

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



	LRESULT CALLBACK	Win32CallBack(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	void		Loop(bool Blocking,std::function<void()> OnQuit);
	void		Loop(std::function<bool()> CanBlock,std::function<void()> OnQuit);	

	template<typename COORDTYPE>
	Soy::Rectx<COORDTYPE>	GetRect(RECT Rect);
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



