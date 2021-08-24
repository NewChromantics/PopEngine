#include <stdlib.h>
#include "TApiSokol.h"
#include "SoyGuiLinux.h"
#include "SoySokol_Linux.h"

//#define SOKOL_ASSERT(c)	SokolAssert( c, #c )

void SokolAssert(bool Condition,const char* Context)
{
	if ( Condition )
		return;

	throw Soy::AssertException( std::string("Sokol assert; ") + Context );
}

#define SOKOL_IMPL
#define SOKOL_GLES3
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
	
	mRenderView = std::dynamic_pointer_cast<EglRenderView>(Params.mRenderView);

	auto OnFrame = [this](Soy::Rectx<size_t> Rect)
	{
		this->OnPaint(Rect);
	};
	Params.mRenderView->mOnDraw = OnFrame;

	auto PaintLoop = [this]()
	{
		auto FrameDelayMs = 1000/mParams.mFramesPerSecond;
		try
		{
			mRenderView->RequestPaint();
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


void SokolOpenglContext::OnPaint(Soy::Rectx<size_t> Rect)
{
	std::lock_guard<std::mutex> Lock(mOpenglContextLock);

	mRenderView->PrePaint();

	//	PROBABLY has to go after sokol setup?
	RunGpuJobs();



	//auto NowCurrentContext = eglGetCurrentContext();
	if ( mSokolContext.id == 0 )
	{
		auto InitialError = glGetError();
		std::Debug << "pre sg_setup error" << InitialError << std::endl;

		GLint num_ext=-1;
		glGetIntegerv(GL_NUM_EXTENSIONS, &num_ext);
		InitialError = glGetError();
		std::Debug << "GL_NUM_EXTENSIONS error" << InitialError << std::endl;


		sg_desc desc={0};
		//	sokol will fail if we use a GLES2 context when compiled for GLES3
		//desc.context.gl.force_gles2 = true;
		std::Debug << "SokolOpenglContext::sg_setup()" << std::endl;
		sg_setup(&desc);
		mSokolContext = sg_setup_context();
	}

	vec2x<size_t> Size( Rect.w, Rect.h );
	mParams.mOnPaint( mSokolContext, Size );

	mRenderView->PostPaint();
}


void SokolOpenglContext::Queue(std::function<void(sg_context)> Exec)
{
	std::lock_guard<std::mutex> Lock(mGpuJobsLock);
	mGpuJobs.PushBack(Exec);
}

void SokolOpenglContext::RunGpuJobs()
{
	//	to avoid infinite execution, copy the jobs and then run
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