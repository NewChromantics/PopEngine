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

//	NSOpenGLView <-- mac
//	GLKView <-- ios
@interface SoyOpenglViewOsx : NSOpenGLView
{
//	callback function when drawRect is triggered. 
//	we use a objc block as we cannot use any c++ types in this file used by swift 
@public void(^mOnDrawRect)(NSRect);
@public void(^mOnMouseEvent)(NSPoint,ButtonEvent,ButtonName);
}


//	overload init so default constructor initialises render modes, double buffering etc
- (nonnull instancetype)init;

//	overload drawRect to detect when window needs redrawing
//	NSView
- (void)drawRect: (NSRect)dirtyRect;

//	NSResponder
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
//acceptsFirstResponder

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender;
/*
- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender; // if the destination responded to draggingEntered: but not to draggingUpdated: the return value from draggingEntered: is used
- (void)draggingExited:(nullable id <NSDraggingInfo>)sender;
 */
- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender;
/*
- (void)concludeDragOperation:(nullable id <NSDraggingInfo>)sender;
// draggingEnded: is implemented as of Mac OS 10.5
- (void)draggingEnded:(nullable id <NSDraggingInfo>)sender;
// the receiver of -wantsPeriodicDraggingUpdates should return NO if it does not require periodic -draggingUpdated messages (eg. not autoscrolling or otherwise dependent on draggingUpdated: sent while mouse is stationary)
- (BOOL)wantsPeriodicDraggingUpdates;

// While a destination may change the dragging images at any time, it is recommended to wait until this method is called before updating the dragging image. This allows the system to delay changing the dragging images until it is likely that the user will drop on this destination. Otherwise, the dragging images will change too often during the drag which would be distracting to the user. The destination may update the dragging images by calling one of the -enumerateDraggingItems methods on the sender.
 - (void)updateDraggingItemsForDrag:(nullable id <NSDraggingInfo>)sender NS_AVAILABLE_MAC(10_7);
*/

@end
