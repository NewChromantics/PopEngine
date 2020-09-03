/* tsdk:
	Clone of SoyGuiIos modified to work on Osx... however lots has already been defined in SoyOpenglWindowOsx.mm
	So the necessary parts for getting a metal view by name have been transfered and this file is left in for reference
*/

/*
#include "SoyGui.h"

#import <AppKit/AppKit.h>
@class AppDelegate;

#include "PopMain.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#define SOKOL_IMPL
#define SOKOL_METAL
#include "sokol/sokol_gfx.h"

void RunJobOnMainThread(std::function<void()> Lambda,bool Block)
{
	Soy::TSemaphore Semaphore;

	//	testing if raw dispatch is faster, results negligable
	static bool UseNsDispatch = false;

	if ( UseNsDispatch )
	{
		Soy::TSemaphore* pSemaphore = Block ? &Semaphore : nullptr;

		dispatch_async( dispatch_get_main_queue(), ^(void){
			Lambda();
			if ( pSemaphore )
				pSemaphore->OnCompleted();
		});

		if ( pSemaphore )
			pSemaphore->WaitAndReset();
	}
	else
	{
		auto& Thread = *Soy::Platform::gMainThread;
		if ( Block )
		{
			Thread.PushJob(Lambda,Semaphore);
			Semaphore.WaitAndReset();
		}
		else
		{
			Thread.PushJob(Lambda);
		}
	}
}


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

	NSWindow*		GetWindow();
	NSView*			GetChild(const std::string& Name);
	void			EnumChildren(std::function<bool(NSView*)> EnumChild);
};


class Platform::TLabel : public SoyLabel
{
public:
	TLabel(NSView* View);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override;

	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;

	NSText*				mView = nullptr;
	size_t				mValueVersion = 0;
};

class Platform::TMetalView : public SoyMetalView
{
public:
	TMetalView(NSView* View);

	MTKView*					mMTKView = nullptr;
	id<MTLDevice>				mtl_device;
};

std::shared_ptr<SoyMetalView> Platform::GetMetalView(SoyWindow& Parent, const std::string& Name)
{
	std::shared_ptr<SoyMetalView> MetalView;
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		MetalView.reset( new Platform::TMetalView(View) );
	};
	RunJobOnMainThread( Run, true );
	return MetalView;
}

Platform::TMetalView::TMetalView(NSView* View)
{
	//	todo: check type!
	mMTKView = View;
	mtl_device = MTLCreateSystemDefaultDevice();
	[mMTKView setDevice: mtl_device];

}

std::shared_ptr<Gui::TColourPicker>	Platform::CreateColourPicker(vec3x<uint8_t> InitialColour)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyTextBox> Platform::CreateTextBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyTickBox> Platform::CreateTickBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}


std::shared_ptr<SoyLabel> Platform::CreateLabel(SoyWindow &Parent, Soy::Rectx<int32_t> &Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyLabel> Platform::GetLabel(SoyWindow& Parent,const std::string& Name)
{
	std::shared_ptr<SoyLabel> Label;
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		Label.reset( new Platform::TLabel(View) );
	};
	RunJobOnMainThread( Run, true );
	return Label;
}

// //Defined in SoyOpenglWindowOsx
std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect,bool Resizable)
{
	std::shared_ptr<SoyWindow> Window;
	auto Job = [&]()
	{
		Window.reset( new Platform::TWindow(Name) );
	};
	RunJobOnMainThread( Job, true );
	return Window;
}

std::shared_ptr<SoySlider> Platform::CreateSlider(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}


std::shared_ptr<SoyColourButton> Platform::CreateColourButton(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<Gui::TImageMap> Platform::CreateImageMap(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}


Platform::TLabel::TLabel(NSView* View)
{
	//	todo: check type!
	mView = View;
}

void Platform::TLabel::SetRect(const Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

void Platform::TLabel::SetValue(const std::string& Value)
{
	mValueVersion++;
	auto Version = mValueVersion;

	auto Job = [=]() mutable
	{
		//	updating the UI is expensive, and in some cases we're calling it a lot
		//	sometimes this is 20ms (maybe vsync?), so lets only update if we're latest in the queue
		//Soy::TScopeTimerPrint Timer("Set value",1);
		//	out of date
		if ( Version != this->mValueVersion )
			return;

		this->mView.string = Soy::StringToNSString(Value);
	};
	RunJobOnMainThread( Job, false );
}

std::string Platform::TLabel::GetValue()
{
	std::string Value;
	auto Job = [&]()
	{
		Value = Soy::NSStringToString( mView.string );
	};
	RunJobOnMainThread( Job, true );
	return Value;
}



Platform::TWindow::TWindow(const std::string& Name)
{
	auto* Window = GetWindow();
}

NSWindow* Platform::TWindow::GetWindow()
{
	auto* App = [NSApplication sharedApplication];
	// tsdk: An App can have multiple windows represented in an array, the first member of this array will always? be the main window
	auto* Window = [[App windows] objectAtIndex:0];
	return Window;
}

NSView* Platform::TWindow::GetChild(const std::string& Name)
{
	NSView* ChildMatch = nullptr;
	auto TestChild = [&](NSView* Child)
	{
		//	tsdk: cannot find way to get view based on name so duplicate the name in the accessibility Identifier in the xib file and then match it here
		auto* AccessibilityIdentifier = Child.accessibilityIdentifier;
		if ( AccessibilityIdentifier == nil )
			return true;

		auto RestorationIdString = Soy::NSStringToString(AccessibilityIdentifier);
		if ( RestorationIdString != Name )
			return true;

		//	found match!
		ChildMatch = Child;
		return false;
	};
	EnumChildren(TestChild);
	return ChildMatch;
}

bool RecurseNSViews(NSView* View,std::function<bool(NSView*)>& EnumView);

bool RecurseNSViews(NSView* View,std::function<bool(NSView*)>& EnumView)
{
	if ( !EnumView(View) )
	return false;

	auto* Array = View.subviews;
	auto Size = [Array count];
	for ( auto i=0;	i<Size;	i++ )
	{
		auto Element = [Array objectAtIndex:i];
		if ( !RecurseNSViews( Element, EnumView ) )
		return false;
	}

	return true;
}

void Platform::TWindow::EnumChildren(std::function<bool(NSView*)> EnumChild)
{
	auto* Window = GetWindow();
	// tsdk: in the ios code UIWindow derives from UIView, this is not the case with NSView and NSWindow
	// so call contentView from the documentation => "The window’s content view, the highest accessible NSView object in the window’s view hierarchy."
	RecurseNSViews( [Window contentView], EnumChild );
}

 //Defined in SoyOpenglWindowOsx
Soy::Rectx<int32_t> Platform::TWindow::GetScreenRect()
{
	Soy_AssertTodo();
}

 //Defined in SoyOpenglWindowOsx
void Platform::TWindow::SetFullscreen(bool Fullscreen)
{
	Soy_AssertTodo();
}

 //Defined in SoyOpenglWindowOsx
bool Platform::TWindow::IsFullscreen()
{
	return true;
}

 //Defined in SoyOpenglWindowOsx
bool Platform::TWindow::IsMinimised()
{
	Soy_AssertTodo();
}

 //Defined in SoyOpenglWindowOsx
bool Platform::TWindow::IsForeground()
{
	Soy_AssertTodo();
}

 //Defined in SoyOpenglWindowOsx
void Platform::TWindow::EnableScrollBars(bool Horz,bool Vert)
{
	Soy_AssertTodo();
}

*/
