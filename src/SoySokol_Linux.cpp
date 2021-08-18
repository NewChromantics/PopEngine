#include <stdlib.h>
#include "TApiSokol.h"
#include "SoyWindowLinux.h"
#include "SoySokol_Linux.h"

#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol/sokol_gfx.h"

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

	//mESContext = &PlatformWindow.mESContext;

	std::function<void()> OnFrame = [this](){ this->OnPaint(); };

	auto PaintLoop = [this]()
	{
		auto FrameDelayMs = 1000/mParams.mFramesPerSecond;
		this->OnPaint();
		std::this_thread::sleep_for(std::chrono::milliseconds(FrameDelayMs));
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
	std::lock_guard<std::mutex> Lock(mOpenglContextLock);
	auto& Window = *mWindow;


	auto FlushedError = eglGetError();
	if ( FlushedError != EGLint(EGL_SUCCESS) )
		std::Debug << "Pre paint, flushed error=" << FlushedError << std::endl;

	//	switch context
	auto* CurrentContext = eglGetCurrentContext();
	if( CurrentContext != Window.mContext )
	{
		auto Result = eglMakeCurrent( Window.mDisplay, Window.mSurface, Window.mSurface, Window.mContext );
		if ( Result != EGL_TRUE )
		{
			Egl::IsOkay("eglMakeCurrent returned false");
		}
	}

	FlushedError = eglGetError();
	if ( FlushedError != EGLint(EGL_SUCCESS) )
		std::Debug << "Post eglMakeCurrent, flushed error=" << FlushedError << std::endl;

	auto NowCurrentContext = eglGetCurrentContext();
	if ( mSokolContext.id == 0 )
	{
		sg_desc desc={0};
		sg_setup(&desc);
		mSokolContext = sg_setup_context();
	}
	
	RunGpuJobs();

	auto Rect = Window.GetScreenRect();
	vec2x<size_t> Size( Rect.w, Rect.h );
	mParams.mOnPaint( mSokolContext, Size );

	FlushedError = eglGetError();
	if ( FlushedError != EGLint(EGL_SUCCESS) )
		std::Debug << "Post OnPaint, flushed error=" << FlushedError << std::endl;

	auto SwapResult = eglSwapBuffers( Window.mDisplay, Window.mSurface);
	if ( SwapResult != EGL_TRUE )
	{
		Egl::IsOkay("eglSwapBuffers returned false");
	}
}


void SokolOpenglContext::Queue(std::function<void(sg_context)> Exec)
{
	std::lock_guard<std::mutex> Lock(mGpuJobsLock);
	mGpuJobs.PushBack(Exec);
}