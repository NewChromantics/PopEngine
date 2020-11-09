#include "SoyGui.h"
#include "SoyWindowApple.h"

#include "PopMain.h"
#include <TargetConditionals.h>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import <GLKit/GLKit.h>

#include "sokol/sokol_gfx.h"

#import <UIKit/UICollectionView.h>	//	UICollectionViewDataSource


namespace Platform
{
	static size_t	UniqueIdentifierCounter = 1000;
	std::string		GetUniqueIdentifier();
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
	[CellView addSubview:Child];
	
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
	void		AddToParent(Platform::TWindow& Window);
	
	UICollectionViewCell*	mCell = nullptr;	//	if we're inside a cell in a parent
	NATIVECLASS*			mControl = nullptr;
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

class Platform::TTextBox : public SoyTextBox
{
public:
	TTextBox(UIView* View);
	~TTextBox();
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override;
	
	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;

	TResponder*			mResponder = [TResponder alloc];
	UITextView*			mView = nullptr;
};

class Platform::TButton : public SoyButton
{
public:
	TButton(UIView* View);
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect);// override;
	
	virtual void		SetLabel(const std::string& Value) override;

	TResponder*			mResponder = [TResponder alloc];
	UIButton*			mView = nullptr;
};

class Platform::TMetalView : public SoyMetalView
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
	
	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override;
	virtual void		SetImage(const SoyPixelsImpl& Pixels) override;
	virtual void		SetCursorMap(const SoyPixelsImpl& CursorMap,const ArrayBridge<std::string>&& CursorIndexes) override;

	TResponder*			mResponder = [TResponder alloc];
};


std::shared_ptr<SoyMetalView> Platform::GetMetalView(SoyWindow& Parent, const std::string& Name)
{
	std::shared_ptr<SoyMetalView> MetalView;
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
		if ( !View )
			throw Soy::AssertException(std::string("No view found named ") + Name);
		Label.reset( new Platform::TLabel(View) );
	};
	RunJobOnMainThread( Run, true );
	return Label;
}

std::shared_ptr<SoyButton> Platform::CreateButton(SoyWindow &Parent, Soy::Rectx<int32_t> &Rect)
{
	Soy_AssertTodo();
}

std::shared_ptr<SoyButton> Platform::GetButton(SoyWindow& Parent,const std::string& Name)
{
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
	auto& ParentView = dynamic_cast<Platform::TWindow&>(Parent);
	std::shared_ptr<Gui::TImageMap> ImageMap;
	auto Allocate = [&]() mutable
	{
		ImageMap.reset( new Platform::TImageMap(ParentView,Rect) );
	};
	RunJobOnMainThread(Allocate,true);
	return ImageMap;
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




Platform::TTextBox::TTextBox(UIView* View)
{
	//	todo: check type!
	mView = View;
	
	//	setup delegate/responder
	//auto ListenEvents = UIControlEventAllEvents;
	auto ListenEvents = UIControlEventEditingChanged;
	//	gr: do we want this? or should event only fire on user-change?
	//ListenEvents |= UIControlEventValueChanged;
 	[mView addTarget:mResponder action:@selector(OnAction) forControlEvents:ListenEvents];
 
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
		this->mView.text = Soy::StringToNSString(Value);
	};
	RunJobOnMainThread( Job, false );
}

std::string Platform::TTextBox::GetValue()
{
	std::string Value;
	auto Job = [&]()
	{
		Value = Soy::NSStringToString( mView.text );
	};
	RunJobOnMainThread( Job, true );
	return Value;
}




Platform::TButton::TButton(UIView* View)
{
	//	this IOS code expects to be run on the main thread
	//	the OSX code can run on any thread, and then inits on main thread (before we had Platform::CreateXXX)
	//	we should align the paradigms

	//	todo: check type!
	mView = View;
	
	//	setup delegate/responder to get click
	//auto ListenEvents = UIControlEventAllEvents;
	auto ListenEvents = UIControlEventTouchUpInside;
 	[mView addTarget:mResponder action:@selector(OnAction) forControlEvents:ListenEvents];
 
	mResponder->mCallback = [this]()	{	this->OnClicked();	};
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
		[this->mView setTitle:ValueNs forState:UIControlStateNormal];
	};
	RunJobOnMainThread( Job, false );
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
	
	mWindow = GlobalWindow;
	mContentView = mWindow.rootViewController.view;
}

UIWindow* Platform::TWindow::GetWindow()
{
	UIWindow* Window = nullptr;
	auto Job = [&]()
	{
		auto* App = [UIApplication sharedApplication];
		Window = App.delegate.window;
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
	auto Job = [&]()
	{
		if ( !EnumView(View) )
		{
			FoundView = false;
			return;
		}
		auto* Array = View.subviews;
		auto Size = [Array count];
		for ( auto i=0;	i<Size;	i++ )
		{
			auto Element = [Array objectAtIndex:i];
			if ( !RecurseUIViews( Element, EnumView ) )
			{
				FoundView = false;
				return;
			}
		}
		
		FoundView = true;
		return;
	};
	RunJobOnMainThread( Job, true );
	return Job;
}

void Platform::TWindow::EnumChildren(std::function<bool(UIView*)> EnumChild)
{
	auto* Window = GetWindow();
	
	RecurseUIViews( Window, EnumChild );
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
}



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


	mControl = [[UIImageView alloc] initWithFrame:PlatformRect];

	//mControl = [[UIImageView alloc] initWithFrame:RectNs];
	//[mControl retain];
/*
	//	setup callback
	mResponder->mCallback = [this]()
	{
		this->OnTextBoxChanged();
	};
	mControl.target = mResponder;
	mControl.action = @selector(OnAction);

	//ApplyStyle();
*/
	AddToParent( Parent );
}

//UICollectionViewDataSource

template<typename NATIVECLASS>
void PlatformControl<NATIVECLASS>::AddToParent(Platform::TWindow& Parent)
{
	//	gr: here if parent is a cell controller, we need to add a cell for this.
	auto* ParentView = Parent.GetContentView();
	
	auto ClassName = Soy::NSStringToString(NSStringFromClass([ParentView class]));
	if ( ClassName == "UICollectionView" )
	{
		UICollectionView* CollectionView = (UICollectionView*)ParentView;

		if ( !CollectionView.dataSource )
			CollectionView.dataSource = [Views_DataSource alloc];
		Views_DataSource* DataSourceViews = CollectionView.dataSource;		
		[DataSourceViews AddChild:mControl];
    	//[CollectionView addSubview:mCell];
    	[CollectionView reloadData];
	}
	else
	{
		[ParentView addSubview:mControl];
		//Parent.OnChildAdded( Rect );
	}
}

void Platform::TImageMap::SetRect(const Soy::Rectx<int32_t>& Rect)
{
}

void Platform::TImageMap::SetImage(const SoyPixelsImpl& Pixels)
{
}

void Platform::TImageMap::SetCursorMap(const SoyPixelsImpl& CursorMap,const ArrayBridge<std::string>&& CursorIndexes)
{
}

