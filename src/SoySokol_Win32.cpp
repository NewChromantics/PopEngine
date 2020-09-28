#include <stdlib.h>
#include "TApiSokol.h"
#include "SoySokol_Win32.h"
#include "SoyGui_Win32.h"

#include "Win32OpenglContext.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol/sokol_gfx.h"

std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params)
{
	auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);
	auto Context = new SokolOpenglContext(Window,Params);
	return std::shared_ptr<Sokol::TContext>(Context);

}

//	gr: a lot of this is from/replaces TOpenglWindow
SokolOpenglContext::SokolOpenglContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params) :
	mParams	( Params )
{
	mWindow = std::dynamic_pointer_cast<Platform::TWindow>(Window);
	auto& PlatformWindow = *mWindow;

	mWindowThread.reset(new Platform::TWin32Thread("SokolOpenglContext"));

	Win32::TOpenglParams OpenglParams;

	//	need to create on the correct thread
	//	this can be called at the same time that JS is assigning events
	//	so don't assume they're non-null
	//	do they need a lock?
	auto CreateControls = [&]()
	{
		bool Resizable = true;
		//mWindow.reset(new Platform::TWindow(Name, Rect, *mWindowThread, Resizable));
		//mWindowContext.reset(new Platform::TOpenglContext(*mWindow, Params));

		auto OnRender = [this](Opengl::TRenderTarget& RenderTarget, std::function<void()> LockContext)
		{
			/*
			if (!mOnRender)
				return;
			mOnRender(RenderTarget, LockContext);
			*/
		};
		//mWindowContext->mOnRender = OnRender;

		//	gr: can I use std::bind?
		auto& Win = static_cast<Platform::TControl&>(*mWindow);
		Win.mOnMouseDown = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	if (this->mOnMouseDown) this->mOnMouseDown(Pos, Button); };
		Win.mOnMouseUp = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	if (mOnMouseUp) this->mOnMouseUp(Pos, Button); };
		Win.mOnMouseMove = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	if (mOnMouseMove) this->mOnMouseMove(Pos, Button); };
		Win.mOnKeyDown = [this](Platform::TControl& Control, SoyKeyButton::Type Key) {	if (mOnKeyDown)	this->mOnKeyDown(Key); };
		Win.mOnKeyUp = [this](Platform::TControl& Control, SoyKeyButton::Type Key) {	if (mOnKeyUp)		this->mOnKeyUp(Key); };
	};
	Soy::TSemaphore Wait;
	mWindowThread->PushJob(CreateControls, Wait);
	Wait.Wait("Win32 thread");
	auto OnFrame = [this](Platform::TControl&)
	{
		this->OnPaint(); 
	};
	PlatformWindow.mOnPaint = OnFrame;
}

SokolOpenglContext::~SokolOpenglContext()
{
	mRunning = false;
	try
	{
		//	this should block until done
		mPaintThread.reset();
	}
	catch(std::exception& e)
	{
		std::Debug << "Caught " << __PRETTY_FUNCTION__ << " exception " << e.what() << std::endl;
	}
}

void SokolOpenglContext::RunGpuJobs()
{
	mGpuJobsLock.lock();
	auto Jobs = mGpuJobs;
	mGpuJobs.Clear(true);
	mGpuJobsLock.unlock();

	for ( auto j=0;	j<Jobs.GetSize();	j++ )
	{
		auto& Job = Jobs[j];
		try
		{
			Job(mSokolContext);
		}
		catch (std::exception& e)
		{
			std::Debug << "Error executing job; " << e.what() << std::endl;
		}
	}
}

void SokolOpenglContext::OnPaint()
{
	std::lock_guard Lock(mOpenglContextLock);
	mOpenglContext->Lock();

	if ( mSokolContext.id == 0 )
	{
		sg_desc desc={0};
		sg_setup(&desc);
		mSokolContext = sg_setup_context();
	}
	
	RunGpuJobs();

	auto Width = 100;
	auto Height = 100;
		
	vec2x<size_t> Size( Width, Height );
	mParams.mOnPaint( mSokolContext, Size );
}

void SokolOpenglContext::DoPaint()
{
	mWindow->Repaint();
}

void SokolOpenglContext::Queue(std::function<void(sg_context)> Exec)
{
	std::lock_guard<std::mutex> Lock(mGpuJobsLock);
	mGpuJobs.PushBack(Exec);
}