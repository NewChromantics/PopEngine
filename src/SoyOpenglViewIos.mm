#import "SoyOpenglViewIos.h"


@implementation SoyOpenglViewIos
	
- (nonnull instancetype)init
{
	//- (instancetype)initWithFrame:(CGRect)frame context:(EAGLContext *)context;
	self = [super init];

	SoyOpenglViewIos_Delegate* Delegate = [[SoyOpenglViewIos_Delegate alloc] init:self];
	//	setup delegate to self
	[self setDelegate:Delegate];

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
