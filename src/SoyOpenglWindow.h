#pragma once

#include "SoyTypes.h"
#include "SoyThread.h"
#include "SoyOpenglContext.h"
#include "SoyWindow.h"
#include "SoyOpenglContext_Win32.h"

#if __has_feature(objc_arc)
#error expected ARC off, if we NEED arc, then the NSWindow & view need to go in a pure obj-c wrapper to auto retain the refcounted object
#endif

//	temp hack to expose this interface class
namespace Win32
{
	class TOpenglContext;
}


class TOpenglWindow : public SoyWindow, public SoyWorkerThread
{
public:
	TOpenglWindow(const std::string& Name,const Soy::Rectx<int32_t>& Rect,Win32::TOpenglParams Params);
	~TOpenglWindow();
	
	bool			IsValid();
	
	virtual bool	Iteration() override;
	
	std::shared_ptr<Opengl::TContext>	GetContext();

	//	temp hack
	std::shared_ptr<Win32::TOpenglContext>	GetWin32Context();

	virtual std::chrono::milliseconds	GetSleepDuration() override;
	virtual Soy::Rectx<int32_t>			GetScreenRect() override;
	virtual void						SetFullscreen(bool Fullscreen) override;
	virtual bool						IsFullscreen() override;
	virtual bool						IsMinimised() override;
	virtual bool						IsForeground() override;
	virtual void						EnableScrollBars(bool Horz,bool Vert) override;

	virtual void						OnClosed() override;

public:
	std::function<void(Opengl::TRenderTarget&,std::function<void()> LockContext)>	mOnRender;
	std::shared_ptr<Platform::TOpenglView>		mView;
	std::shared_ptr<Platform::TOpenglContext>	mWindowContext;
	std::shared_ptr<Platform::TWin32Thread>		mWindowThread;

	//	mid-level params, controlled at high level (but don't want to call high level every frame for it)
	bool			mEnableRenderWhenMinimised = true;
	bool			mEnableRenderWhenBackground = true;

protected:
	Win32::TOpenglParams	mParams;
	
private:
	std::string		mName;
	std::shared_ptr<Platform::TWindow>		mWindow;

};

