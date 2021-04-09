//	gr: this file can be a mix of c++ & obj-c, but the header cannot
#import "SoyOpenglViewOsx.h"

#include "HeapArray.hpp"
#include "SoyString.h"
#include "SoyVector.h"
#include "SoyWindow.h"
#include "SoyGuiApple.h"


CGPoint GetTouchPosition(NSEvent* Touch,NSView* View)
{
	//	a) this is in View.bounds space (eg 1366x910.5)
	//	b) this is flipped (no View.isFlipped like on osx)
	//	c) we need coords in drawableWidthxdrawableHeight (2732x1821)
	auto View_isFlipped = View.isFlipped;
	
	//	window... not view? but seems to give view pos
	auto BoundsPosition = Touch.locationInWindow;//[Touch locationInView:View];
	auto Bounds = [View bounds];
	
	if ( View_isFlipped )
		BoundsPosition.y = Bounds.size.height - BoundsPosition.y;
	
	BoundsPosition = [View convertPointToBacking:BoundsPosition];

	auto x = BoundsPosition.x;
	auto y = BoundsPosition.y;
	return CGPointMake(x,y);
}

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
	mOnMouseEvent = ^(CGPoint Position,ButtonEvent Event,ButtonName Name)
	{
		std::Debug << "Mouse event " << Position.x << "," << Position.y << " Type=" << Event << " Button=" << Name << std::endl;
	};
	mOnDrawRect = ^(CGRect)
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
	
	
	[self initDragAndDrop];
	[self initMouseTracking];
	
	return self;
}


- (void)initDragAndDrop
{
	//	enable drag & drop
	//	https://stackoverflow.com/a/29029456
	//	https://stackoverflow.com/a/8567836	NSFilenamesPboardType
	[self registerForDraggedTypes: @[(NSString*)kUTTypeItem]];
	//[self registerForDraggedTypes:[NSImage imagePasteboardTypes]];
	//registerForDraggedTypes([NSFilenamesPboardType])
}


- (void)initMouseTracking
{
	//	enable mouse tracking events (enter, exit, move)
	//	https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/TrackingAreaObjects/TrackingAreaObjects.html#//apple_ref/doc/uid/10000060i-CH8-SW1
	NSTrackingAreaOptions options = (NSTrackingActiveAlways | NSTrackingInVisibleRect |
									 NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved);
	
	NSTrackingArea *area = [[NSTrackingArea alloc] initWithRect:[self bounds]
														options:options
														  owner:self
													   userInfo:nil];
	[self addTrackingArea:area];
	
}

- (void)drawRect: (NSRect)dirtyRect
{
	auto Rect = [self convertRectToBacking:[self bounds]];
	/*
	auto Scale = self.getPixelScreenCoordScale;
	//	gr: we dont want to report the dirty rect, we need to tell caller about our whole rect
	auto Size = self.drawableSize;
	auto Width = self.drawableWidth;
	auto Height = self.drawableHeight;
	//auto Rect = [self bounds];
	auto Rect = CGRectMake( 0, 0, Width, Height );
	*/
	mOnDrawRect(Rect);
}

- (BOOL)acceptsFirstResponder
{
	//	enable key events
	return YES;
}

-(void)mouseMoved:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseMove, MouseNone ); 
}

-(void)mouseDown:(NSEvent *)event
{
	//Platform::PushCursor(SoyCursor::Hand);
	mOnMouseEvent( GetTouchPosition(event,self), MouseDown, MouseLeft ); 
}

-(void)mouseDragged:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseMove, MouseLeft ); 
}

-(void)mouseUp:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseUp, MouseLeft ); 
}

-(void)rightMouseDown:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseDown, MouseRight ); 
}

-(void)rightMouseDragged:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseMove, MouseRight ); 
}

-(void)rightMouseUp:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseUp, MouseRight ); 
}

-(void)otherMouseDown:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseDown, MouseMiddle ); 
}

-(void)otherMouseDragged:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseMove, MouseMiddle ); 
}

-(void)otherMouseUp:(NSEvent *)event
{
	mOnMouseEvent( GetTouchPosition(event,self), MouseUp, MouseMiddle ); 
}


@end
