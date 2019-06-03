#include "SoyOpengl.h"
#include <Cocoa/Cocoa.h>
#include "SoyOpenglWindow.h"
#include "SoyOpenglView.h"
#include "PopMain.h"


namespace Platform
{
	//	Osx doesn't have labels, so it's a kind of text box
	template<typename BASETYPE>
	class TTextBox_Base;
}

@interface TResponder : NSResponder
{
@public std::function<void()>	mCallback;
}

-(void) OnAction;

@end

@implementation TResponder

	-(void) OnAction
	{
		//	call lambda
		if ( !mCallback )
		{
			std::Debug << "TResponderCallback unhandled callback" << std::endl;
			return;
		}
		mCallback();
	}

@end

class Platform::TWindow : public SoyWindow
{
public:
	TWindow(PopWorker::TJobQueue& Thread) :
		mThread	( Thread )
	{
	}
	~TWindow()
	{
		[mWindow release];
	}
	
	virtual Soy::Rectx<int32_t>		GetScreenRect() override;
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	
	NSRect							GetChildRect(Soy::Rectx<int32_t> Rect);
	
public:
	PopWorker::TJobQueue&			mThread;	//	NS ui needs to be on the main thread
	NSWindow*						mWindow = nullptr;
	CVDisplayLinkRef				mDisplayLink = nullptr;
};


class Platform::TSlider : public SoySlider
{
public:
	TSlider(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t> Rect);
	~TSlider()
	{
		[mControl release];
	}
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect)override;

	virtual void		SetMinMax(uint16_t Min,uint16_t Max) override;
	virtual void		SetValue(uint16_t Value) override;
	virtual uint16_t	GetValue() override	{	return mLastValue;	}

	virtual void		OnChanged() override
	{
		CacheValue();
		SoySlider::OnChanged();
	}
	
protected:
	void				CacheValue();		//	call from UI thread
	
public:
	uint16_t				mLastValue = 0;	//	as all UI is on the main thread, we have to cache value for reading
	PopWorker::TJobQueue&	mThread;		//	NS ui needs to be on the main thread
	TResponder*				mResponder = [TResponder alloc];
	NSSlider*				mControl = nullptr;
};



//	on OSX there is no label, so use a TextField
//	todo: lets just do a text box for now and make it readonly later
//	https://stackoverflow.com/a/20169310/355753
template<typename BASETYPE=SoyTextBox>
class Platform::TTextBox_Base : public BASETYPE
{
public:
	TTextBox_Base(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t>& Rect);
	~TTextBox_Base()
	{
		[mControl release];
	}
	
	void					Create();
	
	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect)override;
	
	virtual void			SetValue(const std::string& Value) override;
	virtual std::string		GetValue() override	{	return mLastValue;	}
	
	
	virtual void 			OnChanged()=0;
	
protected:
	virtual void			ApplyStyle()	{}
	void					CacheValue();		//	call from UI thread
	
public:
	std::string				mLastValue;		//	as all UI is on the main thread, we have to cache value for reading
	PopWorker::TJobQueue&	mThread;		//	NS ui needs to be on the main thread
	TResponder*				mResponder = [TResponder alloc];
	NSTextField*			mControl = nullptr;
};

class Platform::TTextBox : public Platform::TTextBox_Base<SoyTextBox>
{
public:
	TTextBox(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t>& Rect) :
		TTextBox_Base( Thread, Parent, Rect )
	{
	}
	
	virtual void			OnChanged() override
	{
		CacheValue();
		SoyTextBox::OnChanged();
	}
};

class Platform::TLabel : public Platform::TTextBox_Base<SoyLabel>
{
public:
	TLabel(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t>& Rect) :
		TTextBox_Base( Thread, Parent, Rect )
	{
	}
	

	virtual void 	OnChanged() override
	{
	}
	
	virtual void	ApplyStyle() override;
};


class Platform::TTickBox : public SoyTickBox
{
public:
	TTickBox(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t> Rect);
	~TTickBox()
	{
		[mControl release];
	}
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect)override;
	
	virtual void		SetValue(bool Value) override;
	virtual bool		GetValue() override	{	return mLastValue;	}
	virtual void		SetLabel(const std::string& Label) override;
	
	virtual void		OnChanged() override
	{
		CacheValue();
		SoyTickBox::OnChanged();
	}
	
protected:
	void				CacheValue();		//	call from UI thread
	
public:
	bool					mLastValue = 0;	//	as all UI is on the main thread, we have to cache value for reading
	PopWorker::TJobQueue&	mThread;		//	NS ui needs to be on the main thread
	TResponder*				mResponder = [TResponder alloc];
	NSButton*				mControl = nullptr;
};











NSRect Platform::TWindow::GetChildRect(Soy::Rectx<int32_t> Rect)
{
	//	todo: make sure this is called on mThread
	auto ParentRect = [mWindow contentView].visibleRect;

	auto Left = std::max<CGFloat>( ParentRect.origin.x, Rect.Left() );
	auto Right = std::min<CGFloat>( ParentRect.origin.x + ParentRect.size.width, Rect.Right() );
	//	rect is upside in osx!
	//	todo: incorporate origin
	auto Top = ParentRect.size.height - Rect.Bottom();
	auto Bottom = ParentRect.size.height - Rect.Top();

	auto RectNs = NSMakeRect( Left, Top, Right-Left, Bottom - Top );
	return RectNs;
}


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
	
	
	auto& MainThread = *Soy::Platform::gMainThread;
	
	//	actual allocation must be on the main thread.
	auto Allocate = [=,&MainThread]
	{
		mWindow.reset( new Platform::TWindow(MainThread) );
		auto& Wrapper = *mWindow;
		auto*& mWindow = Wrapper.mWindow;

		NSUInteger Style = NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable;
		NSRect FrameRect = NSMakeRect( Rect.x, Rect.y, Rect.w, Rect.h );
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
		//[mWindow setBackgroundColor:[NSColor blueColor]];
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
		MainThread.PushJob( Allocate, Semaphore );
		Semaphore.Wait();
	}
	else
	{
		MainThread.PushJob( Allocate );
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
	return mWindow && mWindow->mWindow&& mView && mView->IsValid();
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

void TOpenglWindow::OnClosed()
{
	//	do osx cleanup of view etc
	SoyWindow::OnClosed();
}

std::chrono::milliseconds TOpenglWindow::GetSleepDuration()
{
	return std::chrono::milliseconds( 1000/mParams.mRefreshRate );
}


Soy::Rectx<int32_t> Platform::TWindow::GetScreenRect()
{
	throw Soy::AssertException(__FUNCTION__);
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




std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	std::shared_ptr<SoyWindow> pWindow( new Platform::TWindow(Thread) );
	
	//	actual allocation must be on the main thread.
	auto Allocate = [=]
	{
		auto& Wrapper = *dynamic_cast<Platform::TWindow*>( pWindow.get() );
		auto*& mWindow = Wrapper.mWindow;
		
		NSUInteger Style = NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable;
		NSRect FrameRect = NSMakeRect( Rect.x, Rect.y, Rect.w, Rect.h );
		NSRect WindowRect = [NSWindow contentRectForFrameRect:FrameRect styleMask:Style];
		
		bool Defer = NO;
		mWindow = [[NSWindow alloc] initWithContentRect:WindowRect styleMask:Style backing:NSBackingStoreBuffered defer:Defer];
		Soy::Assert(mWindow,"failed to create window");
		[mWindow retain];
		
		/*
		//	note: this is deffered, but as flags above don't seem to work right, not much choice
		//		plus, every other OSX app seems to do the same?
		pWindow->SetFullscreen( Params.mFullscreen );
		*/
		
		//	auto save window location
		auto AutoSaveName = Soy::StringToNSString( Name );
		[mWindow setFrameAutosaveName:AutoSaveName];
		
		id Sender = NSApp;
		[mWindow makeKeyAndOrderFront:Sender];
		
		auto Title = Soy::StringToNSString( Name );
		[mWindow setTitle:Title];
		//[mWindow setMiniwindowTitle:Title];
		//[mWindow setTitleWithRepresentedFilename:Title];
		
		//	mouse callbacks
		[mWindow setAcceptsMouseMovedEvents:YES];
	};
	
	//	move this to constructor
	static auto Wait = false;
	if ( Wait )
	{
		Soy::TSemaphore Semaphore;
		Thread.PushJob( Allocate, Semaphore );
		Semaphore.Wait();
	}
	else
	{
		Thread.PushJob( Allocate );
	}
	
	return pWindow;
}

std::shared_ptr<SoySlider> Platform::CreateSlider(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	auto& ParentWindow = dynamic_cast<TWindow&>(Parent);
	std::shared_ptr<SoySlider> pSlider( new Platform::TSlider(Thread,ParentWindow,Rect) );
	return pSlider;
}



Platform::TSlider::TSlider(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t> Rect) :
	mThread		( Thread )
{
	//	move this to constrctor
	auto Allocate = [this,Rect,&Parent]()
	{
		auto ChildRect = Parent.GetChildRect( Rect );
		mControl = [[NSSlider alloc] initWithFrame:ChildRect];
		[mControl retain];

		//	setup callback
		mResponder->mCallback = [this]()	{	this->OnChanged();	};
		mControl.target = mResponder;
		mControl.action = @selector(OnAction);
	
		[[Parent.mWindow contentView] addSubview:mControl];
	};
	mThread.PushJob( Allocate );
}

void Platform::TSlider::SetMinMax(uint16_t Min,uint16_t Max)
{
	auto Exec = [=]
	{
		if ( !mControl )
			throw Soy_AssertException("before slider created");
		
		mControl.minValue = Min;
		mControl.maxValue = Max;
		CacheValue();
	};
	
	mThread.PushJob(Exec);
}

void Platform::TSlider::SetValue(uint16_t Value)
{
	//	assuming success for immediate retrieval
	mLastValue = Value;
	
	auto Exec = [=]
	{
		if ( !mControl )
			throw Soy_AssertException("before slider created");
		mControl.intValue = Value;
		CacheValue();
	};
	mThread.PushJob( Exec );
}

void Platform::TSlider::CacheValue()
{
	//	todo: check is on mThread
	if ( !mControl )
		throw Soy_AssertException("before slider created");

	//	shouldn't let OSX slider get into this state
	auto Value = mControl.intValue;
	if ( Value < 0 || Value > std::numeric_limits<uint16_t>::max() )
	{
		std::stringstream Error;
		Error << "slider int value " << Value << " out of 16bit range";
		throw Soy_AssertException(Error);
	}
	
	mLastValue = Value;
}

void Platform::TSlider::SetRect(const Soy::Rectx<int32_t>& Rect)
{
	auto NewRect = NSMakeRect( Rect.x, Rect.y, Rect.w, Rect.h );

	auto Exec = [=]
	{
		if ( !mControl )
			throw Soy_AssertException("before slider created");
		
		mControl.frame = NewRect;
	};

	mThread.PushJob(Exec);
}




std::shared_ptr<SoyTickBox> Platform::CreateTickBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	auto& ParentWindow = dynamic_cast<TWindow&>(Parent);
	std::shared_ptr<SoyTickBox> pControl( new Platform::TTickBox(Thread,ParentWindow,Rect) );
	return pControl;
}



Platform::TTickBox::TTickBox(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t> Rect) :
	mThread		( Thread )
{
	//	move this to constrctor
	auto Allocate = [this,Rect,&Parent]()
	{
		auto ChildRect = Parent.GetChildRect( Rect );
		mControl = [[NSButton alloc] initWithFrame:ChildRect];
		[mControl retain];
		
		//	setup callback
		mResponder->mCallback = [this]()	{	this->OnChanged();	};
		mControl.target = mResponder;
		mControl.action = @selector(OnAction);
		
		[mControl setButtonType:NSSwitchButton];
		//[mControl setBezelStyle:0];
		
		//	windows & osx both have labels for tickbox, but our current api is that this isn't setup at construction
		mControl.title = @"";
		
		[[Parent.mWindow contentView] addSubview:mControl];
	};
	mThread.PushJob( Allocate );
}

void Platform::TTickBox::SetValue(bool Value)
{
	//	assuming success for immediate retrieval
	mLastValue = Value;
	
	auto Exec = [=]
	{
		if ( !mControl )
		throw Soy_AssertException("before slider created");
		mControl.state = Value ? NSControlStateValueOn : NSControlStateValueOff;
		CacheValue();
	};
	mThread.PushJob( Exec );
}

void Platform::TTickBox::CacheValue()
{
	//	todo: check is on mThread
	if ( !mControl )
		throw Soy_AssertException("before tickbox created");
	
	auto Value = mControl.state == NSControlStateValueOn;

	mLastValue = Value;
}

void Platform::TTickBox::SetLabel(const std::string& Label)
{
	auto Exec = [=]
	{
		if ( !mControl )
			throw Soy_AssertException("before TTickBox created");
		mControl.title = Soy::StringToNSString(Label);
		CacheValue();
	};
	mThread.PushJob( Exec );
}




void Platform::TTickBox::SetRect(const Soy::Rectx<int32_t>& Rect)
{
	auto NewRect = NSMakeRect( Rect.x, Rect.y, Rect.w, Rect.h );
	
	auto Exec = [=]
	{
		if ( !mControl )
		throw Soy_AssertException("before slider created");
		
		mControl.frame = NewRect;
	};
	
	mThread.PushJob(Exec);
}




std::shared_ptr<SoyTextBox> Platform::CreateTextBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	auto& ParentWindow = dynamic_cast<TWindow&>(Parent);
	std::shared_ptr<SoyTextBox> pControl( new Platform::TTextBox(Thread, ParentWindow, Rect ) );
	return pControl;
}

std::shared_ptr<SoyLabel> Platform::CreateLabel(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	auto& ParentWindow = dynamic_cast<TWindow&>(Parent);
	std::shared_ptr<SoyLabel> pControl( new Platform::TLabel(Thread, ParentWindow, Rect ) );
	return pControl;
}


template<typename BASETYPE>
Platform::TTextBox_Base<BASETYPE>::TTextBox_Base(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t>& Rect) :
	mThread		( Thread )
{
	//	move this to constrctor
	auto Allocate = [this,Rect,&Parent]()mutable
	{
		auto RectNs = Parent.GetChildRect(Rect);
	
		mControl = [[NSTextField alloc] initWithFrame:RectNs];
		[mControl retain];
	
		//	setup callback
		mResponder->mCallback = [this]()	{	this->OnChanged();	};
		mControl.target = mResponder;
		mControl.action = @selector(OnAction);
	
		ApplyStyle();
		
		[[Parent.mWindow contentView] addSubview:mControl];
	};
	mThread.PushJob( Allocate );
}

template<typename BASETYPE>
void Platform::TTextBox_Base<BASETYPE>::SetValue(const std::string& Value)
{
	//	assuming success for immediate retrieval
	mLastValue = Value;
	
	auto Exec = [=]
	{
		if ( !mControl )
			throw Soy_AssertException("before control created");
		
		mControl.stringValue = Soy::StringToNSString( Value );
		CacheValue();
	};
	mThread.PushJob( Exec );
}

template<typename BASETYPE>
void Platform::TTextBox_Base<BASETYPE>::CacheValue()
{
	//	todo: check is on mThread
	if ( !mControl )
		throw Soy_AssertException("before control created");
	
	auto Value = mControl.stringValue;
	mLastValue = Soy::NSStringToString( Value );
}

template<typename BASETYPE>
void Platform::TTextBox_Base<BASETYPE>::SetRect(const Soy::Rectx<int32_t>& Rect)
{
	auto NewRect = NSMakeRect( Rect.x, Rect.y, Rect.w, Rect.h );
	
	auto Exec = [=]
	{
		if ( !mControl )
			throw Soy_AssertException("before control created");
		
		mControl.frame = NewRect;
	};
	
	mThread.PushJob(Exec);
}


void Platform::TLabel::ApplyStyle()
{
	//	todo: check in thread
	[mControl setBezeled:NO];
	[mControl setDrawsBackground:NO];
	[mControl setEditable:NO];
	[mControl setSelectable:NO];
}

