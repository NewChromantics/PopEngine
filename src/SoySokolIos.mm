#include "TApiSokol.h"
#include "SoyWindowIos.h"
#include "PopMain.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

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
@interface SokolViewDelegate : UIResponder<GLKViewDelegate>

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
	
@end



class SokolMetalContext : public Sokol::TContext
{
public:
	SokolMetalContext(std::shared_ptr<SoyWindow> Window,MTKView* View,int SampleCount);

	sg_context_desc					GetSokolContext() override	{	return mContextDesc;	}

public:
	sg_context_desc         		mContextDesc;
	MTKView*             			mView = nullptr;
	id<MTLDevice>         			mMetalDevice;
};

class SokolOpenglContext : public Sokol::TContext
{
public:
	SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLKView* View,int SampleCount);
	
	sg_context_desc					GetSokolContext() override	{	return mContextDesc;	}
	
public:
	sg_context_desc         		mContextDesc;
	GLKView*             			mView = nullptr;
	EAGLContext*					mOpenglContext = nullptr;
	SokolViewDelegate*				mDelegate = nullptr;
	GLKViewController*				mViewController = nullptr;
};



std::shared_ptr<Sokol::TContext> Sokol::Platform_CreateContext(std::shared_ptr<SoyWindow> Window,const std::string& ViewName,int SampleCount)
{
	auto& PlatformWindow = dynamic_cast<Platform::TWindow&>(*Window);
	
	//	get the view with matching name, if it's a metal view, make a metal context
	//	if its gl, make a gl context
	auto* View = PlatformWindow.GetChild(ViewName);
	if ( !View )
	{
		std::stringstream Error;
		Error << "Failed to find child view " << ViewName << " (required on ios)";
		throw Soy::AssertException(Error);
	}
	
	auto ClassName = Soy::NSStringToString(NSStringFromClass([View class]));
	std::Debug << "View " << ViewName << " class name " << ClassName << std::endl;
	
	if ( ClassName == "MTKView" )
	{
		MTKView* MetalView = (MTKView*)View;
		auto* Context = new SokolMetalContext(Window,MetalView,SampleCount);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
	
	if ( ClassName == "GLKView" )
	{
		GLKView* GlView = (GLKView*)View;
		auto* Context = new SokolOpenglContext(Window,GlView,SampleCount);
		return std::shared_ptr<Sokol::TContext>(Context);
	}
	
	std::stringstream Error;
	Error << "Class of view " << ViewName << " is not MTKView or GLKView; " << ClassName;
	throw Soy::AssertException(Error);
}






SokolMetalContext::SokolMetalContext(std::shared_ptr<SoyWindow> Window,MTKView* View,int SampleCount) :
	mView	( View )
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
		.sample_count = SampleCount,
		.metal =
		{
			.device= (__bridge const void*) mMetalDevice,
			.renderpass_descriptor_userdata_cb = GetRenderPassDescriptor,
			.drawable_userdata_cb = GetDrawable,
			.user_data = this
		}
	};

}

extern void RunJobOnMainThread(std::function<void()> Lambda,bool Block);

SokolOpenglContext::SokolOpenglContext(std::shared_ptr<SoyWindow> Window,GLKView* View,int SampleCount) :
	mView	( View )
{
	auto OnFrame = [](CGRect Rect)
	{
		std::Debug << __PRETTY_FUNCTION__ << "(" << Rect.origin.x << "," << Rect.origin.y << "," << Rect.size.width << "," << Rect.size.height << ")" << std::endl;
		glClearColor(1.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
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
	//	must be on UI thread, we should be queuing this up on the main window, maybe?
	//	gr: this is a single Dirty-Rect call
	//	a GLViewController will do regular drawing for us
	auto SetNeedDisplay = [this]()
	{
		[mView setNeedsDisplay];
	};
	//RunJobOnMainThread(SetNeedDisplay,false);

	mDelegate = [[SokolViewDelegate alloc] init:OnFrame];
	[mView setDelegate:mDelegate];
	
	/*
	//	built in auto-renderer
	mViewController = [[GLKViewController alloc] init];
	mViewController.view = mView;
	mViewController.preferredFramesPerSecond = 60;
	 */
}
