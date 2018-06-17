#import <Cocoa/Cocoa.h>
#include <SoyOpenglContext.h>
#include <SoyWindow.h>

class TOpenglView;
class TOpenglParams;


@interface MacOpenglView : NSOpenGLView
{
	TOpenglView*	mParent;
	NSPoint			mLastPos;
}

- (id)initFrameWithParent:(TOpenglView*)Parent viewRect:(NSRect)viewRect pixelFormat:(NSOpenGLPixelFormat*)pixelFormat;


//	overloaded
- (void)drawRect: (NSRect) bounds;

-(void)mouseDown:(NSEvent *)event;
-(void)mouseDragged:(NSEvent *)event;
-(void)mouseUp:(NSEvent *)event;

@end


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

class GlViewContext : public Opengl::TContext
{
public:
	GlViewContext(TOpenglView& Parent) :
		mParent		( Parent )
	{
	}
	
	virtual bool	Lock() override;
	virtual void	Unlock() override;
	
	void			WakeThread();
	bool			IsDoubleBuffered() const;
	
	virtual CGLContextObj						GetPlatformContext() override;
	virtual std::shared_ptr<Opengl::TContext>	CreateSharedContext() override;

public:
	TOpenglView&	mParent;
};

class GlViewSharedContext : public Opengl::TContext, public SoyWorkerThread
{
public:
	GlViewSharedContext(CGLContextObj NewContextHandle);
	~GlViewSharedContext();
	
	virtual bool		Lock() override;
	virtual void		Unlock() override;
	virtual bool		Iteration() override	{	PopWorker::TJobQueue::Flush(*this);	return true;	}
	virtual bool		CanSleep() override		{	return !PopWorker::TJobQueue::HasJobs();	}	//	break out of conditional with this
	virtual CGLContextObj	GetPlatformContext() override	{	return mContext;	}

public:
	CGLContextObj		mContext;
};

class TOpenglView
{
public:
	TOpenglView(vec2f Position,vec2f Size,const TOpenglParams& Params);
	~TOpenglView();
	
	bool			IsValid()	{	return mView != nullptr;	}
	bool			IsDoubleBuffered() const;
	
public:
	SoyEvent<const TMousePos>	mOnMouseDown;
	SoyEvent<const TMousePos>	mOnMouseMove;
	SoyEvent<const TMousePos>	mOnMouseUp;
	SoyEvent<Opengl::TRenderTarget>	mOnRender;
	MacOpenglView*				mView;
	std::shared_ptr<GlViewContext>	mContext;
	GlViewRenderTarget			mRenderTarget;
};
