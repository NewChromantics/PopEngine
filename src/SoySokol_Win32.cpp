#include <stdlib.h>
#include "TApiSokol.h"
#include "SoySokol_Win32.h"
#include "SoyGui_Win32.h"

#include "SoyOpenglContext_Win32.h"
#define SOKOL_EXTERNAL_GL_LOADER
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol/sokol_gfx.h"

std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(Sokol::TContextParams Params)
{
	auto Context = new SokolOpenglContext(Params);
	return std::shared_ptr<Sokol::TContext>(Context);
}

//	gr: a lot of this is from/replaces TOpenglWindow
SokolOpenglContext::SokolOpenglContext(Sokol::TContextParams Params) :
	mParams	( Params )
{
	mWindow = std::dynamic_pointer_cast<Platform::TRenderView>(mParams.mRenderView);
	if (!mWindow)
		throw std::runtime_error("SokolOpenglContext with no win32 renderview");

	mWindowThread.reset(new Platform::TWin32Thread("SokolOpenglContext"));

	TOpenglParams OpenglParams;

	//	need to create on the correct thread
	//	this can be called at the same time that JS is assigning events
	//	so don't assume they're non-null
	//	do they need a lock?
	auto CreateControls = [&]()
	{
		bool Resizable = true;
		//mWindow.reset(new Platform::TWindow(Name, Rect, *mWindowThread, Resizable));
		auto& Control = mWindow->GetControl();	//	temporarily needed whilst renderview isnt actually its own control
		mOpenglContext.reset(new Platform::TOpenglContext(Control, OpenglParams));

		auto OnRender = [this](Opengl::TRenderTarget& RenderTarget, std::function<void()> LockContext)
		{
			auto Rect = RenderTarget.GetSize();
			this->OnPaint(Rect);
		};
		auto& PlatformContext = dynamic_cast<Platform::TOpenglContext&>(*mOpenglContext);
		PlatformContext.mOnRender = OnRender;

		mWindow->mOnPaint = [this](Platform::TControl&)
		{
			std::Debug << "On paint" << std::endl;
		};
		/*
		//	gr: can I use std::bind?
		auto& Win = static_cast<Platform::TControl&>(*mWindow);
		Win.mOnMouseDown = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	if (this->mOnMouseDown) this->mOnMouseDown(Pos, Button); };
		Win.mOnMouseUp = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	if (mOnMouseUp) this->mOnMouseUp(Pos, Button); };
		Win.mOnMouseMove = [this](Platform::TControl& Control, const TMousePos& Pos, SoyMouseButton::Type Button) {	if (mOnMouseMove) this->mOnMouseMove(Pos, Button); };
		Win.mOnKeyDown = [this](Platform::TControl& Control, SoyKeyButton::Type Key) {	if (mOnKeyDown)	this->mOnKeyDown(Key); };
		Win.mOnKeyUp = [this](Platform::TControl& Control, SoyKeyButton::Type Key) {	if (mOnKeyUp)		this->mOnKeyUp(Key); };
		*/
	};
	Soy::TSemaphore Wait;
	mWindowThread->PushJob(CreateControls, Wait);
	Wait.Wait("Win32 thread");
	//auto OnFrame = [this](Platform::TControl&)
	auto OnFrame = [this](Soy::Rectx<size_t> Rect)
	{
		this->OnPaint(Rect);
	};
	Params.mRenderView->mOnDraw = OnFrame;
	//PlatformWindow.mOnPaint = OnFrame;


	//	still need our own paint trigger
	auto PaintLoop = [this]()
	{
		auto FrameDelayMs = 1000 / mParams.mFramesPerSecond;
	/*
		//	gr: do this at a higher(c++) level so the check is cross platform
		if (!mEnableRenderWhenMinimised)
		{
			if (mWindow->IsMinimised())
				return true;
		}

		if (!mEnableRenderWhenBackground)
		{
			if (!mWindow->IsForeground())
				return true;
		}
		*/
		//	trigger repaint!
		this->DoPaint();
		
		//	sleep
		std::this_thread::sleep_for(std::chrono::milliseconds(FrameDelayMs));
		return mRunning;
	};

	//	make render loop
	mPaintThread.reset(new SoyThreadLambda("SokolOpenglContext Paint Loop", PaintLoop));
}

SokolOpenglContext::~SokolOpenglContext()
{
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

void SokolOpenglContext::OnPaint(Soy::Rectx<size_t> Rect)
{
	if (!mOpenglContext)
		return;

	std::lock_guard Lock(mOpenglContextLock);
	auto& PlatformContext = dynamic_cast<Platform::TOpenglContext&>(*mOpenglContext);
	mOpenglContext->Lock();

	if ( mSokolContext.id == 0 )
	{
		sg_desc desc={0};
		sg_setup(&desc);
		mSokolContext = sg_setup_context();
	}
	
	//	test we are actually drawing
	//glClearColor(1, 0, 0, 1);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	RunGpuJobs();

	vec2x<size_t> Size(Rect.w, Rect.h);
	mParams.mOnPaint( mSokolContext, Size );

	mOpenglContext->Unlock();
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