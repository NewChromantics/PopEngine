//	this must be pure obj-c as it's used in swift.
//	it's just a GL view providing callbacks for events
//	todo: make it metal-capable
//	todo: make it ios-compatible for responder events
//	we should make this metal-capable too

#if TARGET_OS_IOS	//	gr: this SHOULD be always defined by xcode
#error SoyOpenglViewOsx should only be used on osx builds
#endif

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
#import <GLKit/GLKit.h>

//	NSOpenGLView <-- mac
//	GLKView <-- ios
@interface SoyOpenglViewOsx : NSOpenGLView
{
//	callback function when drawRect is triggered. 
//	we use a objc block as we cannot use any c++ types in this file used by swift 
@public void(^mOnDrawRect)(NSRect);
}

//	overload drawRect to detect when window needs redrawing
- (void)drawRect: (NSRect)dirtyRect;

//	overload init so default constructor initialises render modes, double buffering etc
- (nonnull instancetype)init;

@end
