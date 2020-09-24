#pragma once

#if defined(TARGET_OSX)
#import <AppKit/AppKit.h>
typedef NSWindow PlatformWindow;
typedef NSView PlatformView;
#elif defined(TARGET_IOS)
#import <UIKit/UIKit.h>
typedef UIWindow PlatformWindow;
typedef UIView PlatformView;
#else
#error Unsupported platform
#endif

class Platform::TWindow : public SoyWindow
{
public:
	TWindow(const std::string& Name);
	
	virtual Soy::Rectx<int32_t>		GetScreenRect() override;
	
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	virtual bool					IsMinimised() override;
	virtual bool					IsForeground() override;
	virtual void					EnableScrollBars(bool Horz,bool Vert) override;
	
	PlatformWindow*					GetWindow();
	PlatformView*					GetChild(const std::string& Name);
	void							EnumChildren(std::function<bool(PlatformView*)> EnumChild);
	void							StartRender( std::function<void()> Frame, std::string ViewName ) override;
};
