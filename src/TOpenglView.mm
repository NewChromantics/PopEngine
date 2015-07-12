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
		NSOpenGLPFAAccelerated,		//	hardware only
		NSOpenGLPFADoubleBuffer,
		
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
	
	//	lock the context
	auto& Context = mParent->mContext;
	
	//	gr: assuming here it's already switched??
	Context.Lock();
	/*
	GLint RenderBufferName = -1;
	glGetIntegerv( GL_RENDERBUFFER_BINDING, &RenderBufferName );
	Opengl::IsOkay("glget GL_RENDERBUFFER_BINDING");
	std::Debug <<"Render buffer name is " << RenderBufferName << std::endl;
	 */
	Context.Unlock();
	
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
		return true;
	};
	
	auto BufferFlip = [self]()
	{
		//	swap OSX buffers - required with os double buffering (NSOpenGLPFADoubleBuffer)
		[[self openGLContext] flushBuffer];
		return true;
	};

	
	//	Run through our contexts jobs, then do a render and a flip, by queueing correctly.
	
	Context.PushJob( ParentRender );
	//	flush all commands before another thread uses this context.... maybe put this in context unlock()
	Context.PushJob( []{glFlush();return true;} );
	Context.PushJob( BufferFlip );
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

