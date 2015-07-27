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
};



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

	SoyWorkerThread::Start();
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

bool TOpenglWindow::Redraw()
{
	if ( !IsValid() )
		return false;
	
	[mView->mView display];
	return true;
}

Opengl::TContext* TOpenglWindow::GetContext()
{
	if ( !mView )
		return nullptr;
	
	return &mView->mContext;
}


