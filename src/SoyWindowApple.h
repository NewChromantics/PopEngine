#pragma once


#if defined(TARGET_OSX)
#import <AppKit/AppKit.h>
typedef NSWindow PlatformWindow;
typedef NSView PlatformView;
typedef NSRect PlatformRect;
#elif defined(TARGET_IOS)
#import <UIKit/UIKit.h>
typedef UIWindow PlatformWindow;
typedef UIView PlatformView;
typedef CGRect PlatformRect;
#else
#error Unsupported platform
#endif

class Platform::TWindow : public SoyWindow
{
public:
	TWindow(const std::string& Name);
	TWindow();
	
	virtual Soy::Rectx<int32_t>		GetScreenRect() override;
	
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	virtual bool					IsMinimised() override;
	virtual bool					IsForeground() override;
	virtual void					EnableScrollBars(bool Horz,bool Vert) override;
	
	PlatformWindow*					GetWindow();
	PlatformView*					GetChild(const std::string& Name);
	void							EnumChildren(std::function<bool(PlatformView*)> EnumChild);
	PlatformRect					GetChildRect(Soy::Rectx<int32_t> Rect);
	
	//
	PlatformView*					GetContentView();
	void							OnChildAdded(const Soy::Rectx<int32_t>& ChildRect);

	//void							StartRender( std::function<void()> Frame, std::string ViewName ) override;
	
public:
	PlatformWindow*		mWindow = nullptr;
	PlatformView*		mContentView = nullptr;
	
#if defined(TARGET_OSX)
	CVDisplayLinkRef				mDisplayLink = nullptr;
#endif
};

/*

#if defined(TARGET_OSX)
class Platform::TWindow : public Platform::TWindow
{
public:
	TNsWindow(PopWorker::TJobQueue& Thread,const std::string& Name,const Soy::Rectx<int32_t>& Rect,bool Resizable,std::function<void()> OnAllocated=nullptr);
	TNsWindow(PopWorker::TJobQueue& Thread);
	~TNsWindow()
	{
		//	gr: this also isn't destroying the window
		[mWindow release];
	}

	virtual Soy::Rectx<int32_t>		GetScreenRect() override;
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	virtual bool					IsMinimised() override;
	virtual bool					IsForeground() override;
	virtual void					EnableScrollBars(bool Horz,bool Vert) override;

	NSRect							GetChildRect(Soy::Rectx<int32_t> Rect);
	NSView*							GetContentView();
	void							OnChildAdded(const Soy::Rectx<int32_t>& ChildRect);
	
protected:
	//	run a safe job on the window (main) thread, queued! dont capture by reference
	void							QueueOnThread(std::function<void()> Exec);

public:
	PopWorker::TJobQueue&			mThread;	//	NS ui needs to be on the main thread
	NSWindow*						mWindow = nullptr;
	Platform_View*					mContentView = nullptr;	//	where controls go
	CVDisplayLinkRef				mDisplayLink = nullptr;
	
	//	Added from SoyGuiOsx
	NSWindow*						GetWindow();
	NSView*							GetChild(const std::string& Name);
	void							EnumChildren(std::function<bool(NSView*)> EnumChild);
};
#endif

*/
