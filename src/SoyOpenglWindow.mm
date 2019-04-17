#include "SoyOpengl.h"
#include <Cocoa/Cocoa.h>
#include "SoyOpenglWindow.h"
#include "SoyOpenglView.h"
#include "PopMain.h"


class Platform::TWindow
{
public:
	TWindow() :
		mWindow	( nullptr )
	{
	}
	~TWindow()
	{
		[mWindow release];
	}
	
	bool			IsValid()	{	return mWindow;	}
	
	bool			IsFullscreen();
	void			SetFullscreen(bool Fullscreen);

public:
	NSWindow*			mWindow;
	CVDisplayLinkRef	mDisplayLink;
};


CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
													const CVTimeStamp *inNow,
													const CVTimeStamp *inOutputTime,
													CVOptionFlags flagsIn,
													CVOptionFlags *flagsOut,
													void *displayLinkContext)
{
	auto* Window = reinterpret_cast<TOpenglWindow*>(displayLinkContext);
	Window->Iteration();
	return kCVReturnSuccess;
}

TOpenglWindow::TOpenglWindow(const std::string& Name,Soy::Rectf Rect,TOpenglParams Params) :
	SoyWorkerThread		( Soy::GetTypeName(*this), Params.mAutoRedraw ? SoyWorkerWaitMode::Sleep : SoyWorkerWaitMode::Wake ),
	mName				( Name ),
	mParams				( Params )
{
	//	gr; check we have an NSApplication initalised here and fail if running as command line app
#if !defined(TARGET_OSX_BUNDLE)
	throw Soy::AssertException("Cannot create windows in non-bundle apps, I don't think.");
#endif

	if ( !Soy::Platform::BundleInitialised )
		throw Soy::AssertException("NSApplication hasn't been started. Cannot create window");

	//	doesn't need to be on main thread, but we're not blocking main thread any more
	auto PostAllocate = [this]()
	{
		if ( mParams.mRedrawWithDisplayLink )
		{
			//	Synchronize buffer swaps with vertical refresh rate
			GLint SwapIntervals = 1;
			auto NSContext = mView->mView.openGLContext;
			[NSContext setValues:&SwapIntervals forParameter:NSOpenGLCPSwapInterval];
			
			auto& mDisplayLink = mWindow->mDisplayLink;
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
	};
	
	
	//	actual allocation must be on the main thread.
	auto Allocate = [=]
	{
		mWindow.reset( new Platform::TWindow );
		auto& Wrapper = *mWindow;
		auto*& mWindow = Wrapper.mWindow;

		NSUInteger Style = NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable;
		NSRect FrameRect = NSMakeRect( Rect.x, Rect.y, Rect.h, Rect.w );
		NSRect WindowRect = [NSWindow contentRectForFrameRect:FrameRect styleMask:Style];

		//	gr: this is unreliable, so we call our SetFullscreen() later
		/*
		if ( Params.mFullscreen )
		{
			Style &= ~NSWindowStyleMaskResizable;
			Style &= ~NSWindowStyleMaskTitled;	//	on mojave we can see title
			Style |= NSWindowStyleMaskFullScreen;
			
			auto* MainScreen = [NSScreen mainScreen];
			FrameRect = MainScreen.frame;
			WindowRect = FrameRect;
			
			//	hide dock & menu bar
			[NSMenu setMenuBarVisible:NO];
		}
		*/

		//	create a view
		mView.reset( new Platform::TOpenglView( vec2f(FrameRect.origin.x,FrameRect.origin.y), vec2f(FrameRect.size.width,FrameRect.size.height), Params ) );
		Soy::Assert( mView->IsValid(), "view isn't valid?" );

		auto OnRender = [this](Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext)
		{
			mOnRender(RenderTarget, LockContext );
		};
		mView->mOnRender = OnRender;

		//[[NSAutoreleasePool alloc] init];
		
		bool Defer = NO;
		mWindow = [[NSWindow alloc] initWithContentRect:WindowRect styleMask:Style backing:NSBackingStoreBuffered defer:Defer];
		Soy::Assert(mWindow,"failed to create window");
		[mWindow retain];

		//	note: this is deffered, but as flags above don't seem to work right, not much choice
		//		plus, every other OSX app seems to do the same?
		this->mWindow->SetFullscreen( Params.mFullscreen );
		

		/*
		[mWindow
		 setFrame:[mWindow frameRectForContentRect:[[mWindow screen] frame]]
		 display:YES
		 animate:YES];
		//[mWindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
		//[mWindow setFrame:screenFrame display:YES];
		//[mWindow toggleFullScreen:self];
		*/
		
		
		//	auto save window location
		auto AutoSaveName = Soy::StringToNSString( Name );
		[mWindow setFrameAutosaveName:AutoSaveName];
		

		//[mWindow setDelegate:[NSApp delegate]];
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
		
		//	mouse callbacks
		[mWindow setAcceptsMouseMovedEvents:YES];
		mView->mOnMouseDown = [this](const TMousePos& MousePos,SoyMouseButton::Type MouseButton)	{	if ( this->mOnMouseDown )	this->mOnMouseDown(MousePos,MouseButton);	};
		mView->mOnMouseMove = [this](const TMousePos& MousePos,SoyMouseButton::Type MouseButton)	{	if ( this->mOnMouseMove )	this->mOnMouseMove(MousePos,MouseButton);	};
		mView->mOnMouseUp = [this](const TMousePos& MousePos,SoyMouseButton::Type MouseButton)		{	if ( this->mOnMouseUp )	this->mOnMouseUp(MousePos,MouseButton);	};
		mView->mOnKeyDown = [this](SoyKeyButton::Type Button)	{	if ( this->mOnKeyDown )	this->mOnKeyDown(Button);	};
		mView->mOnKeyUp = [this](SoyKeyButton::Type Button)		{	if ( this->mOnKeyUp )	this->mOnKeyUp(Button);	};
		mView->mOnTryDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	return this->mOnTryDragDrop ? this->mOnTryDragDrop(Filenames) : false;	};
		mView->mOnDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	if ( this->mOnDragDrop ) this->mOnDragDrop(Filenames);	};

		//	doesn't need to be on main thread, but is deffered
		PostAllocate();
	};
	
	auto Wait = false;
	if ( Wait )
	{
		Soy::TSemaphore Semaphore;
		Soy::Platform::gMainThread->PushJob( Allocate, Semaphore );
		Semaphore.Wait();
	}
	else
	{
		Soy::Platform::gMainThread->PushJob( Allocate );
	}
}

TOpenglWindow::~TOpenglWindow()
{
	std::Debug << __func__ << std::endl;
	mView.reset();
	mWindow.reset();
}
	
bool TOpenglWindow::IsValid()
{
	return mWindow && mWindow->IsValid() && mView && mView->IsValid();
}

bool TOpenglWindow::Iteration()
{
	if ( !IsValid() )
	{
		std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
		return true;
	}
	
	//	see if this works, we're interrupting the main thread though
	dispatch_queue_t q = dispatch_get_main_queue();
	dispatch_async(q, ^{
		[mView->mView setNeedsDisplay: YES];
	});
	return true;
	
	static bool RedrawOnMainThread = false;
	
	auto RedrawImpl = [this]
	{
		//	gr: OSX/Xcode will give a warning if this is not called on the main thread
		[mView->mView setNeedsDisplay:YES];
		//[mView->mView display];
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

std::shared_ptr<Opengl::TContext> TOpenglWindow::GetContext()
{
	if ( !mView )
		return nullptr;
	
	return mView->mContext;
}

std::chrono::milliseconds TOpenglWindow::GetSleepDuration()
{
	return std::chrono::milliseconds( 1000/mParams.mRefreshRate );
}

Soy::Rectx<int32_t> TOpenglWindow::GetScreenRect()
{
	//	this must be called on the main thread, so we use the cache from the render target
	//return mView->GetScreenRect();
	return mView->mRenderTarget.GetSize();
}

void TOpenglWindow::SetFullscreen(bool Fullscreen)
{
	if ( !mWindow )
		return;
	
	mWindow->SetFullscreen(Fullscreen);
}

bool TOpenglWindow::IsFullscreen()
{
	if ( !mWindow )
		return false;
	
	return mWindow->IsFullscreen();
}

bool Platform::TWindow::IsFullscreen()
{
	if ( !mWindow )
		throw Soy::AssertException("IsFullscreen: no window");

	auto Style = [mWindow styleMask];
	Style &= NSWindowStyleMaskFullScreen;
	return Style == NSWindowStyleMaskFullScreen;
}


void Platform::TWindow::SetFullscreen(bool Fullscreen)
{
	if ( !mWindow )
		throw Soy::AssertException("Platform::TWindow::SetFullscreen() missing platform window");
	
	//	if not done on main thread, this blocks,
	//	then opengl waits for js context lock to free up and we get a deadlock
	auto DoSetFullScreen = [this,Fullscreen]()
	{
		//	check current state and change if we have to
		//	toggle seems to be the only approach
		auto OldFullscreen = this->IsFullscreen();
		if ( Fullscreen == OldFullscreen )
			return;

		[mWindow toggleFullScreen:nil];
	};
	Soy::Platform::gMainThread->PushJob( DoSetFullScreen );
}

