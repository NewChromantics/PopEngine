#include <stdlib.h>
#include "TApiSokol.h"
#include "SoyWindowLinux.h"
#include "SoySokol_Linux.h"

#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol/sokol_gfx.h"

std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params)
{
	auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);
	auto Context = new SokolOpenglContext(Window,Params);
	return std::shared_ptr<Sokol::TContext>(Context);

}

SokolOpenglContext::SokolOpenglContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params) :
	mParams	( Params )
{
	auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);

	mESContext = &PlatformWindow.mESContext;

	std::function<void()> OnFrame = [this](){ this->OnPaint(); };

	esRegisterDrawFunc( mESContext, OnFrame );

	auto PaintLoop = [this]()
	{
		auto FrameDelayMs = 1000/mParams.mFramesPerSecond;
		this->DoPaint();
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
	std::lock_guard Lock(mOpenglContextLock);


	auto FlushedError = eglGetError();
	if ( FlushedError != EGLint(EGL_SUCCESS) )
		std::Debug << "Pre paint, flushed error=" << FlushedError << std::endl;

	EGLBoolean makeCurrent;

	auto* CurrentContext = eglGetCurrentContext();
	if( CurrentContext != mESContext->eglContext )
		makeCurrent = eglMakeCurrent( mESContext->eglDisplay, mESContext->eglSurface, mESContext->eglSurface, mESContext->eglContext);

	FlushedError = eglGetError();
	if ( FlushedError != EGLint(EGL_SUCCESS) )
		std::Debug << "Post eglMakeCurrent, flushed error=" << FlushedError << std::endl;

	auto* NowCurretin = eglGetCurrentContext();
	if ( mSokolContext.id == 0 )
	{
		sg_desc desc={0};
		sg_setup(&desc);
		mSokolContext = sg_setup_context();
	}
	
	RunGpuJobs();

	auto Width = mESContext->screenWidth;
	auto Height = mESContext->screenHeight;
		
	vec2x<size_t> Size( Width, Height );
	mParams.mOnPaint( mSokolContext, Size );

	FlushedError = eglGetError();
	if ( FlushedError != EGLint(EGL_SUCCESS) )
		std::Debug << "Post OnPaint, flushed error=" << FlushedError << std::endl;
}

void SokolOpenglContext::DoPaint()
{
	esPaint(mESContext);
}

void SokolOpenglContext::Queue(std::function<void(sg_context)> Exec)
{
	std::lock_guard<std::mutex> Lock(mGpuJobsLock);
	mGpuJobs.PushBack(Exec);
}