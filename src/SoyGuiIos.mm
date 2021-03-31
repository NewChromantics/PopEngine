#include "SoyGui.h"
#include "SoyGuiApple.h"
#include "SoyWindowApple.h"
#include "SoyGuiSwiftCpp.h"

#include "PopMain.h"
#include <TargetConditionals.h>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import <GLKit/GLKit.h>

namespace Sokol
{
	#include "sokol/sokol_gfx.h"
}
#import <UIKit/UICollectionView.h>	//	UICollectionViewDataSource


#import <objc/runtime.h>
void ListClassVariables(UIView* View)
{
	unsigned int varCount;
	Ivar *vars = class_copyIvarList([View class], &varCount);

	for (int i = 0; i < varCount; i++)
	{
    	Ivar var = vars[i];
		const char* name = ivar_getName(var);
		const char* typeEncoding = ivar_getTypeEncoding(var);
		if ( !name )	name = "<null>";
		if ( !typeEncoding )	typeEncoding = "<null>";
		std::Debug << typeEncoding << " " << name << std::endl;
	}
}

namespace Platform
{
	class TMetalView;
	
	static size_t	UniqueIdentifierCounter = 1000;
	std::string		GetUniqueIdentifier();
	
	std::string		GetClassName(UIView* View);
}

namespace Swift
{
	std::shared_ptr<SoyLabel>			GetLabel(const std::string& Name);
	std::shared_ptr<SoyButton>			GetButton(const std::string& Name);
	std::shared_ptr<SoyTickBox>			GetTickBox(const std::string& Name);
	std::shared_ptr<Gui::TRenderView>	GetRenderView(const std::string& Name);
	std::shared_ptr<Gui::TList>			GetList(const std::string& Name);
}


@interface Views_CollectionViewCell: UICollectionViewCell 
{
}
- (void)prepareForReuse;
@end

@implementation Views_CollectionViewCell

- (void)prepareForReuse
{
	[super prepareForReuse];
	//self.autoresizesSubviews = YES;
	
	self.contentView.contentMode = UIViewContentModeScaleToFill;
	
	//	remove subviews
}

@end


//	gr: a data source, which is just a bunch of views
//		to mimic a win32 icon
@interface Views_DataSource : NSObject<UICollectionViewDataSource>
{
	@public Array<UIView*>					mChildViews;
	@public Array<UICollectionViewCell*>	mChildCells;
	bool									mRegisteredCellClass;
}

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section;
// The cell that is returned must be retrieved from a call to -dequeueReusableCellWithReuseIdentifier:forIndexPath:
- (__kindof UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath;

/*
- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView;

// The view that is returned must be retrieved from a call to -dequeueReusableSupplementaryViewOfKind:withReuseIdentifier:forIndexPath:
- (UICollectionReusableView *)collectionView:(UICollectionView *)collectionView viewForSupplementaryElementOfKind:(NSString *)kind atIndexPath:(NSIndexPath *)indexPath;

- (BOOL)collectionView:(UICollectionView *)collectionView canMoveItemAtIndexPath:(NSIndexPath *)indexPath API_AVAILABLE(ios(9.0));
- (void)collectionView:(UICollectionView *)collectionView moveItemAtIndexPath:(NSIndexPath *)sourceIndexPath toIndexPath:(NSIndexPath*)destinationIndexPath API_AVAILABLE(ios(9.0));

/// Returns a list of index titles to display in the index view (e.g. ["A", "B", "C" ... "Z", "#"])
- (nullable NSArray<NSString *> *)indexTitlesForCollectionView:(UICollectionView *)collectionView API_AVAILABLE(tvos(10.2));

/// Returns the index path that corresponds to the given title / index. (e.g. "B",1)
/// Return an index path with a single index to indicate an entire section, instead of a specific item.
- (NSIndexPath *)collectionView:(UICollectionView *)collectionView indexPathForIndexTitle:(NSString *)title atIndex:(NSInteger)index API_AVAILABLE(tvos(10.2));
*/

- (void)AddChild:(UIView*)Child;
@end

@implementation Views_DataSource


- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
	return mChildViews.GetSize();
}

- (__kindof UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
	if ( !mRegisteredCellClass ) 
    {
    	NSString* ReuseIdentifier = @"Views_CollectionViewCell";
    	id CellClass = Views_CollectionViewCell.self;
        [collectionView registerClass:CellClass forCellWithReuseIdentifier:ReuseIdentifier];
        mRegisteredCellClass = true;
    }
    
	NSString* ReuseIdentifier = @"Views_CollectionViewCell";
	auto* Cell = [collectionView dequeueReusableCellWithReuseIdentifier:ReuseIdentifier forIndexPath:indexPath];

	//	setup cell
	auto Section = indexPath.section;	//	should be 0
	auto Index = indexPath.row;
	auto* Child = mChildViews[Index];
	
	auto* CellView = Cell.contentView;
	
	//	style things
	CellView.clipsToBounds = YES;
	CellView.backgroundColor = UIColor.blackColor;	//	for debugging
	
	//	gr: this isn't working
	CellView.contentMode = UIViewContentModeScaleToFill;

	//	add to content view
	[CellView addSubview:Child];
	
	//	redo layout
	if ( collectionView.collectionViewLayout )
	{
		[collectionView.collectionViewLayout invalidateLayout];
	}
	
	[collectionView layoutIfNeeded];
		
	return Cell;
}

- (void)AddChild:(UIView*)Child
{
	mChildViews.PushBack(Child);	//	retain?
}

@end

template<typename NATIVECLASS>
class PlatformControl
{
public:
	~PlatformControl()
	{
		//std::Debug << "Control deallocating" << std::endl;
	}
	void			SetControl(UIView* View);		//	type checked		
	void			AddToParent(Platform::TWindow& Window);

	void			SetVisible(bool Visible);
	void			SetColour(const vec3x<uint8_t>& Rgb);

	NATIVECLASS*	mControl = nullptr;
};


@interface TResponder : UIResponder
{
@public std::function<void()>	mCallback;
}

-(void) OnAction;

@end


@implementation TResponder

-(void) OnAction
{
	//	call lambda
	if ( !mCallback )
	{
		std::Debug << "TResponderCallback unhandled callback" << std::endl;
		return;
	}
	mCallback();
}

@end

void GetColourSpace(CGColorSpaceRef& ColourSpace,CGBitmapInfo& Flags,SoyPixelsFormat::Type Format)
{
	switch(Format)
	{
		case SoyPixelsFormat::Greyscale:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaNone;
			ColourSpace = CGColorSpaceCreateDeviceGray();
			return;

		case SoyPixelsFormat::GreyscaleAlpha:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast;
			ColourSpace = CGColorSpaceCreateDeviceGray();
			return;

		case SoyPixelsFormat::RGB:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaNone;
			ColourSpace = CGColorSpaceCreateDeviceRGB();
			return;

		case SoyPixelsFormat::RGBA:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast;
			ColourSpace = CGColorSpaceCreateDeviceRGB();
			return;

		case SoyPixelsFormat::ARGB:
			Flags = kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedFirst;
			ColourSpace = CGColorSpaceCreateDeviceRGB();
			return;
	}

	std::stringstream Error;
	Error << "Unhandled format(" << magic_enum::enum_name(Format) << ") to convert to CGColour space (options are gray or rgb)";
	throw Soy::AssertException(Error);
}

class Platform::TLabel : public SoyLabel, public PlatformControl<UILabel>
{
public:
	TLabel(UIView* View);
	TLabel(TWindow& Parent,Soy::Rectx<int32_t>& Rect);
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override;
	virtual void		SetVisible(bool Visible) override				{	PlatformControl::SetVisible(Visible);	}
	virtual void		SetColour(const vec3x<uint8_t>& Rgb) override	{	PlatformControl::SetColour(Rgb);	}

	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;

	size_t				mValueVersion = 0;
};

//	UITextField = single line
//	UITextView = Multi line
class Platform::TTextBox : public SoyTextBox, public PlatformControl<UITextField>
{
public:
	TTextBox(UIView* View);
	~TTextBox();
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override;
	virtual void		SetVisible(bool Visible) override		{	PlatformControl::SetVisible(Visible);	}
	virtual void		SetColour(const vec3x<uint8_t>& Rgb) override	{	PlatformControl::SetColour(Rgb);	}
	
	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;

	TResponder*			mResponder = [TResponder alloc];
};

//	a button can act as
//	- button
//	- Tick box with selected state
class Platform::TButton : public SoyButton, public SoyTickBox, public PlatformControl<UIView>
{
public:
	TButton(UIView* View);
	TButton(TWindow& Parent,Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override;
	virtual void		SetVisible(bool Visible) override		{	PlatformControl::SetVisible(Visible);	}
	virtual void		SetColour(const vec3x<uint8_t>& Rgb) override	{	PlatformControl::SetColour(Rgb);	}

	//	button (gr: and tickbox?)
	virtual void		SetLabel(const std::string& Value) override;

	//	tickbox
	virtual void		SetValue(bool Value) override;
	virtual bool		GetValue() override;
	void				ToggleTickBoxValue();

private:
	void				BindEvents();
	void				GetValueNow();	//	update cached value, should be called on main thread

public:
	bool				mCachedTickBoxValue = false;
	TResponder*			mResponder = [TResponder alloc];
};

class Platform::TMetalView : public Gui::TRenderView
{
public:
	TMetalView(UIView* View);
	
	MTKView*					mMTKView = nullptr;
	id<MTLDevice>				mtl_device;
};


class Platform::TImageMap : public Gui::TImageMap, public PlatformControl<UIImageView>
{
public:
	TImageMap(TWindow& Parent,Soy::Rectx<int32_t>& Rect);
	TImageMap(UIView* View);
	~TImageMap();
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override;
	virtual void		SetVisible(bool Visible) override		{	PlatformControl::SetVisible(Visible);	}
	virtual void		SetColour(const vec3x<uint8_t>& Rgb) override	{	PlatformControl::SetColour(Rgb);	}

	virtual void		SetImage(const SoyPixelsImpl& Pixels) override;
	virtual void		SetCursorMap(const SoyPixelsImpl& CursorMap,const ArrayBridge<std::string>&& CursorIndexes) override;

private:
	void				FreeImage();

public:
	TResponder*			mResponder = [TResponder alloc];
	
private:
	CGImageRef			mCgImage = nullptr;
	UIImage*			mNsImage = nullptr;
	SoyPixels			mPixelsImage;		//	we keep a copy for thread use and CGImage references these bytes

};


std::shared_ptr<Gui::TRenderView> Platform::GetRenderView(SoyWindow& Parent, const std::string& Name)
{
	try
	{
		return Swift::GetRenderView(Name);
	}
	catch(std::exception& e)
	{
		std::Debug << "Failed to get swift render view " << Name << "; " << e.what() << std::endl;
	}
		

	std::shared_ptr<Gui::TRenderView> MetalView;
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		if ( !View )
			throw Soy::AssertException(std::string("No view found named ") + Name);
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

std::shared_ptr<SoyTextBox> Platform::GetTextBox(SoyWindow& Parent,const std::string& Name)
{
	std::shared_ptr<SoyTextBox> Control;
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		if ( !View )
			throw Soy::AssertException(std::string("No view found named ") + Name);
		Control.reset( new Platform::TTextBox(View) );
	};
	RunJobOnMainThread( Run, true );
	return Control;
}

std::shared_ptr<SoyTickBox> Platform::CreateTickBox(SoyWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyTickBox> Platform::GetTickBox(SoyWindow& Parent,const std::string& Name)
{
	try
	{
		return Swift::GetTickBox(Name);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
		
	std::shared_ptr<SoyTickBox> Control;
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		if ( !View )
			throw Soy::AssertException(std::string("No view found named ") + Name);
			
		std::Debug << "Creating tickbox from " << GetClassName(View) << std::endl;
		//	gr: tickbox can be interpreted as many types
		Control.reset( new Platform::TButton(View) );
	};
	RunJobOnMainThread( Run, true );
	return Control;
}


std::shared_ptr<SoyLabel> Platform::CreateLabel(SoyWindow &Parent, Soy::Rectx<int32_t> &Rect)
{
	auto& ParentView = dynamic_cast<Platform::TWindow&>(Parent);
	std::shared_ptr<SoyLabel> Label;
	auto Allocate = [&]() mutable
	{
		Label.reset( new Platform::TLabel( ParentView, Rect) );
		Label->SetValue("New label");
	};
	RunJobOnMainThread(Allocate,true);
	return Label;
}

std::shared_ptr<SoyLabel> Platform::GetLabel(SoyWindow& Parent,const std::string& Name)
{
	try
	{
		return Swift::GetLabel(Name);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
		
	std::shared_ptr<SoyLabel> Label;
	
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		if ( !View )
			throw Soy::AssertException(std::string("No view found named ") + Name);
		Label.reset( new Platform::TLabel(View) );
	};
	RunJobOnMainThread( Run, true );
	return Label;
}

std::shared_ptr<SoyButton> Platform::CreateButton(SoyWindow &Parent, Soy::Rectx<int32_t> &Rect)
{
	auto& ParentView = dynamic_cast<Platform::TWindow&>(Parent);
	std::shared_ptr<SoyButton> Label;
	auto Allocate = [&]() mutable
	{
		Label.reset( new Platform::TButton( ParentView, Rect) );
		Label->SetLabel("New Button");
	};
	RunJobOnMainThread(Allocate,true);
	return Label;
}

std::shared_ptr<SoyButton> Platform::GetButton(SoyWindow& Parent,const std::string& Name)
{
	try
	{
		return Swift::GetButton(Name);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}		

	std::shared_ptr<SoyButton> Label;
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		if ( !View )
			throw Soy::AssertException(std::string("No view found named ") + Name);
		Label.reset( new Platform::TButton(View) );
	};
	RunJobOnMainThread( Run, true );
	return Label;
}


std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name,Soy::Rectx<int32_t>& Rect,bool Resizable)
{
	//	try and create a swift window instance with this name
	try
	{
		auto Window = Swift::GetWindow(Name);
		return Window;
	}
	catch(std::exception& e)
	{
		std::Debug << "Failed to create swift window instance named " << Name << "; " << e.what() << std::endl;
	}
	
	std::shared_ptr<SoyWindow> Window;
	auto Job = [&]()
	{
		Window.reset( new Platform::TWindow() );
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
	auto& ParentView = dynamic_cast<Platform::TWindow&>(Parent);
	std::shared_ptr<Gui::TImageMap> Control;
	auto Allocate = [&]() mutable
	{
		Control.reset( new Platform::TImageMap(ParentView,Rect) );
	};
	RunJobOnMainThread(Allocate,true);
	return Control;
}

std::shared_ptr<Gui::TImageMap> Platform::GetImageMap(SoyWindow& Parent,const std::string& Name)
{
	auto& Window = dynamic_cast<Platform::TWindow&>(Parent);
	std::shared_ptr<Gui::TImageMap> Control;
	auto Run = [&]()
	{
		auto* View = Window.GetChild(Name);
		if ( !View )
			throw Soy::AssertException(std::string("No view found named ") + Name);
		Control.reset( new Platform::TImageMap(View) );
	};
	RunJobOnMainThread(Run,true);
	return Control;
}

Platform::TLabel::TLabel(UIView* View)
{
	//	todo: check type!
	mControl = View;
}

Platform::TLabel::TLabel(TWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	//auto RectNs = Parent.GetChildRect(Rect);
	auto PlatformRect = CGRectMake( Rect.x, Rect.y, Rect.w, Rect.h );
	mControl = [[UILabel alloc] initWithFrame:PlatformRect];
	AddToParent( Parent );
}

void Platform::TLabel::SetRect(const Soy::Rectx<int32_t>& Rect)
{
	//	todo
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

		this->mControl.text = Soy::StringToNSString(Value);
	};
	RunJobOnMainThread( Job, false );
}

std::string Platform::TLabel::GetValue()
{
	std::string Value;
	auto Job = [&]()
	{
		Value = Soy::NSStringToString( mControl.text );
	};
	RunJobOnMainThread( Job, true );
	return Value;
}




Platform::TTextBox::TTextBox(UIView* View)
{
	SetControl(View);
	
	auto AddResponder = [&]()
	{
		//	setup delegate/responder
		//auto ListenEvents = UIControlEventAllEvents;
		auto ListenEvents = UIControlEventEditingChanged;
		//	gr: do we want this? or should event only fire on user-change?
		//ListenEvents |= UIControlEventValueChanged;
	 	[mControl addTarget:mResponder action:@selector(OnAction) forControlEvents:ListenEvents];
	};
	Platform::ExecuteTryCatchObjc(AddResponder);

	mResponder->mCallback = [this]()	{	this->OnChanged();	};
}

Platform::TTextBox::~TTextBox()
{
	mResponder->mCallback = nullptr;
}

void Platform::TTextBox::SetRect(const Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

void Platform::TTextBox::SetValue(const std::string& Value)
{
	auto Job = [=]() mutable
	{
		this->mControl.text = Soy::StringToNSString(Value);
	};
	RunJobOnMainThread( Job, false );
}

std::string Platform::TTextBox::GetValue()
{
	std::string Value;
	auto Job = [&]()
	{
		Value = Soy::NSStringToString( mControl.text );
	};
	RunJobOnMainThread( Job, true );
	return Value;
}


Platform::TButton::TButton(UIView* View)
{
	//	this IOS code expects to be run on the main thread
	//	the OSX code can run on any thread, and then inits on main thread (before we had Platform::CreateXXX)
	//	we should align the paradigms
	SetControl(View);
	BindEvents();
	GetValueNow();
}

static int DebugColourIndex = 0;
UIColor* GetDebugColour()
{
	UIColor* Colours[] =
	{
		UIColor.redColor,
		UIColor.orangeColor,
		UIColor.yellowColor,
		UIColor.greenColor,
		UIColor.cyanColor,
		UIColor.blueColor,
		UIColor.purpleColor,
		UIColor.magentaColor,
		UIColor.brownColor,
	};
	auto Index = (DebugColourIndex++) % std::size(Colours);
	return Colours[Index];
}

Platform::TButton::TButton(TWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	//auto RectNs = Parent.GetChildRect(Rect);
	auto PlatformRect = CGRectMake( Rect.x, Rect.y, Rect.w, Rect.h );
	mControl = [[UIButton alloc] initWithFrame:PlatformRect];
	
	//	debug to help sizing
	auto* DebugColour = GetDebugColour();
	mControl.backgroundColor = DebugColour;

	AddToParent( Parent );
	
	BindEvents();
}

void Platform::TButton::BindEvents()
{
	//	setup delegate/responder to get click
	@try
	{
		//auto ListenEvents = UIControlEventAllEvents;
		auto ListenEvents = UIControlEventTouchUpInside;
	 	[mControl addTarget:mResponder action:@selector(OnAction) forControlEvents:ListenEvents];
 	}
 	@catch (NSException* e)
	{
		throw Soy::AssertException(e);
	}

	mResponder->mCallback = [this]()	
	{
		//	gr: if we're a check box, toggle state
		ToggleTickBoxValue();
		GetValueNow();
		this->OnClicked();	//	button	
		this->OnChanged();	//	tick box
	};

	//	init style
	
	//	make text wrap
	if ( [mControl isKindOfClass:[UIButton class]] )
	//if ( [mControl respondsToSelector:@"titleLabel"])
	{
		UIButton* Button = (UIButton*)mControl;
		Button.titleLabel.numberOfLines = 0;
		Button.titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
		Button.titleLabel.textAlignment = NSTextAlignmentCenter;
	}
}

void Platform::TButton::SetRect(const Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}

void Platform::TButton::SetLabel(const std::string& Value)
{
	auto Job = [=]() mutable
	{
		auto ValueNs = Soy::StringToNSString(Value);
		[this->mControl setTitle:ValueNs forState:UIControlStateNormal];
	};
	RunJobOnMainThread( Job, false );
}


void Platform::TButton::SetValue(bool Value)
{
	auto Job = [=]() mutable
	{
		mCachedTickBoxValue = Value;
		if ( [mControl isKindOfClass:[UIButton class]] )
		{
			UIButton* Button = (UIButton*)mControl;
			Button.selected = Value;
			//button.highlighted = NO;
			//button.enabled = Yes;
		}
	};
	RunJobOnMainThread( Job, false );
}

bool Platform::TButton::GetValue()
{
	return mCachedTickBoxValue;
}



void Platform::TButton::GetValueNow()
{
	//	should be run on main thread
	if ( [mControl isKindOfClass:[UIButton class]] )
	{
		UIButton* Button = (UIButton*)mControl;
		mCachedTickBoxValue = Button.selected;
	}
	else
	{
		Soy_AssertTodo();
	}
}

void Platform::TButton::ToggleTickBoxValue()
{
	//	should be run on main thread
	if ( [mControl isKindOfClass:[UIButton class]] )
	{
		UIButton* Button = (UIButton*)mControl;
		Button.selected = !Button.selected;
	}
	else
	{
		Soy_AssertTodo();
	}
}

Platform::TWindow::TWindow() :
	TWindow	( std::string() )
{
}

Platform::TWindow::TWindow(const std::string& Name)
{
	//	on ios, we should have null to use the global window
	//	but we should support external screens...
	//	if there is a name, try and find a child window with the name
	//	if there isn't create a new window?
	auto* GlobalWindow = GetWindow();
	if ( !GlobalWindow )
		throw Soy::AssertException("Couldn't get ios window");
	mWindow = GlobalWindow;

	//	if we have a name, and there's a matching child
	//	then this is a "sub window"
	if ( Name.length() )
	{
		mContentView = GetChild(Name);
		if ( mContentView )
		{
			mWindow = nullptr;
			std::Debug << "Bound " << Name << " to child window" << std::endl;
			return;
		}
	}
	
	mContentView = mWindow.rootViewController.view;

	static bool DebugAllChildren = false;
	if ( DebugAllChildren )
	{
		//	gr: list all children
		auto EnumChild = [&](UIView* Child)
		{
			auto Name = GetClassName(Child);
			std::Debug << "Child & vars: " << Name << std::endl;
			ListClassVariables(Child);
			return true;
		};
		EnumChildren(EnumChild);
	}
}

UIWindow* Platform::TWindow::GetWindow()
{
	UIWindow* Window = nullptr;
	auto Job = [&]()
	{
		auto* App = [UIApplication sharedApplication];

		//	objective-c storyboard's main window
		Window = App.delegate.window;
		if ( Window )
			return;

		//	loop through all windows
		auto EnumWindow = [&](UIWindow* AWindow)
		{
			bool IsMainWindow = AWindow.keyWindow;
			std::Debug << "Found window IsMainWindow=" << (IsMainWindow?"true":"false") << std::endl;
			if ( AWindow )
				Window = AWindow;
		};
		auto* Windows = [App windows];
		NSArray_ForEach<UIWindow*>(Windows,EnumWindow);
		
		//	found obj-c window
		if ( Window )
			return;
				
		//	with swift's new scene delegate (UIWindowSceneDelegate)
		//	the app delegate is still set, but there's no window on it
		//	instead we look through the scenes connected to the app (gr: dunno how many there might be!)
		//	https://stackoverflow.com/a/59614748/355753
		
		UIScene* scene = [[[App connectedScenes] allObjects] firstObject];
		//	the default SceneDelegate from swift is a UIWindowSceneDelegate, so, assume that's the right way to do it :)
		if( [scene.delegate conformsToProtocol:@protocol(UIWindowSceneDelegate)])
		{
			auto SceneDelegate = (id <UIWindowSceneDelegate>)scene.delegate;
			//	gr: is this the var on the swift delegate? (using selector)
			//		the swift code does NOT set UIWindowSceneDelegate.window
			UIWindow* SceneWindow = [SceneDelegate window];
			if ( SceneWindow )
			{
				std::Debug << "Found SceneDelegate's window" << std::endl;
				Window = SceneWindow;
				return;
			}
		}
		else
		{
			std::Debug << "App's scene delegate is not a UIWindowSceneDelegate" << std::endl;
		}
	};
	RunJobOnMainThread( Job, true );
	return Window;
}

UIView* Platform::TWindow::GetContentView()
{
	return mContentView;
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


PlatformRect Platform::TWindow::GetChildRect(Soy::Rectx<int32_t> Rect)
{
	Soy_AssertTodo();
	/*
	//	todo: make sure this is called on mThread
	auto* ContentView = GetContentView();
	auto ParentRect = ContentView.visibleRect;

	auto Left = std::max<CGFloat>( ParentRect.origin.x, Rect.Left() );
	auto Right = std::min<CGFloat>( ParentRect.origin.x + ParentRect.size.width, Rect.Right() );

	auto Top = Rect.Top();
	auto Bottom = Rect.Bottom();

	//	rect is upside in osx!
	//	todo: incorporate origin
	if ( !ContentView.isFlipped )
	{
		Top = ParentRect.size.height - Rect.Bottom();
		Bottom = ParentRect.size.height - Rect.Top();
	}

	auto RectNs = NSMakeRect( Left, Top, Right-Left, Bottom - Top );
	return RectNs;
	*/
}

bool RecurseUIViews(UIView* View,std::function<bool(UIView*)>& EnumView);

bool RecurseUIViews(UIView* View,std::function<bool(UIView*)>& EnumView)
{
	bool FoundView;
	if ( !EnumView(View) )
	{
		FoundView = false;
		return false;
	}
	auto* Array = View.subviews;
	auto Size = [Array count];
	for ( auto i=0;	i<Size;	i++ )
	{
		auto Element = [Array objectAtIndex:i];
		if ( !RecurseUIViews( Element, EnumView ) )
		{
			FoundView = false;
			return false;
		}
	}
	
	FoundView = true;
	return true;
}

void Platform::TWindow::EnumChildren(std::function<bool(UIView*)> EnumChild)
{
	auto Job = [&]()
	{
		RecurseUIViews( mWindow, EnumChild );
	};
	RunJobOnMainThread( Job, true );
}


Soy::Rectx<int32_t> Platform::TWindow::GetScreenRect()
{
	//	get window size
	Soy_AssertTodo();
}

void Platform::TWindow::SetFullscreen(bool Fullscreen)
{
	if ( !Fullscreen )
		throw Soy::AssertException("IOS window cannot be not-fullscreen");
}

bool Platform::TWindow::IsFullscreen()
{
	//	if we start having multiple windows for storyboards/views
	//	then maybe these functions have other meanings
	return true;
}

bool Platform::TWindow::IsMinimised()
{
	//	assuming the js code wont be running if app is not foreground
	return false;
}

bool Platform::TWindow::IsForeground()
{
	//	assuming the js code wont be running if app is not foreground
	return true;
}

void Platform::TWindow::EnableScrollBars(bool Horz,bool Vert)
{
	//Soy_AssertTodo();
}

/*
void Platform::TWindow::StartRender( std::function<void()> Frame, std::string ViewName )
{
	/*
	//	todo: check type!
//	 MTKView* MetalView = Platform::TWindow::GetChild(ViewName);
	GLKView* GLView = Platform::TWindow::GetChild(ViewName);
	
	EAGLContext* context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
	
	auto delegate = [[SokolViewDelegate alloc] init:Frame];
	
	GLView.context = context;
	
	GLView.delegate = delegate;
	*/
//	GLKViewController * viewController = [[GLKViewController alloc] initWithNibName:nil bundle:nil];
//    viewController.view = GLView;
//    viewController.delegate = delegate;
    
	
//	auto* ViewController = new GLKViewController();
//	[ViewController setView: GLView];
//
//	auto sokol_view_delegate = [[SokolViewDelegate alloc] init:Frame];
//	[ViewController setDelegate:sokol_view_delegate];
/*}
*/


void Platform::TWindow::OnChildAdded(const Soy::Rectx<int32_t>& ChildRect)
{
/*
	//	expand scroll space to match child rect min/max
	auto Right = ChildRect.Right();
	auto Bottom = ChildRect.Bottom();

	auto NewSize = mContentView.frame.size;
	NewSize.width = std::max<CGFloat>( NewSize.width, Right );
	NewSize.height = std::max<CGFloat>( NewSize.height, Bottom );

	NSScrollView* ScrollView = [mWindow contentView];
	auto* ClipView = ScrollView.contentView;
	ClipView.documentView.frameSize = NewSize;
*/
}


Platform::TImageMap::TImageMap(TWindow& Parent,Soy::Rectx<int32_t>& Rect)
{
	//auto RectNs = Parent.GetChildRect(Rect);
	auto PlatformRect = CGRectMake( Rect.x, Rect.y, Rect.w, Rect.h );

	//	samee as NSImageView
	mControl = [[UIImageView alloc] initWithFrame:PlatformRect];

	[mControl retain];
		
	//	image map stretches
	//[mControl setImageScaling:NSImageScaleAxesIndependently];
	mControl.contentMode = UIViewContentModeScaleToFill;

	//mControl.target = mResponder;
	//mControl.action = @selector(OnAction);
	
	AddToParent( Parent );
}

Platform::TImageMap::TImageMap(UIView* View)
{
	SetControl(View);
	/*
	UIView *view = [[UIView alloc] initWithFrame:self.view.bounds];
view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
[self.view addSubview:view];
*/
	//	gr: this isn't a UIControl, we can't get events
	//		maybe need to add a child control and catch it's events
	//	setup delegate/responder
	//auto ListenEvents = UIControlEventAllEvents;
	//auto ListenEvents = UIControlEventEditingChanged;
	//	gr: do we want this? or should event only fire on user-change?
	//ListenEvents |= UIControlEventValueChanged;
 	//[mControl addTarget:mResponder action:@selector(OnAction) forControlEvents:ListenEvents];
 
	//mResponder->mCallback = [this]()	{	this->OnChanged();	};
}

//UICollectionViewDataSource

std::string Platform::GetClassName(UIView* View)
{
	auto* Class = [View class];
	auto ClassNameNs = NSStringFromClass(Class);
	auto ClassName = Soy::NSStringToString(ClassNameNs);
	return ClassName;
}






template<typename NATIVECLASS>
void PlatformControl<NATIVECLASS>::AddToParent(Platform::TWindow& Parent)
{
	//	gr: here if parent is a cell controller, we need to add a cell for this.
	auto* ParentView = Parent.GetContentView();
	
	auto ClassName = Platform::GetClassName(ParentView);
	if ( ClassName == "UICollectionView" )
	{
		UICollectionView* CollectionView = (UICollectionView*)ParentView;

		if ( !CollectionView.dataSource )
			CollectionView.dataSource = [Views_DataSource alloc];
		Views_DataSource* DataSourceViews = CollectionView.dataSource;		
		[DataSourceViews AddChild:mControl];
    	//[CollectionView addSubview:mCell];
    	
		//	trigger re-layout
    	[CollectionView reloadData];
    	if ( CollectionView.collectionViewLayout )
		{
			[CollectionView.collectionViewLayout invalidateLayout];
		}
	}
	else
	{
		[ParentView addSubview:mControl];
		//Parent.OnChildAdded( Rect );
	}
}


template<typename NATIVECLASS>
void PlatformControl<NATIVECLASS>::SetControl(UIView* View)
{
	auto ExpectedClass = [NATIVECLASS class];
	auto ExpectedClassName = Soy::NSStringToString(NSStringFromClass(ExpectedClass));
	
	if ( !View )
		throw Soy::AssertException("View is null");

	auto ViewClassName = Soy::NSStringToString(NSStringFromClass([View class]));
	
	if ( ![View isKindOfClass:ExpectedClass] )
	{
		std::stringstream Error;
		Error << "Trying to assign " << ViewClassName << " to " << ExpectedClassName;
		throw Soy::AssertException(Error);
	}
	
	//	cast to remove warning
	mControl = (NATIVECLASS*)View;
}

template<typename NATIVECLASS>
void PlatformControl<NATIVECLASS>::SetVisible(bool Visible)
{
	auto Job = [=]()
	{
		//	gr: if this is in a cell and the only thing in the cell, we need to
		//		hide it. put this in some parent callback here  
		mControl.hidden = Visible ? NO : YES;
	};
	Platform::RunJobOnMainThread( Job, false );
}

template<typename TYPE>
UIColor* GetColour(TYPE* Member,const vec3x<uint8_t>& Rgb)
{
	CGFloat r = Rgb[0] / 255.f;
	CGFloat g = Rgb[1] / 255.f;
	CGFloat b = Rgb[2] / 255.f;
	CGFloat a = 1.0f;
		
	//	get existing alpha
	[Member getRed:nil green:nil blue:nil alpha:&a];

	auto* Colour = [UIColor colorWithRed:r green:g blue:b alpha:a];
	return Colour;
}


template<typename NATIVECLASS>
void PlatformControl<NATIVECLASS>::SetColour(const vec3x<uint8_t>& Rgb)
{
	auto Job = [=]() mutable
	{
		mControl.tintColor = GetColour( mControl.tintColor, Rgb );
		
		//	update label if object has one
		//	gr: this isn't working on custom button, they need to be system buttons
		//	probably need to use 
		//	- (UIColor *)titleColorForState:(UIControlState)state;
		//	for complete ness
		if ( [mControl respondsToSelector:NSSelectorFromString(@"titleLabel")])
		{
			auto* Label = (UIView*)[mControl titleLabel];
			//	gr: this doesn't actually do anything, it doesn't tint text
			Label.tintColor = GetColour( Label.tintColor, Rgb );
			
			if ( [Label isKindOfClass:[UILabel class]] )
			{
				auto* LabelLabel = (UILabel*)Label;
				LabelLabel.textColor = GetColour( LabelLabel.textColor, Rgb );
			}
		}

	/*
		if ( [mControl respondsToSelector:NSSelectorFromString(@"titleColorForState")])
		{
			//	change colour of current state
			auto State = mControl.state;	<-- no such thing
			auto* OldColour = [mControl titleColorForState:State];
			auto NewColour = GetColour( OldColour, Rgb );
			[mControl setTitleColor:NewColour forState:State];
		}
		*/
		if ( [mControl isKindOfClass:[UILabel class]] )
		{
			auto* LabelLabel = (UILabel*)mControl;
			LabelLabel.textColor = GetColour( LabelLabel.textColor, Rgb );
		}
		
		if ( [mControl isKindOfClass:[UILabel class]] )
		{
			auto* LabelLabel = (UILabel*)mControl;
			LabelLabel.textColor = GetColour( LabelLabel.textColor, Rgb );
		}
		
	};
	Platform::RunJobOnMainThread( Job, false );
}


Platform::TImageMap::~TImageMap()
{
	[mControl release];

	FreeImage();
}

void Platform::TImageMap::SetRect(const Soy::Rectx<int32_t>& Rect)
{
}

void Platform::TImageMap::SetImage(const SoyPixelsImpl& _Pixels)
{
	//	make a copy as we're copying to a thread, and re-using the data for CGDataProviderCreateWithData
	//	todo: optimise this so we re-use the same buffer and maybe can just refresh the view with new pixels?
	mPixelsImage.Copy(_Pixels);

	auto Run = [this]() mutable
	{
		if ( !mControl )
			throw Soy::AssertException("Control hasn't been created yet");

		auto& Pixels = mPixelsImage;
		FreeImage();

		auto PixelMeta = Pixels.GetMeta();
		auto& PixelArray = Pixels.GetPixelsArray();
		CGDataProviderRef provider = CGDataProviderCreateWithData( nullptr, PixelArray.GetArray(), PixelArray.GetDataSize(), nullptr );
		size_t bitsPerComponent = 8 * PixelMeta.GetBytesPerChannel();
		size_t bitsPerPixel = 8 * PixelMeta.GetPixelDataSize();
		size_t bytesPerRow = PixelMeta.GetRowDataSize();

		CGBitmapInfo bitmapInfo = 0;
		CGColorSpaceRef colorSpaceRef = nullptr;
		GetColourSpace( colorSpaceRef, bitmapInfo, PixelMeta.GetFormat() );
		CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;

		mCgImage = CGImageCreate( PixelMeta.GetWidth(),
										PixelMeta.GetHeight(),
										bitsPerComponent,
										bitsPerPixel,
										bytesPerRow,
										colorSpaceRef,
										bitmapInfo,
										provider,   // data provider
										NULL,       // decode
										YES,        // should interpolate
										renderingIntent);
		if ( !mCgImage )
		{
			throw Soy::AssertException("Failed to create CGImage");
		}

		/*
		- (instancetype)initWithCGImage:(CGImageRef)cgImage;
- (instancetype)initWithCGImage:(CGImageRef)cgImage scale:(CGFloat)scale orientation:(UIImageOrientation)orientation API_AVAILABLE(ios(4.0));
#if __has_include(<CoreImage/CoreImage.h>)
- (instancetype)initWithCIImage:(CIImage *)ciImage API_AVAILABLE(ios(5.0));
- (instancetype)initWithCIImage:(CIImage *)ciImage scale:(CGFloat)scale orientation:(UIImageOrientation)orientation API_AVAILABLE(ios(6.0));
#endif
		*/
		mNsImage = [[UIImage alloc] initWithCGImage:mCgImage];
		//auto ImageValid = [mNsImage isValid];	//	osx only

		//mControl.frame = NSMakeRect( 0,0,100,100);
		//mControl.layer.backgroundColor = CGColorCreateGenericRGB(1,0,0,1);
		//mControl.image = mNsImage;
		[mControl setImage:mNsImage];

		//	gr: this selector is going missing?? (but succeeds)
		UIImageView* pControl = mControl;
		auto ViewIsValid = true;
		//auto ViewIsValid = [pControl isValid];
		//ViewIsValid = [pControl isValid];
		//auto ViewIsValid = [pControl isValid];
		if ( !ViewIsValid )
			throw Soy::AssertException("NSViewImage is not valid");
	};
	RunJobOnMainThread( Run, true );
}

void Platform::TImageMap::SetCursorMap(const SoyPixelsImpl& CursorMap,const ArrayBridge<std::string>&& CursorIndexes)
{
}

void  Platform::TImageMap::FreeImage()
{
	if ( mCgImage )
	{
		CGImageRelease(mCgImage);
		mCgImage = nullptr;
	}

	if ( mNsImage )
	{
		[mNsImage release];
		mNsImage = nullptr;
	}
}

class Platform::TList : public Gui::TList, public PlatformControl<NSMutableArray<NSString*>>
{
public:
    TList(TWindow& Parent);
    
    virtual void				SetValue(const ArrayBridge<std::string>&& Value);
    virtual void				GetValue(ArrayBridge<std::string>&& Values);
};

Platform::TList::TList(TWindow& Parent)
{
	auto NewValue = [[NSMutableArray alloc] init];
	mControl = NewValue;
}

void Platform::TList::SetValue(const ArrayBridge<std::string>&& Value)
{
	auto NewValue = [[NSMutableArray alloc] init];
	for ( auto a=0; a < Value.GetSize(); a++ )
	{
		auto* Arg = Soy::StringToNSString(Value[a]);
		[NewValue addObject:Arg];
	}
	mControl = NewValue;
}

void Platform::TList::GetValue(ArrayBridge<std::string>&& Values)
{
	for (id item in mControl) 
	{
		auto string = Soy::NSStringToString(item);
		Values.PushBack(string);
	}
}
/*
std::shared_ptr<Gui::TList> Platform::GetList(SoyWindow& Parent, const ArrayBridge<std::string>&& Value)
{
    auto& ParentView = dynamic_cast<Platform::TWindow&>(Parent);
    std::shared_ptr<Gui::TList> Control;
    auto Allocate = [&]() mutable
    {
        Control.reset( new Platform::TList( ParentView, GetArrayBridge(Value) ) );
    };
    RunJobOnMainThread(Allocate,true);
    return Control;
}
*/
std::shared_ptr<Gui::TList> Platform::GetList(SoyWindow& Parent, const std::string& Name)
{
    return Swift::GetList(Name);
}
