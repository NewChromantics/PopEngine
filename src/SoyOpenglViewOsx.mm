//	gr: this file can be a mix of c++ & obj-c, but the header cannot
#import "SoyOpenglViewOsx.h"

#include "HeapArray.hpp"
#include "SoyString.h"
#include "SoyVector.h"
#include "SoyWindow.h"
#include "SoyGuiApple.h"


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
	mOnMouseEvent = ^(NSPoint Position,ButtonEvent Event,ButtonName Name)
	{
		std::Debug << "Mouse event " << Position.x << "," << Position.y << " Type=" << Event << " Button=" << Name << std::endl;
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
	//	gr: we dont want to report the dirty rect, we need to tell caller about our whole rect
	//auto Width = mView.drawableWidth;
	///auto Height = mView.drawableHeight;
	auto Rect = [self bounds];
	mOnDrawRect(Rect);
}

- (BOOL)acceptsFirstResponder
{
	//	enable key events
	return YES;
}

-(void)mouseMoved:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseMove, MouseNone ); 
}

-(void)mouseDown:(NSEvent *)event
{
	//Platform::PushCursor(SoyCursor::Hand);
	mOnMouseEvent( event.locationInWindow, MouseDown, MouseLeft ); 
}

-(void)mouseDragged:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseMove, MouseLeft ); 
}

-(void)mouseUp:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseUp, MouseLeft ); 
}

-(void)rightMouseDown:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseDown, MouseRight ); 
}

-(void)rightMouseDragged:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseMove, MouseRight ); 
}

-(void)rightMouseUp:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseUp, MouseRight ); 
}

-(void)otherMouseDown:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseDown, MouseMiddle ); 
}

-(void)otherMouseDragged:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseMove, MouseMiddle ); 
}

-(void)otherMouseUp:(NSEvent *)event
{
	mOnMouseEvent( event.locationInWindow, MouseUp, MouseMiddle ); 
}


@end
