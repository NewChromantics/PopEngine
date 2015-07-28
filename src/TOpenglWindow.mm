#include "SoyOpengl.h"
#include <Cocoa/Cocoa.h>
#include "TOpenglWindow.h"
#include "TOpenglView.h"
#include "PopMain.h"


class MacWindow
{
public:
	MacWindow() :
		mWindow	( nullptr )
	{
	}
	~MacWindow()
	{
		[mWindow release];
	}
	
	bool			IsValid()	{	return mWindow;	}

public:
	NSWindow*		mWindow;
	CVDisplayLinkRef				mDisplayLink;
};


CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
													const CVTimeStamp *inNow,
													const CVTimeStamp *inOutputTime,
													CVOptionFlags flagsIn,
													CVOptionFlags *flagsOut,
													void *displayLinkContext)
{
	auto* Window = reinterpret_cast<TOpenglWindow*>(displayLinkContext);
	
	NSRect Bounds = NSMakeRect( 0, 0, 100, 100 );
	[Window->mView->mView drawRect:Bounds];
	/*
	auto* pContext = Window->GetContext();
	if ( !pContext )
		return kCVReturnError;
	auto& Context = *pContext;
	
	Context.Lock();
	
	// Add your drawing codes here
		
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	drawAnObject();
	glFlush();
	
	//[currentContext flushBuffer];
	Context.Unlock();
	 */
	return kCVReturnSuccess;
}

TOpenglWindow::TOpenglWindow(const std::string& Name,vec2f Pos,vec2f Size) :
	SoyWorkerThread		( Soy::GetTypeName(*this), SoyWorkerWaitMode::Sleep ),
	mName				( Name )
{
	//	gr; check we have an NSApplication initalised here and fail if running as command line app
#if !defined(TARGET_OSX_BUNDLE)
	throw Soy::AssertException("Cannot create windows in non-bundle apps, I don't think.");
#endif

	if ( !Soy::Platform::BundleInitialised )
		throw Soy::AssertException("NSApplication hasn't been started. Cannot create window");

	auto Allocate = [&Name,this,&Pos,&Size]
	{
		mMacWindow.reset( new MacWindow );
		auto& Wrapper = *mMacWindow;
		auto*& mWindow = Wrapper.mWindow;

		
		
		
		//NSUInteger styleMask =    NSBorderlessWindowMask;
		NSUInteger Style = NSTitledWindowMask|NSClosableWindowMask|NSResizableWindowMask;
		//NSUInteger Style = NSBorderlessWindowMask;
		NSRect FrameRect = NSMakeRect( Pos.x, Pos.y, Size.x, Size.y );
		NSRect WindowRect = [NSWindow contentRectForFrameRect:FrameRect styleMask:Style];
	//	NSRect WindowRect = NSMakeRect( Pos.x, Pos.y, Size.x, Size.y );
	//	NSRect WindowRect = [[NSScreen mainScreen] frame];

		//	create a view
		mView.reset( new TOpenglView( vec2f(FrameRect.origin.x,FrameRect.origin.y), vec2f(FrameRect.size.width,FrameRect.size.height) ) );
		Soy::Assert( mView->IsValid(), "view isn't valid?" );

		mOnRenderListener = mView->mOnRender.AddListener( *this, &TOpenglWindow::OnViewRender );

		//[[NSAutoreleasePool alloc] init];
		
		bool Defer = NO;
		mWindow = [[NSWindow alloc] initWithContentRect:WindowRect styleMask:Style backing:NSBackingStoreBuffered defer:Defer];
		Soy::Assert(mWindow,"failed to create window");

		[mWindow retain];

		//[mWindow setDelegate:[NSApp delegate]];
		//[mWindow setAcceptsMouseMovedEvents:TRUE];
		//[mWindow setIsVisible:TRUE];
		//[mWindow makeKeyAndOrderFront:nil];
		//[mWindow setStyleMask:NSTitledWindowMask|NSClosableWindowMask];
		
		//	setup window
	//	[Window setLevel:NSMainMenuWindowLevel+1];
	//	[Window setOpaque:YES];
	//	[Window setHidesOnDeactivate:YES];

		
		//	assign view to window
		[mWindow setContentView: mView->mView];

		id Sender = NSApp;
		[mWindow setBackgroundColor:[NSColor blueColor]];
		[mWindow makeKeyAndOrderFront:Sender];

		auto Title = Soy::StringToNSString( Name );
		[mWindow setTitle:Title];
		//[mWindow setMiniwindowTitle:Title];
		//[mWindow setTitleWithRepresentedFilename:Title];
		
		return true;
	};
	Soy::TSemaphore Semaphore;
	Soy::Platform::gMainThread->PushJob( Allocate, Semaphore );
	Semaphore.Wait();

	
	static bool UseDisplayLink = false;

	if ( UseDisplayLink )
	{
		//	Synchronize buffer swaps with vertical refresh rate
		GLint swapInt = 1;
		auto NSContext = mView->mView.openGLContext;
		[NSContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
		
		auto& mDisplayLink = mMacWindow->mDisplayLink;
		// Create a display link capable of being used with all active displays
		CVDisplayLinkCreateWithActiveCGDisplays(&mDisplayLink);
		
		// Set the renderer output callback function
		CVDisplayLinkSetOutputCallback(mDisplayLink, &DisplayLinkCallback, this );
		
		// Set the display link for the current renderer
		CGLContextObj cglContext = [NSContext CGLContextObj];
		CGLPixelFormatObj cglPixelFormat = NSContext.pixelFormat.CGLPixelFormatObj;
		CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext( mDisplayLink, cglContext, cglPixelFormat);
		
		// Activate the display link
		CVDisplayLinkStart( mDisplayLink );
	}
	else
	{
		SoyWorkerThread::Start();
	}
}

TOpenglWindow::~TOpenglWindow()
{
	std::Debug << __func__ << std::endl;
	mView.reset();
	mMacWindow.reset();
}
	
bool TOpenglWindow::IsValid()
{
	return mMacWindow && mMacWindow->IsValid() && mView && mView->IsValid();
}

bool TOpenglWindow::Iteration()
{
	if ( !IsValid() )
	{
		std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
		return true;
	}
	
	static bool RedrawOnMainThread = true;
	
	auto RedrawImpl = [this]
	{
		[mView->mView setNeedsDisplay:YES];
		//[mView->mView display];
		return true;
	};
	
	if ( RedrawOnMainThread )
	{
		//	if we're drawing on the main thread, wait for it to finish before triggering again
		//	we can easily trigger a redraw before the draw has finished (wait 16ms, render takes 17ms),
		//	main thread never gets out of job queue
		//	waiting on a semaphore means we just draw every N ms and don't repeat ourselves
		//	change this to a "dirty" system
		Soy::TSemaphore Semaphore;
		Soy::Platform::gMainThread->PushJob( RedrawImpl, Semaphore );
		Semaphore.Wait();
	}
	else
	{
		RedrawImpl();
	}
	
	return true;
}

Opengl::TContext* TOpenglWindow::GetContext()
{
	if ( !mView )
		return nullptr;
	
	return &mView->mContext;
}


