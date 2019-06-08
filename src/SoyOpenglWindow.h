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
	class TSlider;
	class TTextBox;
	class TLabel;
	class TTickBox;

	class TOpenglView;		//	on osx it's a view control
	class TOpenglContext;	//	on windows, its a context that binds to any control
	class TWin32Thread;		//	windows needs to make calls on a specific thread (just as OSX needs it to be on the main dispatcher)
	
	
	std::shared_ptr<SoyWindow>	CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect,bool Resizable);
	std::shared_ptr<SoySlider>	CreateSlider(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect);
	std::shared_ptr<SoyTextBox>	CreateTextBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect);
	std::shared_ptr<SoyLabel>	CreateLabel(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect);
	std::shared_ptr<SoyTickBox>	CreateTickBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect);
}


class TOpenglParams
{
public:
	bool		mFullscreen = false;
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
	TOpenglWindow(const std::string& Name,const Soy::Rectx<int32_t>& Rect,TOpenglParams Params);
	~TOpenglWindow();
	
	bool			IsValid();
	
	virtual bool	Iteration() override;
	
	std::shared_ptr<Opengl::TContext>	GetContext();

	virtual std::chrono::milliseconds	GetSleepDuration() override;
	virtual Soy::Rectx<int32_t>			GetScreenRect() override;
	virtual void						SetFullscreen(bool Fullscreen) override;
	virtual bool						IsFullscreen() override;
	virtual void						EnableScrollBars(bool Horz,bool Vert) override;

	virtual void						OnClosed() override;

public:
	std::function<void(Opengl::TRenderTarget&,std::function<void()> LockContext)>	mOnRender;
	std::shared_ptr<Platform::TOpenglView>		mView;
	std::shared_ptr<Platform::TOpenglContext>	mWindowContext;
	std::shared_ptr<Platform::TWin32Thread>		mWindowThread;

protected:
	TOpenglParams	mParams;
	
private:
	std::string		mName;
	std::shared_ptr<Platform::TWindow>		mWindow;
};

