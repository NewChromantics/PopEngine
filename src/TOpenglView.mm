#import "TOpenGLView.h"
#include <SoyMath.h>


TOpenglView::TOpenglView(vec2f Position,vec2f Size) :
	mView			( nullptr ),
	mRenderTarget	( "osx gl view" ),
	mContext		( *this )
{
	//	gr: for device-specific render choices..
	//	https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_pixelformats/opengl_pixelformats.html#//apple_ref/doc/uid/TP40001987-CH214-SW9
	//	make "pixelformat" (context params)
	NSOpenGLPixelFormatAttribute attrs[] =
	{
		//	hardware ONLY
		NSOpenGLPFAAccelerated,
		//NSOpenGLPFANoRecovery,

		//	gr: lets get rid of double buffering
		NSOpenGLPFADoubleBuffer,
		
		//	enable alpha for FBO's
		//NSOpenGLPFASampleAlpha,
		//NSOpenGLPFAColorFloat,
		NSOpenGLPFAAlphaSize, 8,
		NSOpenGLPFAColorSize, 32,
		
		//	require 3.2 to enable some features without using extensions (eg. glGenVertexArrays)
		NSOpenGLPFAOpenGLProfile,
		NSOpenGLProfileVersion3_2Core,
		0
	};
	NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
	Soy::Assert( pixelFormat, "Failed to create pixel format" );
 
	NSRect viewRect = NSMakeRect( Position.x, Position.y, Size.x, Size.y );
	mView = [[MacOpenglView alloc] initFrameWithParent:this viewRect:viewRect pixelFormat:pixelFormat];
	[mView retain];
	Soy::Assert( mView, "Failed to create view" );
	
	//	enable multi-threading
	//	this places the render callback on the calling thread rather than main
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
	
	//	do init on first thread run
	auto DefferedInit = [this]
	{
		this->mContext.Init();
		return true;
	};
	mContext.PushJob( DefferedInit );
}

TOpenglView::~TOpenglView()
{
	[mView release];
}




@implementation MacOpenglView


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
	//	render callback from OS, always on main thread?
	if ( !mParent )
	{
		Opengl::ClearColour( Soy::TRgb(1,0,0) );
		return;
	}
	
	//	do parent's minimal render
	auto ParentRender = [self,bounds]()
	{
		mParent->mRenderTarget.mRect.x = bounds.origin.x;
		mParent->mRenderTarget.mRect.y = bounds.origin.y;
		mParent->mRenderTarget.mRect.w = bounds.size.width;
		mParent->mRenderTarget.mRect.h = bounds.size.height;
		if ( !mParent->mRenderTarget.Bind() )
			return false;
		//	gr: don't really wanna send the context here I don't think.... probably wanna send render target
		//Opengl::ClearColour( Soy::TRgb(0,1,0) );
		mParent->mOnRender.OnTriggered( mParent->mRenderTarget );
		mParent->mRenderTarget.Unbind();
		
		glFlush();

		//	swap OSX buffers - required with os double buffering (NSOpenGLPFADoubleBuffer)
		[[self openGLContext] flushBuffer];
		return true;
	};

	//	queue render so when we iterate previous commands are flushed first
	auto& Context = mParent->mContext;
	//Context.PushJob( ParentRender );
	//	gr: need to render immediately whilst render target is bound... which is currently out of our control
	ParentRender();

	//	run other jobs
	//	gr: shouln't need this, it should be queued in the runloop from PushJob() now
	Context.Iteration();
}

@end


bool GlViewRenderTarget::Bind()
{
	//	gr: maybe need to work out how to bind to the default render target rather than relying on others to unbind
	return true;
}


void GlViewRenderTarget::Unbind()
{
}

bool GlViewContext::Lock()
{
	if ( !mParent.mView )
		return false;
	
	auto ContextObj = [mParent.mView.openGLContext CGLContextObj];
	CGLLockContext( ContextObj );
	
	//	should already been on this thread...
	[mParent.mView.openGLContext makeCurrentContext];
	return true;
}

void GlViewContext::Unlock()
{
	auto ContextObj = [mParent.mView.openGLContext CGLContextObj];
	CGLUnlockContext( ContextObj );
//	leaves artifacts everywhere
	//[mParent.mView.openGLContext flushBuffer];
}

void GlViewContext::InternalPushJob(std::shared_ptr<Opengl::TJob>& Job,Opengl::TJobSempahore* Semaphore)
{
	Opengl::TContext::InternalPushJob( Job, Semaphore );
	
	//	wake up the runloop to make sure an iteration is done ratehr than waiting for OS to redraw
	WakeThread();
}


void GlViewContext::WakeThread()
{
	
}
