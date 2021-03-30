//	gr: this file can be a mix of c++ & obj-c, but the header cannot
#import "SoyOpenglViewOsx.h"

#include "HeapArray.hpp"
#include "SoyString.h"
#include "SoyVector.h"
#include "SoyWindow.h"
#include "SoyGuiApple.h"


NSString* MouseLeft = @"Left";

/*
NSPoint ViewPointToVector(NSView* View,const NSPoint& Point)
{
	vec2x<int32_t> Position( Point.x, Point.y );
	
	//	invert y - osx is bottom-left/0,0 so flipped would be what we want. hence wierd flipping when not flipped
	if ( !View.isFlipped )
	{
		auto Rect = NSRectToRect( View.bounds );
		Position.y = Rect.h - Position.y;
	}
	
	return Position;
}
*/


@implementation SoyOpenglViewOsx


- (nonnull instancetype)init
{
	//	setup default events
	mOnMouseEvent = ^(NSPoint Position,ButtonEventType Event,NSString* ButtonName)
	{
		std::Debug << "Mouse event " << Position.x << "," << Position.y << " Type=" << Event << " Button=" << ButtonName << std::endl;
	};
	mOnDrawRect = ^(NSRect)
	{
		std::Debug << "GLView::mOnDrawRect()" << std::endl;
	};


	//	setup opengl view with parameters we need (for sokol)
	//	gr: where do we get this from by default!
	//	gr: set to something small so we can spot when its broken
	NSRect frameRect = NSMakeRect(0,0,20,20);
	auto HardwareAccellerationOnly = true;
	auto DoubleBuffer = false;
	
	Array<NSOpenGLPixelFormatAttribute> Attributes;
	if ( HardwareAccellerationOnly )
	{
		Attributes.PushBack( NSOpenGLPFAAccelerated );
		Attributes.PushBack( NSOpenGLPFANoRecovery );
	}
	
	if ( DoubleBuffer )
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
	
	//	make a depth buffer!
	Attributes.PushBack( NSOpenGLPFADepthSize );
	Attributes.PushBack( 32 );
	
	//	require 3.2 to enable some features without using extensions (eg. glGenVertexArrays)
	Attributes.PushBack( NSOpenGLPFAOpenGLProfile );
	Attributes.PushBack( NSOpenGLProfileVersion3_2Core );
	
	//	terminator
	Attributes.PushBack(0);
	
	NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:Attributes.GetArray()];

	self = [super initWithFrame:frameRect pixelFormat:pixelFormat];
	return self;
}



- (void)drawRect: (NSRect)dirtyRect
{
	//	gr: we dont want to report the dirty rect, we need to tell caller about our whole rect
	//auto Width = mView.drawableWidth;
	///auto Height = mView.drawableHeight;
	auto Rect = [self bounds];
	mOnDrawRect(Rect);
}

-(void)mouseMoved:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, Move, MouseLeft ); 
}
/*
-(void)mouseDown:(NSEvent *)event;
-(void)mouseDragged:(NSEvent *)event;
-(void)mouseUp:(NSEvent *)event;
-(void)rightMouseDown:(NSEvent *)event;
-(void)rightMouseDragged:(NSEvent *)event;
-(void)rightMouseUp:(NSEvent *)event;
-(void)otherMouseDown:(NSEvent *)event;
-(void)otherMouseDragged:(NSEvent *)event;
-(void)otherMouseUp:(NSEvent *)event;
*/

@end
