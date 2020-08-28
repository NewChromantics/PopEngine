#include "SoyGuiOsx.h"

@class AppDelegate;

#include "PopMain.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>


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
	
	UIWindow*		GetWindow();
	UIView*			GetChild(const std::string& Name);
	void			EnumChildren(std::function<bool(UIView*)> EnumChild);
};


class Platform::TLabel : public SoyLabel
{
public:
	TLabel(UIView* View);
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override;
	
	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;

	UITextView*			mView = nullptr;
	size_t				mValueVersion = 0;
};

class Platform::TMetalView : public SoyMetalView
{
public:
	TMetalView(UIView* View);
	
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

Platform::TMetalView::TMetalView(UIView* View)
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


Platform::TLabel::TLabel(UIView* View)
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

		this->mView.text = Soy::StringToNSString(Value);
	};
	RunJobOnMainThread( Job, false );
}

std::string Platform::TLabel::GetValue()
{
	std::string Value;
	auto Job = [&]()
	{
		Value = Soy::NSStringToString( mView.text );
	};
	RunJobOnMainThread( Job, true );
	return Value;
}



Platform::TWindow::TWindow(const std::string& Name)
{
	auto* Window = GetWindow();
}

UIWindow* Platform::TWindow::GetWindow()
{
	auto* App = [UIApplication sharedApplication];
	auto* Window = App.delegate.window;
	return Window;
}

UIView* Platform::TWindow::GetChild(const std::string& Name)
{
	UIView* ChildMatch = nullptr;
	auto TestChild = [&](UIView* Child)
	{
		//	gr: this is the only string in the xib that comes through in a generic way :/
		auto* RestorationIdentifier = Child.restorationIdentifier;
		if ( RestorationIdentifier == nil )
			return true;
		
		auto RestorationIdString = Soy::NSStringToString(RestorationIdentifier);
		if ( RestorationIdString != Name )
			return true;
		
		//	found match!
		ChildMatch = Child;
		return false;
	};
	EnumChildren(TestChild);
	return ChildMatch;
}

bool RecurseUIViews(UIView* View,std::function<bool(UIView*)>& EnumView);

bool RecurseUIViews(UIView* View,std::function<bool(UIView*)>& EnumView)
{
	if ( !EnumView(View) )
	return false;
	
	auto* Array = View.subviews;
	auto Size = [Array count];
	for ( auto i=0;	i<Size;	i++ )
	{
		auto Element = [Array objectAtIndex:i];
		if ( !RecurseUIViews( Element, EnumView ) )
		return false;
	}
	
	return true;
}

void Platform::TWindow::EnumChildren(std::function<bool(UIView*)> EnumChild)
{
	auto* Window = GetWindow();
	
	RecurseUIViews( Window, EnumChild );
}


Soy::Rectx<int32_t> Platform::TWindow::GetScreenRect()
{
	Soy_AssertTodo();
}

void Platform::TWindow::SetFullscreen(bool Fullscreen)
{
	Soy_AssertTodo();
}

bool Platform::TWindow::IsFullscreen()
{
	return true;
}

bool Platform::TWindow::IsMinimised()
{
	Soy_AssertTodo();
}

bool Platform::TWindow::IsForeground()
{
	Soy_AssertTodo();
}

void Platform::TWindow::EnableScrollBars(bool Horz,bool Vert)
{
	//Soy_AssertTodo();
}
