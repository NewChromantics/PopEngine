#pragma once

#include <SoyTypes.h>
#include <SoyThread.h>
#include <SoyOpenglContext.h>
#include <SoyWindow.h>

#if __has_feature(objc_arc)
#error expected ARC off, if we NEED arc, then the NSWindow & view need to go in a pure obj-c wrapper to auto retain the refcounted object
#endif


class MacWindow;
class TOpenglView;


class TOpenglParams
{
public:
	bool		mHardwareAccellerationOnly = true;
	bool		mDoubleBuffer = true;
	bool		mRedrawWithDisplayLink = false;
	int			mVsyncSwapInterval = 0;	//	0 = no vsync
	int			mRefreshRate = 30;		//	will try to skip redraws if vsync on
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

	
private:
	void			OnViewRender(Opengl::TRenderTarget& RenderTarget)
	{
		if ( mOnRender )
			mOnRender(RenderTarget);
	}
	
public:
	std::function<void(Opengl::TRenderTarget&)>	mOnRender;
	std::shared_ptr<TOpenglView>	mView;

protected:
	TOpenglParams	mParams;
	
private:
	std::string		mName;
	std::shared_ptr<MacWindow>		mMacWindow;
};

