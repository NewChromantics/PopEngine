#import "SoyOpenglViewIos.h"

#include "SoyDebug.h"


ButtonName GetTouchButtonName(UITouch* Touch)
{
	//	https://stackoverflow.com/questions/989702/differentiating-between-uitouch-objects-on-the-iphone
	//	gr: apparently the address of UITouch is consistent for down,move,up
	//		and that should be used as the identifier! (NSObject has .hash?
	return MouseLeft;
}

float Range(float Min,float Max,float Value)
{
	return (Value-Min) / (Max-Min);
}

float Lerp(float Min,float Max,float Time)
{
	return Min + ((Max-Min)*Time);
}

CGPoint GetTouchPosition(UITouch* Touch,GLKView* View)
{
	//	a) this is in View.bounds space (eg 1366x910.5)
	//	b) this is flipped (no View.isFlipped like on osx)
	//	c) we need coords in drawableWidthxdrawableHeight (2732x1821)
	auto View_isFlipped = true;
	auto BoundsPosition = [Touch locationInView:View];
	
	float u = Range( View.bounds.origin.x, View.bounds.origin.x+View.bounds.size.width, BoundsPosition.x ); 
	float v = Range( View.bounds.origin.y, View.bounds.origin.y+View.bounds.size.height, BoundsPosition.y ); 
		
	if ( View_isFlipped )
		v = 1.0f - v;
	
	float x = Lerp( 0, View.drawableWidth, u );
	float y = Lerp( 0, View.drawableHeight, v );
	
	return CGPointMake(x,y);
}

@implementation SoyOpenglViewIos
	
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
	
	//- (instancetype)initWithFrame:(CGRect)frame context:(EAGLContext *)context;
	self = [super init];
	
	//	set formats, default has no depth!
	//	https://github.com/floooh/sokol/blob/36b35207c7016ef42c43c7425e21ffd4c11bf01b/sokol_app.h#L3158
	//self.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
	self.drawableDepthFormat = GLKViewDrawableDepthFormat24;
	//self.drawableStencilFormat = GLKViewDrawableStencilFormatNone;
	//self.drawableMultisample = GLKViewDrawableMultisampleNone; /* FIXME */

	SoyOpenglViewIos_Delegate* Delegate = [[SoyOpenglViewIos_Delegate alloc] init:self];
	//	setup delegate to self
	[self setDelegate:Delegate];

	//	gr: seem to on by default
	self.userInteractionEnabled = YES;
#if !defined(TARGET_OS_TV)
	self.multipleTouchEnabled = YES;
#endif

	return self;
}
	
- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
	//	gr: we use the drawableWidth, this is the pixel size instead of
	//		retina size!
	auto Width = view.drawableWidth;
	auto Height = view.drawableHeight;
	auto RealRect = CGRectMake( 0, 0, Width, Height );
	self->mOnDrawRect(RealRect);
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent*)event 
{
	for ( UITouch* Touch in touches )
	{
		auto ButtonName = GetTouchButtonName(Touch);
		auto Position = GetTouchPosition(Touch,self);
		mOnMouseEvent( Position, MouseMove, ButtonName );
	}
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent*)event 
{
	for ( UITouch* Touch in touches )
	{
		auto ButtonName = GetTouchButtonName(Touch);
		auto Position = GetTouchPosition(Touch,self);
		mOnMouseEvent( Position, MouseDown, ButtonName );
	}
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent*)event 
{
	for ( UITouch* Touch in touches )
	{
		auto ButtonName = GetTouchButtonName(Touch);
		auto Position = GetTouchPosition(Touch,self);
		mOnMouseEvent( Position, MouseUp, ButtonName );
	}
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent*)event 
{
	[self touchesEnded:touches withEvent:event];
}
@end



@implementation SoyOpenglViewIos_Delegate
{
	SoyOpenglViewIos* _Nonnull Parent;
}
- (instancetype)init:(SoyOpenglViewIos* _Nonnull)Parent
{
	self = [super init];
	self->Parent = Parent;
	return self;
}
	
- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
	Parent->mOnDrawRect(rect);
}
	
@end
