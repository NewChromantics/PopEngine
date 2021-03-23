#include "TApiSokol.h"
#include "PopMain.h"
#include <magic_enum.hpp>

#include "SoyGuiApple.h"
#include "SoyWindowApple.h"
#include "SoySokol_Osx.h"

#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif


#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include <Opengl/gl3.h>
#include <Opengl/gl3ext.h>

#include "sokol/sokol_gfx.h"


#if !defined(ENABLE_OPENGL)
#error Compiling ios gles support but ENABLE_OPENGL not defined
#endif

namespace Swift
{
	std::shared_ptr<Sokol::TContext>	CreateSokolContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params);
}

std::shared_ptr<Sokol::TContext> Swift::CreateSokolContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params)
{
	std::shared_ptr<Gui::TRenderView> RenderView = Platform::GetRenderView(*Window,Params.mViewName);
	if ( !RenderView )
		throw Soy::AssertException(std::string("Didn't find an existing render view name ")+Params.mViewName);

	auto PlatformRenderView = std::dynamic_pointer_cast<Platform::TRenderView>(RenderView);
	if ( !PlatformRenderView )
		throw Soy::AssertException("Found render view but is not a Platform::TRenderView");
	
	auto* GlView = PlatformRenderView->GetOpenglView();
	auto* MetalView = PlatformRenderView->GetMetalView();
	
#if defined(ENABLE_OPENGL)
	if ( GlView )
	{
		//	gr: reaplce with PlatformRenderView as it needs to be held onto
		auto* Context = new SokolOpenglContext(Window,GlView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
#endif
	
#if defined(ENABLE_METAL)
	if ( MetalView )
	{
		auto* Context = new SokolMetalContext(Window,MetalView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
#endif
	
	return nullptr;
}


std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params)
{
	try
	{
		auto SwiftContext = Swift::CreateSokolContext( Window, Params );
		return SwiftContext;
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}


	auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);
	
	//	get the view with matching name, if it's a metal view, make a metal context
	//	if its gl, make a gl context
	auto* View = PlatformWindow.GetChild(Params.mViewName);
	if ( !View )
	{
		std::stringstream Error;
		Error << "Failed to find child view " << Params.mViewName << " (required on ios)";
		throw Soy::AssertException(Error);
	}
	
	auto ClassName = Soy::NSStringToString(NSStringFromClass([View class]));
	std::Debug << "View " << Params.mViewName << " class name " << ClassName << std::endl;
	
#if defined(ENABLE_METAL)
	if ( ClassName == "MTKView" )
	{
		MTKView* MetalView = (MTKView*)View;
		auto* Context = new SokolMetalContext(Window,MetalView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
#endif
	
#if defined(ENABLE_OPENGL)
	if ( ClassName == "GLKView" )
	{
		auto* GlView = (NSOpenGLView*)View;
		auto* Context = new SokolOpenglContext(Window,GlView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
#endif
	
	std::stringstream Error;
	Error << "Class of view " << Params.mViewName << " is not MTKView or GLKView; " << ClassName;
	throw Soy::AssertException(Error);
}

@class GLView;


SokolOpenglContext::SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLView* View,Sokol::TContextParams Params) :
	mView	( View ),
	mParams	( Params )
{
	//	get opengl context attached to view
	mOpenglContext = mView.openGLContext;
	auto OpenglContextHandle = [mOpenglContext CGLContextObj];
	if ( !OpenglContextHandle )
		throw Soy::AssertException("No opengl context on NSOpenglView");
	
	
	auto Error = CGLEnable( OpenglContextHandle, kCGLCEMPEngine);
	if ( Error == kCGLNoError )
		std::Debug << "Opengl multithreading enabled" << std::endl;
	else
		std::Debug << "Opengl multithreading not enabled" << std::endl;

	//	sync with vsync
	GLint swapInt = 60;//Params.mVsyncSwapInterval;
	[mOpenglContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

    auto TriggerPaint = [this]()
    {
        this->RequestPaint();
    };

    mView->OnDrawRect = TriggerPaint;
	//	gr: this doesn't do anything, need to call the func
/*	//mView.enableSetNeedsDisplay = YES;

	auto OnFrame = [this](CGRect Rect)
	{
		this->OnPaint(Rect);
	};
	mDelegate = [[SokolViewDelegate_Gles alloc] init:OnFrame];
	[mView setDelegate:mDelegate];
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


void SokolOpenglContext::OnPaint(CGRect Rect)
{
	std::lock_guard Lock(mOpenglContextLock);
	auto Context = [mOpenglContext CGLContextObj]; 
	CGLLockContext(Context);

	auto* CurrentContext = CGLGetCurrentContext();
	if ( CurrentContext != Context )
	{
		auto Error = CGLSetCurrentContext( Context );
		if ( Error != kCGLNoError )
		{
			std::stringstream ErrorString;
			ErrorString << "Error setting current context: " << magic_enum::enum_name(Error);
			throw Soy::AssertException(ErrorString.str());
		}
	}
	
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
		
	glClearColor(0,1,1,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		
	auto Width = Rect.size.width;
	auto Height = Rect.size.height;
	//auto Width = mView.drawableWidth;
	///auto Height = mView.drawableHeight;
		
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
		//	gr: this is for IOS, not osx
		//[mView setNeedsDisplay];
		[mView setNeedsDisplay:YES];
	};
	//	gr: this is getting invoked immediately when job is pushed
	Platform::RunJobOnMainThread(SetNeedDisplay,false);
}


void SokolOpenglContext::Queue(std::function<void(sg_context)> Exec)
{
	std::lock_guard<std::mutex> Lock(mGpuJobsLock);
	mGpuJobs.PushBack(Exec);
}

