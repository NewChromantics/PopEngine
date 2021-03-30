//	gr: this file can be a mix of c++ & obj-c, but the header cannot
#import "SoyOpenglViewOsx.h"

#include "HeapArray.hpp"
#include "SoyString.h"
#include "SoyVector.h"
#include "SoyWindow.h"
#include "SoyGuiApple.h"


@implementation SoyOpenglViewOsx


- (nonnull instancetype)init
{
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
	//	call lambda
	if ( !mOnDrawRect )
	{
		std::Debug << "GLView::drawRect has no mOnDrawRect assigned" << std::endl;
		return;
	}
	
	//	gr: we dont want to report the dirty rect, we need to tell caller about our whole rect
	auto Rect = [self bounds];
	mOnDrawRect(Rect);
}



@end
