#include "TApiSokol.h"
#include "SoyWindowIos.h"
#include "PopMain.h"

#include "SoySokol_Ios_Gles.h"


//	include a GLES implementation
#define SOKOL_IMPL
//#define SOKOL_GLES2
#define SOKOL_GLES3

#if defined(SOKOL_GLES3)
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#endif

#include "sokol/sokol_gfx.h"






@implementation SokolViewDelegate_Gles
	
- (instancetype)init:(std::function<void(CGRect)> )OnPaint
{
	self = [super init];
	self.mOnPaint = OnPaint;
	return self;
}
	
- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
	self.mOnPaint(rect);
}
	
@end







SokolOpenglContext::SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLKView* View,Sokol::TContextParams Params) :
	mView	( View ),
	mParams	( Params )
{
	auto OnFrame = [this](CGRect Rect)
	{
		auto* CurrentContext = [EAGLContext currentContext];
		if ( CurrentContext != mOpenglContext )
			[EAGLContext setCurrentContext:mOpenglContext];

		if ( mSokolContext.id == 0 )
		{
			sg_desc desc={0};
			sg_setup(&desc);
			mSokolContext = sg_setup_context();
		}
		
		//std::Debug << __PRETTY_FUNCTION__ << "(" << Rect.origin.x << "," << Rect.origin.y << "," << Rect.size.width << "," << Rect.size.height << ")" << std::endl;
		
		glClearColor(0,1,1,1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		
		vec2x<size_t> Size( Rect.size.width, Rect.size.height );
		mParams.mOnPaint( mSokolContext, Size );
	};

	//	can we get this from sokol impl?
#if defined(SOKOL_GLES2)
	auto Api = kEAGLRenderingAPIOpenGLES2;
#elif defined(SOKOL_GLES3)
	auto Api = kEAGLRenderingAPIOpenGLES3;
#else
#pragma message Unknown GL API selected
	auto Api = kEAGLRenderingAPIOpenGLES1;
#endif
	mOpenglContext = [[EAGLContext alloc] initWithAPI:Api];
	mView.context = mOpenglContext;

	//	gr: this doesn't do anything, need to call the func
	mView.enableSetNeedsDisplay = YES;

	mDelegate = [[SokolViewDelegate_Gles alloc] init:OnFrame];
	[mView setDelegate:mDelegate];


	/*gr: this still isn't triggering, I think it needs to be in the view tree
	 but Interface Builder won't let us add this, and I can't see how (as its not a view)...
	//	built in auto-renderer
	mViewController = [[GLKViewController alloc] init];
	[mViewController setView:mView];
	mViewController.preferredFramesPerSecond = FrameRate;
	[mViewController setDelegate:mDelegate];
*/
	/*
	mContextDesc.color_format = _SG_PIXELFORMAT_DEFAULT;
	mContextDesc.depth_format = _SG_PIXELFORMAT_DEFAULT;
	mContextDesc.sample_count = 1;
	mContextDesc.gl.force_gles2 = true;
	*/
	
	//	gr: given that TriggerPaint needs to be on the main thread,
	//		maybe this thread should just something on the main dispath queue
	//		that could be dangerous for deadlocks on destruction though
	auto PaintLoop = [this]()
	{
		auto FrameDelayMs = 1000/mParams.mFramesPerSecond;
		this->TriggerPaint();
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

extern void RunJobOnMainThread(std::function<void()> Lambda,bool Block);

void SokolOpenglContext::TriggerPaint()
{
	//	must be on UI thread, we should be queuing this up on the main window, maybe?
	//	gr: this is a single Dirty-Rect call
	//	a GLViewController will do regular drawing for us
	auto SetNeedDisplay = [this]()
	{
		[mView setNeedsDisplay];
	};
	RunJobOnMainThread(SetNeedDisplay,false);
}
