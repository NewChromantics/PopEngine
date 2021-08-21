#include <stdlib.h>
#include "TApiSokol.h"
#include "SoyGuiLinux.h"
#include "SoySokol_Linux.h"

#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol/sokol_gfx.h"

#include "EglContext.h"


std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(Sokol::TContextParams Params)
{
	//auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);
	
	auto Context = new SokolOpenglContext(Params);
	return std::shared_ptr<Sokol::TContext>(Context);

}

// tsdk: removed the std::shared_ptr<SoyWindow> Window from this function
SokolOpenglContext::SokolOpenglContext(Sokol::TContextParams Params) :
	mParams	( Params )
{
	if ( !Params.mRenderView )
		throw Soy::AssertException("Linux SokolOpenglContext params missing render view");
	auto& PlatformRenderView = dynamic_cast<EglRenderView&>(*Params.mRenderView);
	mWindow = &PlatformRenderView.mWindow;

	auto PaintLoop = [this]()
	{
		auto FrameDelayMs = 1000/mParams.mFramesPerSecond;
		try
		{
			this->OnPaint();
			std::this_thread::sleep_for(std::chrono::milliseconds(FrameDelayMs));
		}
		catch(std::exception& e)
		{
			std::Debug << "Paint thread error" << e.what() << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(1*1000));
		}
		return mRunning;
	};

		//	make render loop
	mPaintThread.reset( new SoyThreadLambda("SokolOpenglContext Paint Loop", PaintLoop ) );
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


void SokolOpenglContext::OnPaint()
{
	std::lock_guard<std::mutex> Lock(mOpenglContextLock);
	auto& Window = *mWindow;

	if ( !Window.mContext )
		throw Soy::AssertException("Window has null context");

	Window.mContext->PrePaint();

	//auto NowCurrentContext = eglGetCurrentContext();
	if ( mSokolContext.id == 0 )
	{
		sg_desc desc={0};
		std::Debug << "SokolOpenglContext::sg_setup()" << std::endl;
		sg_setup(&desc);
		mSokolContext = sg_setup_context();
	}


	auto Rect = Window.GetScreenRect();
	vec2x<size_t> Size( Rect.w, Rect.h );
	mParams.mOnPaint( mSokolContext, Size );

	Window.mContext->PostPaint();
}


void SokolOpenglContext::Queue(std::function<void(sg_context)> Exec)
{
	std::lock_guard<std::mutex> Lock(mGpuJobsLock);
	mGpuJobs.PushBack(Exec);
}