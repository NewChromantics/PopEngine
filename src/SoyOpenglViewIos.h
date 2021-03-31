#pragma once
//	this must be pure obj-c as it's used in swift.
//	it's just a GL view providing callbacks for events
//	todo: make it metal-capable
//	todo: make it ios-compatible for responder events
//	we should make this metal-capable too

//#if !defined(TARGET_OS_IOS)	//	gr: this SHOULD be always defined by xcode
//#error SoyOpenglViewOsx should only be used on ios builds
//#endif

#if !defined(__OBJC__)
#error This should only be included in mm files
#endif

//	GLKViewController needs GLKit framework linked
#import <GLKit/GLKit.h>

typedef enum ButtonEvent : NSUInteger 
{
	MouseUp,
	MouseDown,
	MouseMove
} ButtonEvent;

typedef enum ButtonName : NSUInteger 
{
	MouseNone,
	MouseLeft,
	MouseMiddle,
	MouseRight,
} ButtonName;

//	this class handles it's own delegate
@interface SoyOpenglViewIos : GLKView
{
//	callback function when drawRect is triggered. 
//	we use a objc block as we cannot use any c++ types in this file used by swift 
@public void(^mOnDrawRect)(CGRect);
@public void(^mOnMouseEvent)(CGPoint,ButtonEvent,ButtonName);
}
	
//	overload init so default constructor initialises render modes, double buffering etc
- (nonnull instancetype)init;

@end


//	this could do metal & gl
@interface SoyOpenglViewIos_Delegate : UIResponder<GLKViewDelegate>
	
- (instancetype)init:(SoyOpenglViewIos* _Nonnull)Parent;

//	delegate overload
- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect;
	
@end

