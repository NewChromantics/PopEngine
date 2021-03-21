#pragma once

#include "SoyWindow.h"
#include "SoyGui.h"
#include "SoyGuiApple.h"
#include "SoyWindowApple.h"

namespace Platform
{
	class TNsWindow;
}

#if defined(TARGET_OSX)
//	note: we want to re-use stuff that's in MacOpenglView too
@interface Platform_View: NSView
{
	@public std::function<NSDragOperation()>				mGetDragDropCursor;
	@public std::function<bool(ArrayBridge<std::string>&)>	mTryDragDrop;
	@public std::function<bool(ArrayBridge<std::string>&)>	mOnDragDrop;
}

-(void)RegisterForEvents;

//	overloaded funcs
- (BOOL) isFlipped;
/*
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
*/
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender;
- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender;


@end
#endif

