#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif
#include "SoyOpenglContext.h"
#include "SoyWindow.h"


namespace Platform
{
	class TOpenglView;
}
class TOpenglParams;


#if defined(__OBJC__)
@interface MacOpenglView : NSOpenGLView
{
	TOpenglView*	mParent;
	NSPoint			mLastPos;
}

- (id)initFrameWithParent:(TOpenglView*)Parent viewRect:(NSRect)viewRect pixelFormat:(NSOpenGLPixelFormat*)pixelFormat;


//	overloaded
- (void)drawRect: (NSRect) bounds;

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
#endif

class GlViewRenderTarget : public Opengl::TRenderTarget
{
public:
	GlViewRenderTarget(const std::string& Name) :
		TRenderTarget	( Name )
	{
	}
	
	virtual Soy::Rectx<size_t>	GetSize() override	{	return mRect;	}
	virtual void				Bind() override;
	virtual void				Unbind() override;
	
	Soy::Rectx<size_t>			mRect;
};

class OsxOpenglContext : public Opengl::TContext
{
public:
	virtual void	Lock() override;
	virtual void	Unlock() override;
};

class GlViewContext : public OsxOpenglContext
{
public:
	GlViewContext(Platform::TOpenglView& Parent) :
		mParent		( Parent )
	{
	}
	
	virtual void	Lock() override;
	void			WakeThread();
	bool			IsDoubleBuffered() const;
	
	virtual CGLContextObj						GetPlatformContext() override;
	virtual std::shared_ptr<Opengl::TContext>	CreateSharedContext() override;

public:
	Platform::TOpenglView&	mParent;
};

class GlViewSharedContext : public OsxOpenglContext, public SoyWorkerThread
{
public:
	GlViewSharedContext(CGLContextObj NewContextHandle);
	~GlViewSharedContext();
	
	virtual bool		Iteration() override	{	PopWorker::TJobQueue::Flush(*this);	return true;	}
	virtual bool		CanSleep() override		{	return !PopWorker::TJobQueue::HasJobs();	}	//	break out of conditional with this
	virtual CGLContextObj	GetPlatformContext() override	{	return mContext;	}
	virtual void	WaitForThreadToFinish() override	{	WaitToFinish();	}

public:
	CGLContextObj		mContext;
};



#if defined(__OBJC__)
class TOpenglView
{
public:
	TOpenglView(vec2f Position,vec2f Size,const TOpenglParams& Params);
	~TOpenglView();
	
	bool					IsValid()	{	return mView != nullptr;	}
	bool					IsDoubleBuffered() const;
	Soy::Rectx<int32_t>		GetScreenRect();

public:
	
	std::function<void(const TMousePos&,SoyMouseButton::Type MouseButton)>	mOnMouseDown;
	std::function<void(const TMousePos&,SoyMouseButton::Type MouseButton)>	mOnMouseMove;
	std::function<void(const TMousePos&,SoyMouseButton::Type MouseButton)>	mOnMouseUp;
	std::function<void(SoyKeyButton::Type KeyButton)>						mOnKeyDown;
	std::function<void(SoyKeyButton::Type KeyButton)>						mOnKeyUp;
	std::function<bool(ArrayBridge<std::string>&)>	mOnTryDragDrop;
	std::function<void(ArrayBridge<std::string>&)>	mOnDragDrop;
	std::function<void(Opengl::TRenderTarget&,std::function<void()>)>	mOnRender;
	MacOpenglView*								mView;
	std::shared_ptr<GlViewContext>				mContext;
	GlViewRenderTarget							mRenderTarget;
};
#endif
