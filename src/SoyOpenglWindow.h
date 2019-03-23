#pragma once

#include "SoyTypes.h"
#include "SoyThread.h"
#include "SoyOpenglContext.h"
#include "SoyWindow.h"

#if __has_feature(objc_arc)
#error expected ARC off, if we NEED arc, then the NSWindow & view need to go in a pure obj-c wrapper to auto retain the refcounted object
#endif


namespace Platform
{
	class TWindow;
}

class TOpenglView;


class TOpenglParams
{
public:
	bool		mHardwareAccellerationOnly = true;
	bool		mDoubleBuffer = true;
	bool		mRedrawWithDisplayLink = true;
	int			mVsyncSwapInterval = 1;	//	0 = no vsync
	int			mRefreshRate = 60;		//	will try to skip redraws if vsync on
	bool		mAutoRedraw = true;
};

class TOpenglWindow : public SoyWindow, public SoyWorkerThread
{
public:
	TOpenglWindow(const std::string& Name,Soy::Rectf Rect,TOpenglParams Params);
	~TOpenglWindow();
	
	bool			IsValid();
	
	virtual bool	Iteration() override;
	
	std::shared_ptr<Opengl::TContext>	GetContext();

	virtual std::chrono::milliseconds	GetSleepDuration() override;
	virtual Soy::Rectx<int32_t>			GetScreenRect() override;

private:
	void			OnViewRender(Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext)
	{
		if ( mOnRender )
			mOnRender(RenderTarget, LockContext );
	}
	
public:
	std::function<void(Opengl::TRenderTarget&,std::function<void()> LockContext)>	mOnRender;
	std::shared_ptr<TOpenglView>	mView;

protected:
	TOpenglParams	mParams;
	
private:
	std::string		mName;
	std::shared_ptr<Platform::TWindow>		mWindow;
};

