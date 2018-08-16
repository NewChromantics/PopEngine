#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif
#include <SoyOpenglContext.h>
#include <SoyWindow.h>

class TOpenglView;
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

-(void)mouseDown:(NSEvent *)event;
-(void)mouseDragged:(NSEvent *)event;
-(void)mouseUp:(NSEvent *)event;

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
	GlViewContext(TOpenglView& Parent) :
		mParent		( Parent )
	{
	}
	
	virtual void	Lock() override;
	void			WakeThread();
	bool			IsDoubleBuffered() const;
	
	virtual CGLContextObj						GetPlatformContext() override;
	virtual std::shared_ptr<Opengl::TContext>	CreateSharedContext() override;

public:
	TOpenglView&	mParent;
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
	
	bool			IsValid()	{	return mView != nullptr;	}
	bool			IsDoubleBuffered() const;
	
public:
	std::function<void(const TMousePos&)>		mOnMouseDown;
	std::function<void(const TMousePos&)>		mOnMouseMove;
	std::function<void(const TMousePos&)>		mOnMouseUp;
	std::function<void(Opengl::TRenderTarget&,std::function<void()>)>	mOnRender;
	MacOpenglView*								mView;
	std::shared_ptr<GlViewContext>				mContext;
	GlViewRenderTarget							mRenderTarget;
};
#endif
