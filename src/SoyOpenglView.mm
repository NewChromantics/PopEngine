#import "SoyOpenGLView.h"
#import "SoyOpenGLWindow.h"
#include <SoyMath.h>

static bool DoCGLLock = true;

vec2f ViewPointToVector(NSView* View,const NSPoint& Point)
{
	vec2f Position( Point.x, Point.y );
	
	//	invert y - osx is bottom-left/0,0 so flipped would be what we want. hence wierd flipping when not flipped
	if ( !View.isFlipped )
	{
		auto Rect = NSRectToRect( View.bounds );
		Position.y = Rect.h - Position.y;
	}
	
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
	mContext->mOnJobPushed.AddListener( OnJobPushed );
	
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


@implementation MacOpenglView


-(void)mouseDown:(NSEvent *)event
{
	if ( !Soy::Assert(mParent,"Parent expected") )
		return;

	Soy::Platform::PushCursor(SoyCursor::Hand);
	auto Pos = ViewPointToVector( self, event.locationInWindow );
	mParent->mOnMouseDown.OnTriggered( Pos );
	mLastPos = event.locationInWindow;
}

-(void)mouseDragged:(NSEvent *)event
{
	if ( !Soy::Assert(mParent,"Parent expected") )
		return;

	auto Pos = ViewPointToVector( self, event.locationInWindow );
	mParent->mOnMouseMove.OnTriggered( Pos );
	mLastPos = event.locationInWindow;
}

-(void)mouseUp:(NSEvent *)event
{
	if ( !Soy::Assert(mParent,"Parent expected") )
		return;

	auto Pos = ViewPointToVector( self, mLastPos );
	mParent->mOnMouseUp.OnTriggered( Pos );
	Soy::Platform::PopCursor();
}


- (id)initFrameWithParent:(TOpenglView*)Parent viewRect:(NSRect)viewRect pixelFormat:(NSOpenGLPixelFormat*)pixelFormat;
{
	self = [super initWithFrame:viewRect pixelFormat: pixelFormat];
	if (self)
	{
		mParent = Parent;
	}
	return self;
}


-(void) drawRect: (NSRect) bounds
{
	//	if this is slow, the OS is slow.
	//ofScopeTimerWarning DrawRect(__func__,20);
	
	//	lock the context as we do iteration from the main thread
	//	gr: maybe have a specific thread for this, as this view-redraw is called from our own thread anyway
	auto& Context = mParent->mContext;
	auto LockContext = [&Context]
	{
		Context->Lock();
		Opengl::IsOkay("pre drawRect flush",false);
	};
	auto UnlockContext = [&Context]
	{
		Opengl::IsOkay("Post drawRect flush",false);
		Context->Unlock();
	};
	auto ContextLock = SoyScope( LockContext, UnlockContext );
	
	
	if ( !mParent )
	{
		Opengl::ClearColour( Soy::TRgb(1,0,0) );
		return;
	}
	
	//	do parent's minimal render
	mParent->mRenderTarget.mRect = NSRectToRect( bounds );
	mParent->mRenderTarget.Bind();
	
	//Opengl::ClearColour( Soy::TRgb(0,1,0) );
	mParent->mOnRender.OnTriggered( mParent->mRenderTarget );
	mParent->mRenderTarget.Unbind();

	
	
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
	}
	
}

@end


void GlViewRenderTarget::Bind()
{
	//	gr: maybe need to work out how to bind to the default render target rather than relying on others to unbind
}


void GlViewRenderTarget::Unbind()
{
}


bool GlViewContext::Lock()
{
	if ( !mParent.mView )
		return false;
	
	auto mContext = [mParent.mView.openGLContext CGLContextObj];
	if ( DoCGLLock )
		CGLLockContext( mContext );
	
	//	make current thread
	auto CurrentContext = CGLGetCurrentContext();
	if ( CurrentContext != mContext )
	{
		auto Error = CGLSetCurrentContext( mContext );
		if ( !Soy::Assert( Error == kCGLNoError, "Error setting current context" ) )
			return false;
	}
	
	return Opengl::TContext::Lock();
}

void GlViewContext::Unlock()
{
	auto ContextObj = [mParent.mView.openGLContext CGLContextObj];
	if ( DoCGLLock )
		CGLUnlockContext( ContextObj );
//	leaves artifacts everywhere
	//[mParent.mView.openGLContext flushBuffer];
	
	Opengl::TContext::Unlock();
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

bool GlViewSharedContext::Lock()
{
	if ( DoCGLLock )
		CGLLockContext( mContext );

	//	make current thread
	auto CurrentContext = CGLGetCurrentContext();
	if ( CurrentContext != mContext )
	{
		auto Error = CGLSetCurrentContext( mContext );
		if ( !Soy::Assert( Error == kCGLNoError, "Error setting current context" ) )
			return false;
	}
	return true;
}

void GlViewSharedContext::Unlock()
{
	if ( DoCGLLock )
		CGLUnlockContext( mContext );
}


