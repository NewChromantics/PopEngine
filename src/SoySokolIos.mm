#include "TApiSokol.h"
#include "SoyWindowIos.h"
#include "PopMain.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

//	GLKViewController needs GLKit framework
#import <GLKit/GLKit.h>

#define SOKOL_IMPL
#define SOKOL_GLES2
//#define SOKOL_METAL
#include "sokol/sokol_gfx.h"



// Sokol

//@interface SokolViewDelegate : NSObject<MTKViewDelegate>
//
//@property std::function<void()> Frame;
//- (instancetype)init:(std::function<void()> )Frame;
//
//@end
//
//@implementation SokolViewDelegate
//
//- (instancetype)init:(std::function<void()> )Frame {
//    self = [super init];
//    if (self) {
//        _Frame = Frame;
//    }
//    return self;
//}
//
//- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
//    (void)view;
//    (void)size;
//    // FIXME
//}
//
//- (void)drawInMTKView:(nonnull MTKView*)view {
//    (void)view;
//    @autoreleasepool {
//			_Frame();
//    }
//}

//@end

//	this could do metal & gl
@interface SokolViewDelegate : UIResponder<GLKViewDelegate,GLKViewControllerDelegate>

	@property std::function<void(CGRect)>	mOnPaint;

- (instancetype)init:(std::function<void(CGRect)> )OnPaint;
	@end

@implementation SokolViewDelegate
	
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
	
- (void)glkViewControllerUpdate:(nonnull GLKViewController *)controller
{
	std::Debug << "glkViewControllerUpdate" << std::endl;
	
}
	
@end



class SokolMetalContext : public Sokol::TContext
{
public:
	SokolMetalContext(std::shared_ptr<SoyWindow> Window,MTKView* View,Sokol::TContextParams Params);

	sg_context_desc					GetSokolContext() override	{	return mContextDesc;	}

public:
	sg_context_desc         		mContextDesc;
	MTKView*             			mView = nullptr;
	id<MTLDevice>         			mMetalDevice;
	
	Sokol::TContextParams			mParams;
};

class SokolOpenglContext : public Sokol::TContext
{
public:
	SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLKView* View,Sokol::TContextParams Params);
	~SokolOpenglContext();
	
	sg_context_desc					GetSokolContext() override	{	return mContextDesc;	}
	
	void							TriggerPaint();
	
public:
	bool							mRunning = true;
	std::shared_ptr<SoyThread>		mPaintThread;
	sg_context_desc         		mContextDesc;
	GLKView*             			mView = nullptr;
	EAGLContext*					mOpenglContext = nullptr;
	SokolViewDelegate*				mDelegate = nullptr;
	GLKViewController*				mViewController = nullptr;
	
	Sokol::TContextParams			mParams;
};



std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(std::shared_ptr<SoyWindow> Window,Sokol::TContextParams Params)
{
	auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);
	
	//	get the view with matching name, if it's a metal view, make a metal context
	//	if its gl, make a gl context
	auto* View = PlatformWindow.GetChild(Params.mViewName);
	if ( !View )
	{
		std::stringstream Error;
		Error << "Failed to find child view " << Params.mViewName << " (required on ios)";
		throw Soy::AssertException(Error);
	}
	
	auto ClassName = Soy::NSStringToString(NSStringFromClass([View class]));
	std::Debug << "View " << Params.mViewName << " class name " << ClassName << std::endl;
	
	if ( ClassName == "MTKView" )
	{
		MTKView* MetalView = (MTKView*)View;
		auto* Context = new SokolMetalContext(Window,MetalView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
	
	if ( ClassName == "GLKView" )
	{
		GLKView* GlView = (GLKView*)View;
		auto* Context = new SokolOpenglContext(Window,GlView,Params);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
	
	std::stringstream Error;
	Error << "Class of view " << Params.mViewName << " is not MTKView or GLKView; " << ClassName;
	throw Soy::AssertException(Error);
}






SokolMetalContext::SokolMetalContext(std::shared_ptr<SoyWindow> Window,MTKView* View,Sokol::TContextParams Params) :
	mView	( View ),
	mParams	( Params )
{
	mMetalDevice = MTLCreateSystemDefaultDevice();
 	[mView setDevice: mMetalDevice];
	 
	auto GetRenderPassDescriptor = [](void* user_data)
	{
		auto* This = reinterpret_cast<SokolMetalContext*>(user_data);
		auto* MetalView = This->mView;
		auto* Descriptor = (__bridge const void*) [MetalView currentRenderPassDescriptor];
		return Descriptor;
	};
	
	auto GetDrawable = [](void* user_data)
	{
		auto* This = reinterpret_cast<SokolMetalContext*>(user_data);
		auto* MetalView = This->mView;
		auto* Drawable = (__bridge const void*) [MetalView currentDrawable];
		return Drawable;
	};

	mContextDesc = (sg_context_desc)
	{
		.sample_count = 1,//mParams.mSampleCount,
		.metal =
		{
			.device= (__bridge const void*) mMetalDevice,
			.renderpass_descriptor_userdata_cb = GetRenderPassDescriptor,
			.drawable_userdata_cb = GetDrawable,
			.user_data = this
		}
	};

}


SokolOpenglContext::SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLKView* View,Sokol::TContextParams Params) :
	mView	( View ),
	mParams	( Params )
{
	auto OnFrame = [this](CGRect Rect)
	{
		/*
		static int Counter = 0;
		Counter++;
		std::Debug << __PRETTY_FUNCTION__ << "(" << Rect.origin.x << "," << Rect.origin.y << "," << Rect.size.width << "," << Rect.size.height << ")" << std::endl;
		auto Blue = (Counter%60)/60.0f;
		glClearColor(1.f, 0.f, Blue, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		*/
		vec2x<size_t> Size( Rect.size.width, Rect.size.height );
		mParams.mOnPaint(Size);
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

	mDelegate = [[SokolViewDelegate alloc] init:OnFrame];
	[mView setDelegate:mDelegate];


	/*gr: this still isn't triggering, I think it needs to be in the view tree
	 but Interface Builder won't let us add this, and I can't see how (as its not a view)...
	//	built in auto-renderer
	mViewController = [[GLKViewController alloc] init];
	[mViewController setView:mView];
	mViewController.preferredFramesPerSecond = FrameRate;
	[mViewController setDelegate:mDelegate];
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
