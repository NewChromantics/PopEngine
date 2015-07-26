#import <Cocoa/Cocoa.h>
#include "SoyOpenglContext.h"


class TOpenglView;

@interface MacOpenglView : NSOpenGLView
{
	TOpenglView*	mParent;
}

- (id)initFrameWithParent:(TOpenglView*)Parent viewRect:(NSRect)viewRect pixelFormat:(NSOpenGLPixelFormat*)pixelFormat;


//	overloaded
- (void) drawRect: (NSRect) bounds;
@end


class GlViewRenderTarget : public Opengl::TRenderTarget
{
public:
	GlViewRenderTarget(const std::string& Name) :
		TRenderTarget	( Name )
	{
	}
	
	virtual Soy::Rectx<size_t>	GetSize() override	{	return mRect;	}
	virtual bool				Bind() override;
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
	
protected:
	virtual void	PushJobImpl(std::shared_ptr<Opengl::TJob>& Job,Soy::TSemaphore* Semaphore) override;
	
public:
	TOpenglView&	mParent;
};


class TOpenglView
{
public:
	TOpenglView(vec2f Position,vec2f Size);
	~TOpenglView();
	
	bool			IsValid()	{	return mView != nullptr;	}
	
public:
	SoyEvent<Opengl::TRenderTarget>	mOnRender;
	MacOpenglView*				mView;
	GlViewContext				mContext;
	GlViewRenderTarget			mRenderTarget;
};
