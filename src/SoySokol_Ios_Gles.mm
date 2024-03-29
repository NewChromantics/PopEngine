

#include "TApiSokol.h"
#include "SoyWindowApple.h"
#include "PopMain.h"

#include "SoySokol_Ios_Gles.h"
#include "SoyGuiApple.h"


//	include a GLES implementation
#define SOKOL_IMPL
//#define SOKOL_GLES2
#define SOKOL_GLES3

#if defined(SOKOL_GLES3)
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#endif

//namespace GlSokol
//{
	#include "sokol/sokol_gfx.h"
//}

#if !defined(ENABLE_OPENGL)
#error Compiling ios gles support but ENABLE_OPENGL not defined
#endif




@class GLView;




SokolOpenglContext::SokolOpenglContext(GLView* View,Sokol::TContextParams Params) :
	mView	( View ),
	mParams	( Params )
{
	//	can we get this from sokol impl?
#if defined(SOKOL_GLES2)
	auto Api = kEAGLRenderingAPIOpenGLES2;
#elif defined(SOKOL_GLES3)
	auto Api = kEAGLRenderingAPIOpenGLES3;
#else
#pragma message Unknown GL API selected
	auto Api = kEAGLRenderingAPIOpenGLES1;
#endif
	mOpenglContext = [[EAGLContext alloc] initWithAPI:Api];
	
	//	verify the API we've created
	{
		auto ContextApi = [mOpenglContext API];
		std::Debug << "EAGLContext API: " << magic_enum::enum_name(ContextApi) << std::endl;
	}
	mView.context = mOpenglContext;

	//	gr: this doesn't do anything, need to call the func
	mView.enableSetNeedsDisplay = YES;

	auto OnFrame = [this](Soy::Rectx<size_t> Rect)
	{
		this->OnPaint(Rect);
	};
	Params.mRenderView->mOnDraw = OnFrame;


	/*gr: this still isn't triggering, I think it needs to be in the view tree
	 but Interface Builder won't let us add this, and I can't see how (as its not a view)...
	//	built in auto-renderer
	mViewController = [[GLKViewController alloc] init];
	[mViewController setView:mView];
	mViewController.preferredFramesPerSecond = FrameRate;
	[mViewController setDelegate:mDelegate];
*/
	/*
	mContextDesc.color_format = _SG_PIXELFORMAT_DEFAULT;
	mContextDesc.depth_format = _SG_PIXELFORMAT_DEFAULT;
	mContextDesc.sample_count = 1;
	mContextDesc.gl.force_gles2 = true;
	*/
	
	//	gr: given that TriggerPaint needs to be on the main thread,
	//		maybe this thread should just something on the main dispath queue
	//		that could be dangerous for deadlocks on destruction though
	auto PaintLoop = [this]()
	{
		auto FrameDelayMs = 1000/mParams.mFramesPerSecond;
		this->RequestViewPaint();
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


void SokolOpenglContext::OnPaint(Soy::Rectx<size_t> Rect)
{
	std::lock_guard Lock(mOpenglContextLock);
	auto* CurrentContext = [EAGLContext currentContext];
	if ( CurrentContext != mOpenglContext )
		[EAGLContext setCurrentContext:mOpenglContext];

	auto FlushedError = glGetError();
	if ( FlushedError != 0 )
		std::Debug << "Pre paint, flushed error=" << FlushedError << std::endl;

	if ( mSokolContext.id == 0 )
	{
		sg_desc desc={0};
		sg_setup(&desc);
		mSokolContext = sg_setup_context();
	}

	//	run any jobs
	RunGpuJobs();
		
	//	sokol can leave things with an error, unsetting current context flushes glGetError
	//	seems like wrong approach...
	//[EAGLContext setCurrentContext:nullptr];
	
	//std::Debug << __PRETTY_FUNCTION__ << "(" << Rect.origin.x << "," << Rect.origin.y << "," << Rect.size.width << "," << Rect.size.height << ")" << std::endl;
	
	//	gr: dont clear in case OnPaint doesnt do anything	
	//glClearColor(0,1,1,1);
	//glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		
	//auto Width = Rect.size.width;
	//auto Height = Rect.size.height;
	auto Width = mView.drawableWidth;
	auto Height = mView.drawableHeight;
		
	vec2x<size_t> Size( Width, Height );
	mParams.mOnPaint( mSokolContext, Size );
		
	FlushedError = glGetError();
	if ( FlushedError != 0 )
		std::Debug << "Post OnPaint, flushed error=" << FlushedError << std::endl;
}


void SokolOpenglContext::RequestPaint()
{
	//	is auto drawing
	if ( mPaintThread )
		return;
	
	RequestViewPaint();
}

void SokolOpenglContext::RequestViewPaint()
{
	//	must be on UI thread, we should be queuing this up on the main window, maybe?
	//	gr: this is a single Dirty-Rect call
	//	a GLViewController will do regular drawing for us
	auto SetNeedDisplay = [this]()
	{
		[mView setNeedsDisplay];
	};
	//	gr: this is getting invoked immediately when job is pushed
	Platform::RunJobOnMainThread(SetNeedDisplay,false);
}


void SokolOpenglContext::Queue(std::function<void(sg_context)> Exec)
{
	std::lock_guard<std::mutex> Lock(mGpuJobsLock);
	mGpuJobs.PushBack(Exec);
}

