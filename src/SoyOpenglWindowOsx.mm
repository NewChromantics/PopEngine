#include "SoyOpengl.h"
#if defined(TARGET_OSX)
#include <Cocoa/Cocoa.h>
#endif
#include "SoyOpenglWindow.h"
#include "SoyOpenglView.h"
#include "PopMain.h"
#include <magic_enum.hpp>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

void RunJobOnMainThread(std::function<void()> Lambda,bool Block)
{
	Soy::TSemaphore Semaphore;

	//	testing if raw dispatch is faster, results negligable
	static bool UseNsDispatch = false;
	
	if ( UseNsDispatch )
	{
		Soy::TSemaphore* pSemaphore = Block ? &Semaphore : nullptr;
		
		dispatch_async( dispatch_get_main_queue(), ^(void){
			Lambda();
			if ( pSemaphore )
				pSemaphore->OnCompleted();
		});
		
		if ( pSemaphore )
			pSemaphore->WaitAndReset();
	}
	else
	{
		auto& Thread = *Soy::Platform::gMainThread;
		if ( Block )
		{
			Thread.PushJob(Lambda,Semaphore);
			Semaphore.WaitAndReset();
		}
		else
		{
			Thread.PushJob(Lambda);
		}
	}
}

namespace Platform
{
	//	Osx doesn't have labels, so it's a kind of text box
	template<typename BASETYPE>
	class TTextBox_Base;
	
	class TNsView;		//	common NSView control stuff
	
	NSColor*		GetColour(vec3x<uint8_t> Rgb);
	vec3x<uint8_t>	GetColour(NSColor* Colour);
}

bool HandleDragFilenames(id sender,std::function<bool(ArrayBridge<std::string>&)>& Callback)
{
	Soy::TScopeTimerPrint Timer( __func__, 1 );

	//	based on https://stackoverflow.com/questions/2604522/registerfordraggedtypes-with-custom-file-formats
	NSArray* FilenamesArray = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];

	Array<std::string> Filenames;

	for ( int f=0;	f<[FilenamesArray count];	f++ )
	{
		NSString* FilenameNs = [FilenamesArray objectAtIndex:f];
		auto Filename = Soy::NSStringToString(FilenameNs);
		Filenames.PushBack(Filename);
	}

	auto FilenamesBridge = GetArrayBridge(Filenames);
	std::Debug << "TryDragDrop( " << Soy::StringJoin( FilenamesBridge,", ") << ")" << std::endl;

	if ( !Callback )
		return false;

	auto Result = Callback(FilenamesBridge);
	return Result;
}

//	note: we want to re-use stuff that's in MacOpenglView too
@interface Platform_View: NSView
{
	@public std::function<NSDragOperation()>				mGetDragDropCursor;
	@public std::function<bool(ArrayBridge<std::string>&)>	mTryDragDrop;
	@public std::function<bool(ArrayBridge<std::string>&)>	mOnDragDrop;
}

-(void)RegisterForEvents;

//	overloaded funcs
- (BOOL) isFlipped;
/*
-(void)mouseMoved:(NSEvent *)event;
-(void)mouseDown:(NSEvent *)event;
-(void)mouseDragged:(NSEvent *)event;
-(void)mouseUp:(NSEvent *)event;
-(void)rightMouseDown:(NSEvent *)event;
-(void)rightMouseDragged:(NSEvent *)event;
-(void)rightMouseUp:(NSEvent *)event;
-(void)otherMouseDown:(NSEvent *)event;
-(void)otherMouseDragged:(NSEvent *)event;
-(void)otherMouseUp:(NSEvent *)event;

- (void)keyDown:(NSEvent *)event;
- (void)keyUp:(NSEvent *)event;
*/
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender;
- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender;


@end

@implementation Platform_View

- (BOOL) isFlipped
{
	return YES;
}

-(void)RegisterForEvents
{
	//	enable drag & drop
	//	https://stackoverflow.com/a/29029456
	//	https://stackoverflow.com/a/8567836	NSFilenamesPboardType
	[self registerForDraggedTypes: @[(NSString*)kUTTypeItem]];
	//[self registerForDraggedTypes:[NSImage imagePasteboardTypes]];
	//registerForDraggedTypes([NSFilenamesPboardType])
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	//	return the cursor to dispaly
	std::Debug << __func__ << std::endl;
	if ( mGetDragDropCursor )
		return mGetDragDropCursor();
	return NSDragOperationLink;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
	return HandleDragFilenames( sender, mTryDragDrop ) ? YES : NO;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	return HandleDragFilenames( sender, mOnDragDrop );
}


@end




@interface TResponder : NSResponder
{
@public std::function<void()>	mCallback;
}

-(void) OnAction;

@end


@interface TColourResponder : NSObject<NSWindowDelegate>
{
@public std::function<void(vec3x<uint8_t>)>	mOnChanged;
@public std::function<void()>				mOnClosed;
}

-(void) OnAction:(NSColorPanel*)ColourPanel;

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


@implementation TColourResponder

-(void)OnAction:(NSColorPanel*)ColourPanel
{
	auto Rgb = Platform::GetColour( ColourPanel.color );

	//	call lambda
	if ( !mOnChanged )
	{
		std::Debug << "TColourResponder unhandled callback " << Rgb << std::endl;
		return;
	}
	mOnChanged( Rgb );
}


- (void)windowWillClose:(NSNotification *)notification
{
	std::Debug << "Colour responder window closing" << std::endl;

	if ( mOnClosed )
		mOnClosed();
}

@end


class Platform::TNsView
{
protected:
	TNsView(PopWorker::TJobQueue& Thread) :
		mThread	( Thread )
	{
	}

	void				SetRect(const Soy::Rectx<int32_t>& Rect);
	virtual NSControl*	GetControl()=0;

protected:
	PopWorker::TJobQueue&	mThread;		//	NS ui needs to be on the main thread
};

NSColor* Platform::GetColour(vec3x<uint8_t> Rgb)
{
	auto GetFloat = [](uint8_t Component)
	{
		float f = static_cast<float>(Component);
		f /= 255.0f;
		return f;
	};

	auto r = GetFloat( Rgb.x );
	auto g = GetFloat( Rgb.y );
	auto b = GetFloat( Rgb.z );
	auto a = 1.0f;
	auto* Colour = [NSColor colorWithRed:r green:g blue:b alpha:a];

	return Colour;
}

vec3x<uint8_t> Platform::GetColour(NSColor* Colour)
{
	auto Red = Colour.redComponent;
	auto Green = Colour.greenComponent;
	auto Blue = Colour.blueComponent;
	auto Alpha = Colour.alphaComponent;

	auto Get8 = [](CGFloat Float)
	{
		Float *= 255.0f;
		if ( Float < 0 )	Float = 0;
		if ( Float > 255 )	Float = 255;
		return static_cast<uint8_t>( Float );
	};

	auto r8 = Get8(Red);
	auto g8 = Get8(Green);
	auto b8 = Get8(Blue);
	vec3x<uint8_t> Rgb( r8, g8, b8 );
	return Rgb;
}


class Platform::TWindow : public SoyWindow
{
public:
	TWindow(PopWorker::TJobQueue& Thread,const std::string& Name,const Soy::Rectx<int32_t>& Rect,bool Resizable,std::function<void()> OnAllocated=nullptr);
	~TWindow()
	{
		//	gr: this also isn't destroying the window
		[mWindow release];
	}

	virtual Soy::Rectx<int32_t>		GetScreenRect() override;
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	virtual bool					IsMinimised() override;
	virtual bool					IsForeground() override;
	virtual void					EnableScrollBars(bool Horz,bool Vert) override;

	NSRect							GetChildRect(Soy::Rectx<int32_t> Rect);
	NSView*							GetContentView();
	void							OnChildAdded(const Soy::Rectx<int32_t>& ChildRect);
	
public:
	PopWorker::TJobQueue&			mThread;	//	NS ui needs to be on the main thread
	NSWindow*						mWindow = nullptr;
	Platform_View*					mContentView = nullptr;	//	where controls go
	CVDisplayLinkRef				mDisplayLink = nullptr;
	
//  Added from SoyGuiOsx
	NSWindow*						GetWindow();
	NSView*							GetChild(const std::string& Name);
	void							EnumChildren(std::function<bool(NSView*)> EnumChild);
};


class Platform::TSlider : public SoySlider
{
public:
	TSlider(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t> Rect);
	~TSlider()
	{
		//	gr: this isn't deleting the control from the window, and so responder still exists, but callback doesn't fire
		if ( mResponder )
		{
			mResponder->mCallback = []
			{
				std::Debug << "Responder owner deleted" << std::endl;

			};
			[mResponder release];
		}

		if ( mControl )
		{
			[mControl release];
		}
	}

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect)override;

	virtual void		SetMinMax(uint16_t Min,uint16_t Max,uint16_t NotchCount) override;
	virtual void		SetValue(uint16_t Value) override;
	virtual uint16_t	GetValue() override	{	return mLastValue;	}

	virtual void		OnChanged(bool FinalValue) override
	{
		CacheValue();
		SoySlider::OnChanged(FinalValue);
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
class Platform::TTextBox_Base : public BASETYPE, public TNsView
{
public:
	TTextBox_Base(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t>& Rect);
	~TTextBox_Base()
	{
		[mControl release];
	}

	void					Create();

	virtual NSControl*		GetControl() override	{	return mControl;	}
	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override	{	TNsView::SetRect(Rect);	}

	virtual void			SetValue(const std::string& Value) override;
	virtual std::string		GetValue() override	{	return mLastValue;	}


	virtual void 			OnChanged()=0;

protected:
	virtual void			ApplyStyle()	{}
	void					CacheValue();		//	call from UI thread

public:
	std::string				mLastValue;		//	as all UI is on the main thread, we have to cache value for reading
	size_t					mValueVersion = 0;

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


class Platform::TTickBox : public SoyTickBox, public TNsView
{
public:
	TTickBox(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t> Rect);
	~TTickBox()
	{
		[mControl release];
	}

	virtual NSControl*	GetControl() override	{	return mControl;	}
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override	{	TNsView::SetRect(Rect);	}

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
	TResponder*				mResponder = [TResponder alloc];
	NSButton*				mControl = nullptr;
};




class Platform::TColourPicker : public Gui::TColourPicker
{
public:
	TColourPicker(PopWorker::TJobQueue& Thread,vec3x<uint8_t> InitialColour);
	~TColourPicker();

public:
	NSColorPanel*			mColorPanel = nullptr;
	PopWorker::TJobQueue&	mThread;
	TColourResponder*		mResponder = [TColourResponder alloc];
};


class Platform::TColourButton : public SoyColourButton, public TNsView
{
public:
	TColourButton(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t>& Rect);
	~TColourButton()
	{
		[mControl release];
	}

	virtual NSControl*		GetControl() override	{	return mControl;	}
	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override	{	TNsView::SetRect(Rect);	}
	virtual void			SetValue(vec3x<uint8_t> Value) override;

	virtual vec3x<uint8_t>	GetValue() override
	{
		return GetColour(mControl.color);
	}


private:
	TResponder*		mResponder = [TResponder alloc];
	NSColorWell*	mControl = nullptr;
};



class Platform::TImageMap : public Gui::TImageMap, public TNsView
{
public:
	TImageMap(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t> Rect);
	~TImageMap();

	virtual NSControl*	GetControl() override	{	return mControl;	}
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override	{	TNsView::SetRect(Rect);	}
	virtual void		SetImage(const SoyPixelsImpl& Pixels) override;
	virtual void		SetCursorMap(const SoyPixelsImpl& CursorMap,const ArrayBridge<std::string>&& CursorIndexes)override;

private:
	void				FreeImage();

public:
	TResponder*			mResponder = [TResponder alloc];
	NSImageView*		mControl = nullptr;
	CGImageRef			mCgImage = nullptr;
	NSImage*			mNsImage = nullptr;
	SoyPixels			mPixelsImage;		//	we keep a copy for thread use and CGImage references these bytes
};






Platform::TColourPicker::TColourPicker(PopWorker::TJobQueue& Thread,vec3x<uint8_t> InitialColour) :
	mThread		( Thread )
{
	auto Allocate = [this,InitialColour]()
	{
		mColorPanel = [[NSColorPanel alloc] init];
		[mColorPanel setTarget:mResponder];
		[mColorPanel setAction:@selector(OnAction:)];
		mColorPanel.continuous = TRUE;
		mColorPanel.showsAlpha = FALSE;
		mColorPanel.delegate = mResponder;

		//	set colour before setting callback, or we'll immediately get a callback, which I think we don't want because it wasn't instigated by the user
		mColorPanel.color = Platform::GetColour( InitialColour );

		mResponder->mOnChanged = [this](vec3x<uint8_t> Rgb)
		{
			if ( this->mOnValueChanged )
			{
				this->mOnValueChanged( Rgb );
			}
			else
			{
				std::Debug << "Colour picker changed (no callback)" << std::endl;
			}

		};

		mResponder->mOnClosed = [this]()
		{
			if ( this->mOnDialogClosed )
				this->mOnDialogClosed();
		};

		//	show
		[mColorPanel orderFront:nil];
	};
	mThread.PushJob( Allocate );
}

Platform::TColourPicker::~TColourPicker()
{
	//	need to make sure OnClosed is called here...
	[mColorPanel close];
	mResponder->mOnChanged = nullptr;
	mResponder->mOnClosed = nullptr;
}





NSView* Platform::TWindow::GetContentView()
{
	return mContentView;
	//NSScrollView* ScrollView = [mWindow contentView];
	//return ScrollView.contentView.documentView;
}

void Platform::TWindow::OnChildAdded(const Soy::Rectx<int32_t>& ChildRect)
{
	//	expand scroll space to match child rect min/max
	auto Right = ChildRect.Right();
	auto Bottom = ChildRect.Bottom();

	auto NewSize = mContentView.frame.size;
	NewSize.width = std::max<CGFloat>( NewSize.width, Right );
	NewSize.height = std::max<CGFloat>( NewSize.height, Bottom );

	NSScrollView* ScrollView = [mWindow contentView];
	auto* ClipView = ScrollView.contentView;
	ClipView.documentView.frameSize = NewSize;

}


NSRect Platform::TWindow::GetChildRect(Soy::Rectx<int32_t> Rect)
{
	//	todo: make sure this is called on mThread
	auto* ContentView = GetContentView();
	auto ParentRect = ContentView.visibleRect;

	auto Left = std::max<CGFloat>( ParentRect.origin.x, Rect.Left() );
	auto Right = std::min<CGFloat>( ParentRect.origin.x + ParentRect.size.width, Rect.Right() );

	auto Top = Rect.Top();
	auto Bottom = Rect.Bottom();

	//	rect is upside in osx!
	//	todo: incorporate origin
	if ( !ContentView.isFlipped )
	{
		Top = ParentRect.size.height - Rect.Bottom();
		Bottom = ParentRect.size.height - Rect.Top();
	}

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

TOpenglWindow::TOpenglWindow(const std::string& Name,const Soy::Rectx<int32_t>& Rect,TOpenglParams Params) :
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

	auto& Thread = *Soy::Platform::gMainThread;

	auto PostAllocate = [=]()
	{
		auto* Window = mWindow->mWindow;

		//	create a view
		NSRect FrameRect = NSMakeRect( Rect.x, Rect.y, Rect.w, Rect.h );
		mView.reset( new Platform::TOpenglView( vec2f(FrameRect.origin.x,FrameRect.origin.y), vec2f(FrameRect.size.width,FrameRect.size.height), Params ) );
		if ( !mView->IsValid() )
			throw Soy::AssertException("Opengl view isn't valid");

		auto OnRender = [this](Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext)
		{
			mOnRender(RenderTarget, LockContext );
		};
		mView->mOnRender = OnRender;

		//	note: this is deffered, but as flags above don't seem to work right, not much choice
		//		plus, every other OSX app seems to do the same?
		mWindow->SetFullscreen( Params.mFullscreen );

		//	gr: todo: this should be replaced with a proper OpenglView control anyway
		//		but we should lose the content view allocated in platform this way
		//	assign view to window
		[Window setContentView: mView->mView];

		mView->mOnMouseDown = [this](const TMousePos& MousePos,SoyMouseButton::Type MouseButton)	{	if ( this->mOnMouseDown )	this->mOnMouseDown(MousePos,MouseButton);	};
		mView->mOnMouseMove = [this](const TMousePos& MousePos,SoyMouseButton::Type MouseButton)	{	if ( this->mOnMouseMove )	this->mOnMouseMove(MousePos,MouseButton);	};
		mView->mOnMouseUp = [this](const TMousePos& MousePos,SoyMouseButton::Type MouseButton)		{	if ( this->mOnMouseUp )	this->mOnMouseUp(MousePos,MouseButton);	};
		mView->mOnKeyDown = [this](SoyKeyButton::Type Button)	{	if ( this->mOnKeyDown )	this->mOnKeyDown(Button);	};
		mView->mOnKeyUp = [this](SoyKeyButton::Type Button)		{	if ( this->mOnKeyUp )	this->mOnKeyUp(Button);	};
		mView->mOnTryDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	return this->mOnTryDragDrop ? this->mOnTryDragDrop(Filenames) : false;	};
		mView->mOnDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	if ( this->mOnDragDrop ) this->mOnDragDrop(Filenames);	};

		//	setup display link
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

	bool Resizable = true;
	mWindow.reset( new Platform::TWindow( Thread, Name, Rect, Resizable, PostAllocate) );
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


void TOpenglWindow::EnableScrollBars(bool Horz,bool Vert)
{
	if ( !mWindow )
		return;

	mWindow->EnableScrollBars( Horz, Vert );
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

bool TOpenglWindow::IsMinimised()
{
	if ( !mWindow )
		return false;

	return mWindow->IsMinimised();
}

bool TOpenglWindow::IsForeground()
{
	if ( !mWindow )
		return false;

	return mWindow->IsForeground();
}


Platform::TWindow::TWindow(PopWorker::TJobQueue& Thread,const std::string& Name,const Soy::Rectx<int32_t>& Rect,bool Resizable,std::function<void()> OnAllocated) :
	mThread	( Thread )
{
	//	actual allocation must be on the main thread.
	auto Allocate = [=]
	{
		NSUInteger Style = NSWindowStyleMaskTitled|NSWindowStyleMaskClosable;
		if ( Resizable )
			Style |= NSWindowStyleMaskResizable;

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

		mContentView = [[Platform_View alloc] initWithFrame:FrameRect];
		[mContentView RegisterForEvents];
		mContentView->mGetDragDropCursor = []
		{
			return NSDragOperationLink;
		};
		mContentView->mTryDragDrop = [this](ArrayBridge<std::string>& Filenames)
		{
			if ( !mOnTryDragDrop )
				return false;
			return this->mOnTryDragDrop( Filenames );
		};
		mContentView->mOnDragDrop = [this](ArrayBridge<std::string>& Filenames)
		{
			if ( !mOnDragDrop )
				return false;
			this->mOnDragDrop( Filenames );
			return true;
		};

		//	gr: on windows, a window can be scrollable
		//		on osx we need a view. In both cases, we can just hide the scrollbars, so lets always put it in a scrollview
		bool ScrollMode = true;
		if ( ScrollMode )
		{
			//	setup scroll view
			auto* ScrollView = [[NSScrollView alloc] initWithFrame:FrameRect];
			[ScrollView setBorderType:NSNoBorder];
			[ScrollView setHasVerticalScroller:NO];
			[ScrollView setHasHorizontalScroller:NO];
			//[ScrollView setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];

			//	the scrollview colour is different, lets make it look like a window
			ScrollView.backgroundColor = mWindow.backgroundColor;

			//	put scroll view on window
			[mWindow setContentView:ScrollView];

			//auto* ClipView = ScrollView.contentView;
			//ClipView.documentView = Wrapper.mContentView;
			//	custom scroll size...
			//ClipView.documentView.frameSize = NSMakeSize(800,8000);
			//ClipView.documentView = Wrapper.mContentView;

			//	assign document view
			[ScrollView setDocumentView:mContentView];

			//	gr: maybe we need to change first responder?
			//[theWindow makeKeyAndOrderFront:nil];
			//[theWindow makeFirstResponder:theTextView];

		}
		else
		{
			mWindow.contentView = mContentView;
		}

		if ( OnAllocated )
			OnAllocated();
	};

	mThread.PushJob( Allocate );
}

bool Platform::TWindow::IsFullscreen()
{
	if ( !mWindow )
		throw Soy::AssertException("IsFullscreen: no window");

	auto Style = [mWindow styleMask];
	Style &= NSWindowStyleMaskFullScreen;
	return Style == NSWindowStyleMaskFullScreen;
}

bool Platform::TWindow::IsMinimised()
{
	if ( !mWindow )
		throw Soy::AssertException("IsMinimsed: no window");

	return mWindow.miniaturized;
}


bool Platform::TWindow::IsForeground()
{
	if ( !mWindow )
		throw Soy::AssertException("IsForeground: no window");

	auto IsMainWindow = mWindow.isMainWindow;
	return IsMainWindow;
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
	mThread.PushJob( DoSetFullScreen );
}

void Platform::TWindow::EnableScrollBars(bool Horz,bool Vert)
{
	auto Exec = [=]()
	{
		NSScrollView* ScrollView = mWindow.contentView;
		[ScrollView setHasVerticalScroller:Vert?YES:NO];
		[ScrollView setHasHorizontalScroller:Horz?YES:NO];
	};
	mThread.PushJob(Exec);
}




std::shared_ptr<Gui::TColourPicker>	Platform::CreateColourPicker(vec3x<uint8_t> InitialColour)
{
	auto& Thread = *Soy::Platform::gMainThread;
	std::shared_ptr<Gui::TColourPicker> Picker( new Platform::TColourPicker( Thread, InitialColour ) );
	return Picker;
}

std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect,bool Resizable)
{
	auto& Thread = *Soy::Platform::gMainThread;
	std::shared_ptr<SoyWindow> pWindow( new Platform::TWindow(Thread,Name,Rect,Resizable) );
	return pWindow;
}

std::shared_ptr<SoySlider> Platform::CreateSlider(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	auto& ParentWindow = dynamic_cast<TWindow&>(Parent);
	std::shared_ptr<SoySlider> pSlider( new Platform::TSlider(Thread,ParentWindow,Rect) );
	return pSlider;
}


std::shared_ptr<SoyColourButton> Platform::CreateColourButton(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	auto& ParentWindow = dynamic_cast<TWindow&>(Parent);
	std::shared_ptr<SoyColourButton> pSlider( new Platform::TColourButton(Thread,ParentWindow,Rect) );
	return pSlider;
}

std::shared_ptr<Gui::TImageMap> Platform::CreateImageMap(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	auto& ParentWindow = dynamic_cast<TWindow&>(Parent);
	std::shared_ptr<Gui::TImageMap> pControl( new Platform::TImageMap(Thread,ParentWindow,Rect) );
	return pControl;
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
		mResponder->mCallback = [this]()
		{
			//	dont have a built in system atm for detecting end-drag style events
			//	https://stackoverflow.com/questions/9416903/determine-when-nsslider-knob-is-let-go-in-continuous-mode
			bool FinalValue = true;
			this->OnChanged(FinalValue);
		};
		mControl.target = mResponder;
		mControl.action = @selector(OnAction);

		auto* ParentView = Parent.GetContentView();
		[ParentView addSubview:mControl];
		Parent.OnChildAdded( Rect );
	};
	mThread.PushJob( Allocate );
}

void Platform::TSlider::SetMinMax(uint16_t Min,uint16_t Max,uint16_t NotchCount)
{
	auto Exec = [=]
	{
		if ( !mControl )
			throw Soy_AssertException("before slider created");

		mControl.minValue = Min;
		mControl.maxValue = Max;
		mControl.numberOfTickMarks = NotchCount;
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
	TNsView		( Thread )
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

		auto* ParentView = Parent.GetContentView();
		[ParentView addSubview:mControl];
		Parent.OnChildAdded( Rect );
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





std::shared_ptr<SoyTextBox> Platform::CreateTextBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	auto& Thread = *Soy::Platform::gMainThread;
	auto& ParentWindow = dynamic_cast<TWindow&>(Parent);
	std::shared_ptr<SoyTextBox> pControl( new Platform::TTextBox(Thread, ParentWindow, Rect ) );
	return pControl;
}


std::shared_ptr<SoyLabel> Platform::GetLabel(SoyWindow& Parent,const std::string& Name)
{
	Soy_AssertTodo();
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
	TNsView		( Thread )
{
	//	move this to constrctor
	auto Allocate = [this,Rect,&Parent]()mutable
	{
		auto RectNs = Parent.GetChildRect(Rect);

		mControl = [[NSTextField alloc] initWithFrame:RectNs];
		[mControl retain];

		//	don't let overflowing text disapear
		[[mControl cell]setLineBreakMode:NSLineBreakByTruncatingTail];

		//	setup callback
		mResponder->mCallback = [this]()	{	this->OnChanged();	};
		mControl.target = mResponder;
		mControl.action = @selector(OnAction);

		ApplyStyle();

		auto* ParentView = Parent.GetContentView();
		[ParentView addSubview:mControl];
		Parent.OnChildAdded( Rect );
	};
	mThread.PushJob( Allocate );
}

template<typename BASETYPE>
void Platform::TTextBox_Base<BASETYPE>::SetValue(const std::string& Value)
{
	//	assuming success for immediate retrieval
	mLastValue = Value;

	mValueVersion++;
	auto Version = mValueVersion;

	auto Exec = [=]
	{
		//	updating the UI is expensive, and in some cases we're calling it a lot
		//	sometimes this is 20ms (maybe vsync?), so lets only update if we're latest in the queue
		//	we also want to flush out the queue for callbacks that do JS stuff
		//	out of date
		if ( Version != this->mValueVersion )
			return;

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


void Platform::TLabel::ApplyStyle()
{
	//	todo: check in thread
	[mControl setBezeled:NO];
	[mControl setDrawsBackground:NO];
	[mControl setEditable:NO];
	[mControl setSelectable:NO];
}


Platform::TColourButton::TColourButton(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t>& Rect) :
	TNsView		( Thread )
{
	//	move this to constrctor
	auto Allocate = [this,Rect,&Parent]()
	{
		auto ChildRect = Parent.GetChildRect( Rect );
		mControl = [[NSColorWell alloc] initWithFrame:ChildRect];
		[mControl retain];

		//	setup callback
		mResponder->mCallback = [this]()	{	this->OnChanged(true);	};
		mControl.target = mResponder;
		mControl.action = @selector(OnAction);

		//[mControl setButtonType:NSSwitchButton];
		//[mControl setBezelStyle:0];

		auto* ParentView = Parent.GetContentView();
		[ParentView addSubview:mControl];
		Parent.OnChildAdded( Rect );
	};
	mThread.PushJob( Allocate );
}

void Platform::TColourButton::SetValue(vec3x<uint8_t> Rgb)
{
	auto Exec = [=]
	{
		mControl.color = GetColour(Rgb);
	};
	mThread.PushJob( Exec );
}

void Platform::TNsView::SetRect(const Soy::Rectx<int32_t>& Rect)
{
	auto NewRect = NSMakeRect( Rect.x, Rect.y, Rect.w, Rect.h );

	auto Exec = [=]
	{
		auto mControl = GetControl();
		mControl.frame = NewRect;
	};

	mThread.PushJob(Exec);
}



Platform::TImageMap::TImageMap(PopWorker::TJobQueue& Thread,TWindow& Parent,Soy::Rectx<int32_t> Rect) :
	TNsView		( Thread )
{
	//	move this to constrctor
	auto Allocate = [this,Rect,&Parent]()
	{
		auto ChildRect = Parent.GetChildRect( Rect );
		mControl = [[NSImageView alloc] initWithFrame:ChildRect];
		[mControl retain];

		//	image map stretches
		[mControl setImageScaling:NSImageScaleAxesIndependently];

		//	setup callback
		//mResponder->mCallback = [this]()	{	this->OnChanged(true);	};
		mControl.target = mResponder;
		mControl.action = @selector(OnAction);

		//[mControl setButtonType:NSSwitchButton];
		//[mControl setBezelStyle:0];

		auto* ParentView = Parent.GetContentView();
		[ParentView addSubview:mControl];
		Parent.OnChildAdded( Rect );
	};
	mThread.PushJob( Allocate );
}

Platform::TImageMap::~TImageMap()
{
	[mControl release];

	FreeImage();
}


void Platform::TImageMap::FreeImage()
{
	if ( mCgImage )
	{
		CGImageRelease(mCgImage);
		mCgImage = nullptr;
	}

	if ( mNsImage )
	{
		[mNsImage release];
		mNsImage = nullptr;
	}
}

void GetColourSpace(CGColorSpaceRef& ColourSpace,CGBitmapInfo& Flags,SoyPixelsFormat::Type Format)
{
	switch(Format)
	{
		case SoyPixelsFormat::Greyscale:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaNone;
			ColourSpace = CGColorSpaceCreateDeviceGray();
			return;

		case SoyPixelsFormat::GreyscaleAlpha:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast;
			ColourSpace = CGColorSpaceCreateDeviceGray();
			return;

		case SoyPixelsFormat::RGB:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaNone;
			ColourSpace = CGColorSpaceCreateDeviceRGB();
			return;

		case SoyPixelsFormat::RGBA:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast;
			ColourSpace = CGColorSpaceCreateDeviceRGB();
			return;

		case SoyPixelsFormat::ARGB:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedFirst;
			ColourSpace = CGColorSpaceCreateDeviceRGB();
			return;
	}

	std::stringstream Error;
	Error << "Unhandled format(" << magic_enum::enum_name(Format) << ") to convert to CGColour space (options are gray or rgb)";
	throw Soy::AssertException(Error);
}

void Platform::TImageMap::SetImage(const SoyPixelsImpl& _Pixels)
{
	//	make a copy as we're copying to a thread, and re-using the data for CGDataProviderCreateWithData
	//	todo: optimise this so we re-use the same buffer and maybe can just refresh the view with new pixels?
	mPixelsImage.Copy(_Pixels);

	auto Run = [this]() mutable
	{
		if ( !mControl )
			throw Soy::AssertException("Control hasn't been created yet");

		auto& Pixels = mPixelsImage;
		FreeImage();

		auto PixelMeta = Pixels.GetMeta();
		auto& PixelArray = Pixels.GetPixelsArray();
		CGDataProviderRef provider = CGDataProviderCreateWithData( nullptr, PixelArray.GetArray(), PixelArray.GetDataSize(), nullptr );
		size_t bitsPerComponent = 8 * PixelMeta.GetBytesPerChannel();
		size_t bitsPerPixel = 8 * PixelMeta.GetPixelDataSize();
		size_t bytesPerRow = PixelMeta.GetRowDataSize();

		CGBitmapInfo bitmapInfo = 0;
		CGColorSpaceRef colorSpaceRef = nullptr;
		GetColourSpace( colorSpaceRef, bitmapInfo, PixelMeta.GetFormat() );
		CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;

		mCgImage = CGImageCreate( PixelMeta.GetWidth(),
										PixelMeta.GetHeight(),
										bitsPerComponent,
										bitsPerPixel,
										bytesPerRow,
										colorSpaceRef,
										bitmapInfo,
										provider,   // data provider
										NULL,       // decode
										YES,        // should interpolate
										renderingIntent);
		if ( !mCgImage )
		{
			throw Soy::AssertException("Failed to create CGImage");
		}

		mNsImage = [[NSImage alloc] initWithCGImage:mCgImage size:NSMakeSize( PixelMeta.GetWidth(), PixelMeta.GetHeight() )];
		auto ImageValid = [mNsImage isValid];

		//mControl.frame = NSMakeRect( 0,0,100,100);
		//mControl.layer.backgroundColor = CGColorCreateGenericRGB(1,0,0,1);
		//mControl.image = mNsImage;
		[mControl setImage:mNsImage];

		//	gr: this selector is going missing?? (but succeeds)
		NSImageView* pControl = mControl;
		auto ViewIsValid = true;
		//auto ViewIsValid = [pControl isValid];
		//ViewIsValid = [pControl isValid];
		//auto ViewIsValid = [pControl isValid];
		if ( !ViewIsValid )
			throw Soy::AssertException("NSViewImage is not valid");
	};
	mThread.PushJob( Run );
}

void Platform::TImageMap::SetCursorMap(const SoyPixelsImpl& CursorMap,const ArrayBridge<std::string>&& CursorIndexes)
{

}

NSWindow* Platform::TWindow::GetWindow()
{
	auto* App = [NSApplication sharedApplication];
	// tsdk: An App can have multiple windows represented in an array, the first member of this array will always? be the main window
	auto* Window = [[App windows] objectAtIndex:0];
	return Window;
}

NSView* Platform::TWindow::GetChild(const std::string& Name)
{
	NSView* ChildMatch = nullptr;
	auto TestChild = [&](NSView* Child)
	{
		//	tsdk: cannot find way to get view based on name so duplicate the name in the accessibility Identifier in the xib file and then match it here
		auto* AccessibilityIdentifier = Child.accessibilityIdentifier;
		if ( AccessibilityIdentifier == nil )
			return true;
		
		auto RestorationIdString = Soy::NSStringToString(AccessibilityIdentifier);
		if ( RestorationIdString != Name )
			return true;
		
		//	found match!
		ChildMatch = Child;
		return false;
	};
	EnumChildren(TestChild);
	return ChildMatch;
}

bool RecurseNSViews(NSView* View,std::function<bool(NSView*)>& EnumView);

bool RecurseNSViews(NSView* View,std::function<bool(NSView*)>& EnumView)
{
	if ( !EnumView(View) )
	return false;
	
	auto* Array = View.subviews;
	auto Size = [Array count];
	for ( auto i=0;	i<Size;	i++ )
	{
		auto Element = [Array objectAtIndex:i];
		if ( !RecurseNSViews( Element, EnumView ) )
		return false;
	}
	
	return true;
}

void Platform::TWindow::EnumChildren(std::function<bool(NSView*)> EnumChild)
{
	auto* Window = GetWindow();
	// tsdk: in the ios code UIWindow derives from UIView, this is not the case with NSView and NSWindow
	// so call contentView from the documentation => "The window’s content view, the highest accessible NSView object in the window’s view hierarchy."
	RecurseNSViews( [Window contentView], EnumChild );
}

class Platform::TMetalView : public SoyMetalView
{
public:
	TMetalView(NSView* View);
	
	MTKView*					mMTKView = nullptr;
	id<MTLDevice>				mtl_device;
};

std::shared_ptr<SoyMetalView> Platform::GetMetalView(SoyWindow& Parent, const std::string& Name)
{
	std::shared_ptr<SoyMetalView> MetalView;
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		MetalView.reset( new Platform::TMetalView(View) );
	};
	RunJobOnMainThread( Run, true );
	return MetalView;
}

Platform::TMetalView::TMetalView(NSView* View)
{
	//	todo: check type!
	mMTKView = View;
	mtl_device = MTLCreateSystemDefaultDevice();
	[mMTKView setDevice: mtl_device];
	
}
