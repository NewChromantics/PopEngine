#import "SoyOpenglView.h"
#import "SoyOpenglWindow.h"
#include "SoyMath.h"


namespace Platform
{
	SoyKeyButton::Type	GetKeyButton(uint16_t KeyCode);
}


SoyKeyButton::Type Platform::GetKeyButton(uint16_t KeyCode)
{
	//	<HIToolbox/Events.h>
	switch(KeyCode)
	{
		//	gr: these will vary by keyboard layout
		//case kVK_ANSI_A:	return 'a';	//
		//case kVK_Space:	return ' ';
		case 0x31:	return ' ';
		case 0x12:	return '1';
		case 0x13:	return '2';
		case 0x14:	return '3';
		case 0x15:	return '4';
		case 0x16:	return '6';
		case 0x17:	return '5';
		case 0x1a:	return '7';
		case 0x1c:	return '8';
		case 0x19:	return '9';
		case 0x1d:	return '0';
			
		case 0x3:	return 'f';
	}
	
	std::stringstream Error;
 	Error << "Unhandled OSX keycode 0x" << std::hex << KeyCode;
	throw Soy::AssertException(Error.str());
}



//	gr; we don't use the CG lock as we have our own internal checks, which should now be locked with our own mutex
//		this CGLLockContext just implements recursive mutex internally, so redundant if we have our own
//		we were getting asserts in Opengl::Context::Lock as the thread id was being released at a different time to unlocking the context (race condition)
//	https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_threading/opengl_threading.html#//apple_ref/doc/uid/TP40001987-CH409-SW7
static bool DoCGLLock = false;

vec2x<int32_t> ViewPointToVector(NSView* View,const NSPoint& Point)
{
	vec2x<int32_t> Position( Point.x, Point.y );
	
	//	invert y - osx is bottom-left/0,0 so flipped would be what we want. hence wierd flipping when not flipped
	if ( !View.isFlipped )
	{
		auto Rect = NSRectToRect( View.bounds );
		Position.y = Rect.h - Position.y;
	}
	
	return Position;
}

vec2f ViewPointToVectorNormalised(NSView* View,const NSPoint& Point)
{
	vec2f Position( Point.x, Point.y );
	auto Rect = NSRectToRect( View.bounds );
	
	//	invert y - osx is bottom-left/0,0 so flipped would be what we want. hence wierd flipping when not flipped
	if ( !View.isFlipped )
	{
		auto Rect = NSRectToRect( View.bounds );
		Position.y = Rect.h - Position.y;
	}

	Position.x /= Rect.w;
	Position.y /= Rect.h;
	
	return Position;
}


TOpenglView::TOpenglView(vec2f Position,vec2f Size,const TOpenglParams& Params) :
	mView			( nullptr ),
	mRenderTarget	( "osx gl view" ),
	mContext		( new GlViewContext(*this) )
{
	//	gr: for device-specific render choices..
	//	https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_pixelformats/opengl_pixelformats.html#//apple_ref/doc/uid/TP40001987-CH214-SW9
	//	make "pixelformat" (context params)
	Array<NSOpenGLPixelFormatAttribute> Attributes;
	
	if ( Params.mHardwareAccellerationOnly )
	{
		Attributes.PushBack( NSOpenGLPFAAccelerated );
		Attributes.PushBack( NSOpenGLPFANoRecovery );
	}
	
	if ( Params.mDoubleBuffer )
	{
		Attributes.PushBack( NSOpenGLPFADoubleBuffer );
	}
	
	//	enable alpha for FBO's
	//NSOpenGLPFASampleAlpha,
	//NSOpenGLPFAColorFloat,
	Attributes.PushBack( NSOpenGLPFAAlphaSize );
	Attributes.PushBack( 8 );
	Attributes.PushBack( NSOpenGLPFAColorSize );
	Attributes.PushBack( 32 );

	//	require 3.2 to enable some features without using extensions (eg. glGenVertexArrays)
	Attributes.PushBack( NSOpenGLPFAOpenGLProfile );
	Attributes.PushBack( NSOpenGLProfileVersion3_2Core );
	
	//	terminator
	Attributes.PushBack(0);
	
	NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:Attributes.GetArray()];
	Soy::Assert( pixelFormat, "Failed to create pixel format" );
 
	NSRect viewRect = NSMakeRect( Position.x, Position.y, Size.x, Size.y );
	mView = [[MacOpenglView alloc] initFrameWithParent:this viewRect:viewRect pixelFormat:pixelFormat];
	[mView retain];
	Soy::Assert( mView, "Failed to create view" );
	
	//	enable multi-threading
	//	this places the render callback on the calling thread rather than main
	//	gr: errrr what callback did I mean?? the context switching is manual...
	//		maybe it disables the need for context switching
	static bool EnableMultithreading = true;
	if ( EnableMultithreading )
	{
		CGLContextObj Context = [mView.openGLContext CGLContextObj];
		auto Error = CGLEnable( Context, kCGLCEMPEngine);
		if ( Error == kCGLNoError )
			std::Debug << "Opengl multithreading enabled" << std::endl;
		else
			std::Debug << "Opengl multithreading not enabled" << std::endl;
	}
	
	//	sync with vsync
	GLint swapInt = Params.mVsyncSwapInterval;
	[mView.openGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
	
	//	wake thread when there are new jobs
	auto OnJobPushed = [this](std::shared_ptr<PopWorker::TJob>&)
	{
		mContext->WakeThread();
	};
	mContext->mOnJobPushed = OnJobPushed;
	
	//	do init on first thread run
	auto DefferedInit = [this]
	{
		this->mContext->Init();
	};
	mContext->PushJob( DefferedInit );

}

TOpenglView::~TOpenglView()
{
	[mView release];
}

bool TOpenglView::IsDoubleBuffered() const
{
	if ( !mView )
		return false;
	GLint IsDoubleBuffered = 0;
	GLint VirtualScreenIndex = 0;
	[[mView pixelFormat]getValues:&IsDoubleBuffered forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:VirtualScreenIndex];
	return IsDoubleBuffered!=0;
}

Soy::Rectx<int32_t> TOpenglView::GetScreenRect()
{
	auto Bounds = [mView bounds];
	auto Rect = NSRectToRect( Bounds );
	return Rect;
}


void TriggerMouseEvent(NSEvent* EventIn,const char* EventName,TOpenglView* Parent,std::function<void(const TMousePos&,SoyMouseButton::Type)>& EventOut,SoyMouseButton::Type Button,NSPoint& LastPos,MacOpenglView* Self)
{
	if ( !Soy::Assert(Parent,"Parent expected") )
		return;
	
	//	gr: finding OS HID problem, don't let this OS callbacks take long
	Soy::TScopeTimerPrint Timer(EventName, 2);

	//	gr: sending normalised coords as we currently dont have easy acess to a window's clientrect!
	auto Pos = ViewPointToVector( Self, EventIn.locationInWindow );
	
	if ( EventOut )
		EventOut( Pos, Button );
	
	LastPos = EventIn.locationInWindow;
}


@implementation MacOpenglView


-(void)mouseMoved:(NSEvent *)event
{
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseMove, SoyMouseButton::None, mLastPos, self );
}

-(void)mouseDown:(NSEvent *)event
{
	Platform::PushCursor(SoyCursor::Hand);
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseDown, SoyMouseButton::Left, mLastPos, self );
}

-(void)mouseDragged:(NSEvent *)event
{
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseMove, SoyMouseButton::Left, mLastPos, self );
}

-(void)mouseUp:(NSEvent *)event
{
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseUp, SoyMouseButton::Left, mLastPos, self );
}


-(void)rightMouseDown:(NSEvent *)event
{
	Platform::PushCursor(SoyCursor::Hand);
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseDown, SoyMouseButton::Right, mLastPos, self );
}

-(void)rightMouseDragged:(NSEvent *)event
{
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseMove, SoyMouseButton::Right, mLastPos, self );
}

-(void)rightMouseUp:(NSEvent *)event
{
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseUp, SoyMouseButton::Right, mLastPos, self );
}


-(void)otherMouseDown:(NSEvent *)event
{
	Platform::PushCursor(SoyCursor::Hand);
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseDown, SoyMouseButton::Middle, mLastPos, self );
}

-(void)otherMouseDragged:(NSEvent *)event
{
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseMove, SoyMouseButton::Middle, mLastPos, self );
}

-(void)otherMouseUp:(NSEvent *)event
{
	TriggerMouseEvent( event, __FUNCTION__, mParent, mParent->mOnMouseUp, SoyMouseButton::Middle, mLastPos, self );
}

- (BOOL)acceptsFirstResponder
{
	//	enable key events
	return YES;
}

- (void)keyDown:(NSEvent *)event
{
	try
	{
		auto Key = Platform::GetKeyButton( event.keyCode );
		auto& Event = mParent->mOnKeyDown;
		if ( Event )
			Event( Key );
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
}

- (void)keyUp:(NSEvent *)event
{
	try
	{
		auto Key = Platform::GetKeyButton( event.keyCode );
		auto& Event = mParent->mOnKeyUp;
		if ( Event )
			Event( Key );
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	//	return the cursor to dispaly
	std::Debug << __func__ << std::endl;
	return NSDragOperationLink;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
	//	drag released
	Soy::TScopeTimerPrint Timer( __func__, 1 );
	std::Debug << __func__ << std::endl;
	
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
	std::Debug << "DragDrop( " << Soy::StringJoin( FilenamesBridge,", ") << ")" << std::endl;
	
	if ( !mParent->mOnDragDrop )
		return NO;
	
	mParent->mOnDragDrop( FilenamesBridge );
	return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
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
	
	if ( !mParent->mOnTryDragDrop )
		return NO;
	
	auto Allow = mParent->mOnTryDragDrop( FilenamesBridge );
	return Allow ? YES : NO;
}


- (id)initFrameWithParent:(TOpenglView*)Parent viewRect:(NSRect)viewRect pixelFormat:(NSOpenGLPixelFormat*)pixelFormat;
{
	self = [super initWithFrame:viewRect pixelFormat: pixelFormat];
	if (self)
	{
		mParent = Parent;
	}
	
	//	enable drag & drop
	//	https://stackoverflow.com/a/29029456
	//	https://stackoverflow.com/a/8567836	NSFilenamesPboardType
	[self registerForDraggedTypes: @[(NSString*)kUTTypeItem]];
	//[self registerForDraggedTypes:[NSImage imagePasteboardTypes]];
	//registerForDraggedTypes([NSFilenamesPboardType])
	
	//	enable mouse tracking events (enter, exit, move)
	//	https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/TrackingAreaObjects/TrackingAreaObjects.html#//apple_ref/doc/uid/10000060i-CH8-SW1
	NSTrackingAreaOptions options = (NSTrackingActiveAlways | NSTrackingInVisibleRect |
									 NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved);
	
	NSTrackingArea *area = [[NSTrackingArea alloc] initWithRect:[self bounds]
														options:options
														  owner:self
													   userInfo:nil];
	[self addTrackingArea:area];
	
	return self;
}


-(void) drawRect: (NSRect) bounds
{
	//	if this is slow, the OS is slow.
	//ofScopeTimerWarning DrawRect(__func__,20);
	
	//	lock the context as we do iteration from the main thread
	//	gr: maybe have a specific thread for this, as this view-redraw is called from our own thread anyway
	auto& Context = mParent->mContext;
	//  gr: oddly (in PopEngine at least) we now get a draw before the opengl deffered init (which is on the main thread)
	//      so lets wait for the context to initialise... we may be dropping a frame :(
	//      while this will still render, none of the extensions will be setup...
	//	gr: does this need a lock?
	if ( !Context->IsInitialised() )
		return;
	
	//	may need to check for negatives here
	auto BoundsRectf = NSRectToRect( bounds );
	Soy::Rectx<size_t> BoundsRect(BoundsRectf);
	auto& Parent = *mParent;

	bool DoneLock = false;
	auto LockContext = [&]
	{
		if ( DoneLock )
			return;
		Context->Lock();
		DoneLock = true;
		Opengl::IsOkay("pre drawRect flush",false);
		//	do parent's minimal render
		//	gr: reset state here!
		Parent.mRenderTarget.mRect = BoundsRect;
		Parent.mRenderTarget.Bind();
	};
	auto UnlockContext = [&]
	{
		Parent.mRenderTarget.Unbind();
		Opengl::IsOkay("Post drawRect flush",false);
		Context->Unlock();
	};
	
	if ( !mParent )
	{
		auto ContextLock = SoyScope( LockContext, UnlockContext );
		Opengl::ClearColour( Soy::TRgb(1,0,0) );
		return;
	}
	
    try
    {
		if ( !mParent->mOnRender )
			throw Soy::AssertException("No OnRender callback");
		
		mParent->mOnRender( mParent->mRenderTarget, LockContext );
		//Opengl::ClearColour( Soy::TRgb(0,1,0) );
    }
    catch(std::exception& e)
    {
		LockContext();
		Opengl::ClearColour( Soy::TRgb(0,0,1) );
		std::Debug << "Window OnRender Exception: " << e.what() << std::endl;
    }
	
	//	in case lock hasn't been done
	LockContext();
	
	try
	{
		if ( Context->IsDoubleBuffered() )
		{
			//	let OSX flush and flip (probably sync'd better than we ever could)
			//	only applies if double buffered (NSOpenGLPFADoubleBuffer)
			//	http://stackoverflow.com/a/13633191/355753
			[[self openGLContext] flushBuffer];
		}
		else
		{
			glFlush();
			Opengl::IsOkay("glFlush");
		}
		
		UnlockContext();
	}
	catch(std::exception& e)
	{
		UnlockContext();
		std::Debug << e.what() << std::endl;
	}
}

@end


void GlViewRenderTarget::Bind()
{
	//	gr: maybe need to work out how to bind to the default render target rather than relying on others to unbind
	Opengl::BindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_SCISSOR_TEST);
}


void GlViewRenderTarget::Unbind()
{
}

std::string GetErrorString(CGLError Error)
{
	switch ( Error )
	{
		case kCGLNoError:			return "kCGLNoError";
		case kCGLBadAttribute:		return "kCGLBadAttribute";
		case kCGLBadProperty:		return "kCGLBadProperty";
		case kCGLBadPixelFormat:	return "kCGLBadPixelFormat";
		case kCGLBadRendererInfo:	return "kCGLBadRendererInfo";
		case kCGLBadContext:		return "kCGLBadContext";
		case kCGLBadDrawable:		return "kCGLBadDrawable";
		case kCGLBadDisplay:		return "kCGLBadDisplay";
		case kCGLBadState:			return "kCGLBadState";
		case kCGLBadValue:			return "kCGLBadValue";
		case kCGLBadMatch:			return "kCGLBadMatch";
		case kCGLBadEnumeration:	return "kCGLBadEnumeration";
		case kCGLBadOffScreen:		return "kCGLBadOffScreen";
		case kCGLBadFullScreen:		return "kCGLBadFullScreen";
		case kCGLBadWindow:			return "kCGLBadWindow";
		case kCGLBadAddress:		return "kCGLBadAddress";
		case kCGLBadCodeModule:		return "kCGLBadCodeModule";
		case kCGLBadAlloc:			return "kCGLBadAlloc";
		case kCGLBadConnection:		return "kCGLBadConnection";
	}
	
	std::stringstream ValueStr;
	ValueStr << "<Unknown CGLError " << Error <<">";
	return ValueStr.str();
}


void OsxOpenglContext::Lock()
{
	//	lock then switch context's thread
	Opengl::TContext::Lock();
	try
	{
		auto mContext = GetPlatformContext();
		if ( mContext == nullptr )
			throw Soy::AssertException("GlViewContext missing context");
		
		//	gr: should never block here any more as our own context has a lock
		if ( DoCGLLock )
			CGLLockContext( mContext );
		
		//	make current thread
		auto CurrentContext = CGLGetCurrentContext();
		if ( CurrentContext != mContext )
		{
			auto Error = CGLSetCurrentContext( mContext );
			if ( Error != kCGLNoError )
			{
				std::stringstream ErrorString;
				ErrorString << "Error setting current context: " << GetErrorString(Error);
				throw Soy::AssertException(ErrorString.str());
			}
		}
	}
	catch(...)
	{
		Unlock();
		throw;
	}
}


void OsxOpenglContext::Unlock()
{
	auto ContextObj = GetPlatformContext();
	
	//	gr: this is missing
	//	gr: 2018, but I don't think we need it, we just set it as we need
	//CGLSetCurrentContext( nullptr );
	
	if ( DoCGLLock )
		CGLUnlockContext( ContextObj );
	//	leaves artifacts everywhere
	//[mParent.mView.openGLContext flushBuffer];
	
	Opengl::TContext::Unlock();
}




void GlViewContext::Lock()
{
	if ( !mParent.mView )
		throw Soy::AssertException("GlViewContext missing parent view");
	
	OsxOpenglContext::Lock();
}

void GlViewContext::WakeThread()
{
	dispatch_async( dispatch_get_main_queue(), ^(void){
		Iteration();
	});
}

bool GlViewContext::IsDoubleBuffered() const
{
	return mParent.IsDoubleBuffered();
}



std::shared_ptr<Opengl::TContext> GlViewContext::CreateSharedContext()
{
	std::shared_ptr<Opengl::TContext> SharedContext;
	
	auto* NSContext = mParent.mView.openGLContext;
	
	//	create another context
	CGLContextObj Context1 = [NSContext CGLContextObj];
	CGLContextObj Context2;
	auto PixelFormat = NSContext.pixelFormat.CGLPixelFormatObj;
	auto Error = CGLCreateContext( PixelFormat, Context1, &Context2 );
	
	if ( Error != kCGLNoError )
	{
		std::stringstream ErrorString;
		ErrorString << "Failed to create shared context: " << Error;
		throw Soy::AssertException( ErrorString.str() );
		return nullptr;
	}
	
	SharedContext.reset( new GlViewSharedContext(Context2) );
	return SharedContext;
}


CGLContextObj GlViewContext::GetPlatformContext()
{
	auto* View = mParent.mView;
	if ( !Soy::Assert( View!=nullptr, "Expected view" ) )
		return nullptr;

	auto Context = [View.openGLContext CGLContextObj];
	return Context;
}



GlViewSharedContext::GlViewSharedContext(CGLContextObj NewContextHandle) :
	SoyWorkerThread	( "Shared opengl context", SoyWorkerWaitMode::Wake ),
	mContext		( NewContextHandle )
{
	//	do init on first thread run
	auto DefferedInit = [this]
	{
		Init();
	};
	PushJob( DefferedInit );

	WakeOnEvent( mOnJobPushed );

	Start();
}

GlViewSharedContext::~GlViewSharedContext()
{
	
}
