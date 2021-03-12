#import "SoyGuiSwift.h"
#include <string>
#include "HeapArray.hpp"
#include "SoyString.h"
#include "SoyVector.h"
#include "SoyWindow.h"
#include "SoyGuiApple.h"


bool NSStringEquals(NSString* a,NSString* b)
{
	//	https://stackoverflow.com/questions/6969115/compare-two-nsstrings
	if ( [a isEqualToString:b] )
		return true;
	return false; 
}


namespace Swift
{
	//	gr: these might not be swift ones, but for the sake of context, let's call them that for now
	Array<PopEngineControl*>	Controls;
	PopEngineControl*			GetControl(const std::string& Name);	
	template<typename TYPE>
	TYPE*						GetControlAs(const std::string& Name);	

	//	base class for common helpers	
	class TControl;
	//	c++ soy-conforming wrappers
	class TLabel;
	class TButton;
	class TTickBox;
	class TRenderView;
    class TList;

	std::shared_ptr<SoyLabel>			GetLabel(const std::string& Name);
	std::shared_ptr<SoyButton>			GetButton(const std::string& Name);
	std::shared_ptr<SoyTickBox>			GetTickBox(const std::string& Name);
	std::shared_ptr<Gui::TRenderView>	GetRenderView(const std::string& Name);
    std::shared_ptr<Gui::TList>			GetList(const std::string& Name);
}
	
namespace Platform
{
	template<typename TYPE,typename BASETYPE>
	TYPE*			ObjcCast(BASETYPE* View);
}

template<typename TYPE,typename BASETYPE>
TYPE* Platform::ObjcCast(BASETYPE* View)
{
	auto GetClassName = [](BASETYPE* View)
	{
		auto* Class = [View class];
		auto ClassNameNs = NSStringFromClass(Class);
		auto ClassName = Soy::NSStringToString(ClassNameNs);
		return ClassName;
	};

	if ( [View isKindOfClass:[TYPE class]] )
	{
		return (TYPE*)View;
	}
		
	auto ClassName = GetClassName(View);
	std::stringstream Error;
	
	auto* TargetClass = [TYPE class];
	auto TargetClassNameNs = NSStringFromClass(TargetClass);
	auto TargetClassName = Soy::NSStringToString(TargetClassNameNs);
	
	Error << "Trying to cast " << ClassName << " to " << TargetClassName;
	throw Soy::AssertException(Error);
}


@implementation GLView
{
}
@end


@implementation PopEngineControl 

- (nonnull instancetype)initWithName:(NSString*)name
{
	self = [super init]; 
	if ( name )
		self.name = [name mutableCopy];
	else
		self.name = [@"<null>" mutableCopy];
	Swift::Controls.PushBack(self);
	std::Debug << "Added swift control " << Soy::NSStringToString(self.name) << std::endl;
	return self;
}

-(void)dealloc 
{
	std::Debug << "Removing swift control " << Soy::NSStringToString(self.name) << std::endl;
	[super dealloc];
	Swift::Controls.Remove(self);
}

- (void)updateUi
{
	std::Debug << "overload update ui" << std::endl;
}


@end




@implementation PopEngineLabel 
{
	NSString* mLabel;
}

- (nonnull instancetype)initWithName:(NSString*)name label:(NSString*)label;
{
	self = [super initWithName:name]; 
	self.label = label;
	return self;
}

- (nonnull instancetype)initWithName:(NSString*)name;
{
	return [self initWithName:name label:@"PopEngineLabel"];
}

- (nonnull instancetype)init
{
	std::Debug << "PopEngineLabel init" << std::endl;
	self = [super initWithName:@"TestLabel1"]; 
	self.label = @"basic init";
	return self;
}


- (NSString*)label
{
   return mLabel;
}

-(void) setLabel: (NSString*)value
{
	//	skip reporting unchanged value
	if ( NSStringEquals( mLabel, value ) )
		return;
		
	mLabel = value;
	[self updateUi];
}

@end


@implementation PopEngineButton
{
	@public std::function<void()>	mOnClicked;
}

- (nonnull instancetype)initWithName:(NSString*)name label:(NSString*)label;
{
	self = [super initWithName:name]; 
	self.label = label;
	return self;
}

- (nonnull instancetype)initWithName:(NSString*)name;
{
	return [self initWithName:name label:@"PopEngineButton"];
}

- (void)onClicked
{
	if ( mOnClicked )
		mOnClicked();
	else
		std::Debug << "Button(" << Soy::NSStringToString(self.name) << "/" << Soy::NSStringToString(self.label) << ") clicked" << std::endl;
}

@end



@implementation PopEngineTickBox
{
	Boolean mValue;
	@public std::function<void(bool)>	mOnChanged;
}


- (nonnull id)initWithName:(nonnull NSString*)name value:(Boolean)value label:(nonnull NSString*)label
{
	self = [super initWithName:name]; 
	self.label = label;
	self.value = value;
	return self;
}

- (nonnull id)initWithName:(nonnull NSString*)name value:(Boolean)value
{
	return [self initWithName:name value:value label:@"PopEngineButton"];
}

- (nonnull id)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label
{
	return [self initWithName:name value:false label:label];
}

- (nonnull id)initWithName:(NSString*)name
{
	return [self initWithName:name value:false label:@"PopEngineButton"];
}


- (Boolean)value
{
   return mValue;
}

-(void) setValue: (Boolean)value
{
	auto OldValue = mValue;
	mValue = value;
    
    //	gr: only notify if changed to avoid recursion?
    if ( mValue == OldValue )
    {
    	//std::Debug << "Set Value to " << (value?"true":"false") << " (was " << (OldValue?"true":"false") << ")" << std::endl;
    	return;
	}
	
	auto Value = self.value;
	if ( mOnChanged )
		mOnChanged(Value);
	else
		std::Debug << "Tickbox(" << Soy::NSStringToString(self.name) << ") changed to " << (Value?"true":"false") << std::endl;
}

@end

@implementation PopEngineList
{
    NSMutableArray<NSString*> * mValue;
//    @public std::function<void(NSMutableArray<NSString*>*)>    mOnChanged;
}


- (nonnull id)initWithName:(nonnull NSString*)name value:(NSMutableArray<NSString*>*)value label:(nonnull NSString*)label
{
    self = [super initWithName:name];
    self.label = label;
    self.value = value;
    return self;
}

- (nonnull id)initWithName:(nonnull NSString*)name value:(NSMutableArray<NSString*>*)value
{
    return [self initWithName:name value:value label:@"PopEngineStringArray"];
}

- (nonnull id)initWithName:(nonnull NSString*)name label:(nonnull NSString*)label
{
    return [self initWithName:name value:[NSMutableArray<NSString*> new] label:label];
}

- (nonnull id)initWithName:(NSString*)name
{
    return [self initWithName:name value:[NSMutableArray<NSString*> new] label:@"PopEngineStringArray"];
}


- (NSMutableArray<NSString*>*)value
{
   return mValue;
}

-(void) setValue: (NSMutableArray<NSString*>*)value
{
    mValue = value;
    [self updateUi];
}

@end


@implementation PopEngineRenderView
{
	@public std::function<void()>	mOnDraw;
}

- (nonnull instancetype)initWithName:(NSString*)name
{
	self = [super initWithName:name]; 
	return self;
}

- (void)onDraw
{
	if ( mOnDraw )
		mOnDraw();
	else
		std::Debug << "Button(" << Soy::NSStringToString(self.name) << ") draw" << std::endl;
}

@end



class Swift::TControl
{
public:
	void			SetRect(const Soy::Rectx<int32_t>& Rect)	{}
	void			SetVisible(bool Visible)					{}
	void			SetColour(const vec3x<uint8_t>& Rgb)		{}
};


class Swift::TLabel : public SoyLabel, public Swift::TControl
{
public:
	TLabel(PopEngineLabel* Control) :
		mControl	(Control)
	{
	}
	
	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override	{	Swift::TControl::SetRect(Rect);	}
	virtual void			SetVisible(bool Visible) override					{	Swift::TControl::SetVisible(Visible);	}	
	virtual void			SetColour(const vec3x<uint8_t>& Rgb) override		{	Swift::TControl::SetColour(Rgb);	} 

	virtual void			SetValue(const std::string& Value) override;
	virtual std::string		GetValue() override;

	PopEngineLabel*			mControl;
};



class Swift::TButton : public SoyButton, public Swift::TControl
{
public:
	TButton(PopEngineButton* Control) :
		mControl	(Control)
	{
		mControl->mOnClicked = [this]()
		{
			this->OnClicked();
		};
	}

	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override	{	Swift::TControl::SetRect(Rect);	}
	virtual void			SetVisible(bool Visible) override					{	Swift::TControl::SetVisible(Visible);	}	
	virtual void			SetColour(const vec3x<uint8_t>& Rgb) override		{	Swift::TControl::SetColour(Rgb);	} 

	//virtual void			SetValue(const std::string& Value) override;
	//virtual std::string		GetValue() override;
	virtual void			SetLabel(const std::string& Label) override;

	PopEngineButton*		mControl;
};



class Swift::TTickBox : public SoyTickBox, public Swift::TControl
{
public:
	TTickBox(PopEngineTickBox* Control) :
		mControl	(Control)
	{
		mControl->mOnChanged = [this](bool Value)
		{
			this->OnChanged();
		};
	}
	~TTickBox()
	{
		mControl->mOnChanged = [](bool Value)
		{
			std::Debug << "Tickbox deallocated" << std::endl;
		};
	}

	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override	{	Swift::TControl::SetRect(Rect);	}
	virtual void			SetVisible(bool Visible) override					{	Swift::TControl::SetVisible(Visible);	}	
	virtual void			SetColour(const vec3x<uint8_t>& Rgb) override		{	Swift::TControl::SetColour(Rgb);	} 

	virtual void			SetValue(bool Value) override;
	virtual bool			GetValue() override;
	virtual void			SetLabel(const std::string& Label) override;

	PopEngineTickBox*		mControl;
};


class Swift::TRenderView : public Platform::TRenderView, public Swift::TControl
{
public:
	TRenderView(PopEngineRenderView* Control) :
		mControl	(Control)
	{
		mControl->mOnDraw = [this]()
		{
			if ( this->mOnDraw )
				this->mOnDraw();
		};
	}
	~TRenderView()
	{
		mControl->mOnDraw = []()
		{
			std::Debug << "TRenderView deallocated" << std::endl;
		};
	}
	
	virtual GLView*			GetOpenglView() override	{	return mControl ? mControl.openglView : nullptr;	}
	virtual MTKView*		GetMetalView() override		{	return mControl ? mControl.metalView : nullptr;	}
	//virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override	{	Swift::TControl::SetRect(Rect);	}
	//virtual void			SetVisible(bool Visible) override					{	Swift::TControl::SetVisible(Visible);	}	
	//virtual void			SetColour(const vec3x<uint8_t>& Rgb) override		{	Swift::TControl::SetColour(Rgb);	} 

	//virtual void			SetValue(const std::string& Value) override;
	//virtual std::string		GetValue() override;

	PopEngineRenderView*	mControl;
};

class Swift::TList : public Gui::TList
{
public:
	TList(PopEngineList* Control) :
		mControl	(Control)
	 {
    }
    
    virtual void		SetValue(const ArrayBridge<std::string>&& Value) override;
    virtual void		GetValue(ArrayBridge<std::string>&& Values) override;
    

    PopEngineList*   mControl;
};


template<typename TYPE>
TYPE* Swift::GetControlAs(const std::string& Name)
{
	auto* BaseControl = GetControl(Name);
	return Platform::ObjcCast<TYPE>(BaseControl);
}	


PopEngineControl* Swift::GetControl(const std::string& Name)
{
	auto NameMs = Soy::StringToNSString(Name);
	for ( auto i=0;	i<Controls.GetSize();	i++ )
	{
		auto* Control = Controls[i];
		auto* ControlName = Control.name;
		
		//	gr: wow, can't do nsstring != nsstring
		//if ( ControlName != NameMs )
		if (!NSStringEquals( NameMs, ControlName ) ) 
			continue;
		return Control;
	}
	std::stringstream Error;
	Error << "No swift control found named " << Name;
	throw Soy::AssertException(Error);
}


std::shared_ptr<SoyLabel> Swift::GetLabel(const std::string& Name)
{
	auto* Control = GetControlAs<PopEngineLabel>(Name);
	return std::shared_ptr<SoyLabel>( new TLabel(Control) );
}

std::shared_ptr<SoyButton> Swift::GetButton(const std::string& Name)
{
	auto* Control = GetControlAs<PopEngineButton>(Name);
	return std::shared_ptr<SoyButton>( new TButton(Control) );
}

std::shared_ptr<SoyTickBox> Swift::GetTickBox(const std::string& Name)
{
	auto* Control = GetControlAs<PopEngineTickBox>(Name);
	return std::shared_ptr<SoyTickBox>( new TTickBox(Control) );
}

std::shared_ptr<Gui::TRenderView> Swift::GetRenderView(const std::string& Name)
{
	auto* Control = GetControlAs<PopEngineRenderView>(Name);
	return std::shared_ptr<Gui::TRenderView>( new TRenderView(Control) );
}

std::shared_ptr<Gui::TList> Swift::GetList(const std::string& Name)
{
    auto* Control = GetControlAs<PopEngineList>(Name);
    return std::shared_ptr<Gui::TList>( new TList(Control) );
}
	
void RunJobOnMainThread(std::function<void()> Lambda,bool Block);

void Swift::TLabel::SetValue(const std::string& Value)
{
	auto* Label = Soy::StringToNSString(Value);
	auto SetLabel = [=]()
	{
		mControl.label = Label;
	};
	RunJobOnMainThread(SetLabel,false);
}

std::string Swift::TLabel::GetValue()
{
	auto* Label = mControl.label;
	return Soy::NSStringToString(Label);
}


void Swift::TButton::SetLabel(const std::string& Value)
{
	auto* Label = Soy::StringToNSString(Value);
	mControl.label = Label;
}


void Swift::TTickBox::SetValue(bool Value)
{
	mControl.value = Value;
}

bool Swift::TTickBox::GetValue()
{
	return mControl.value;
}

void Swift::TTickBox::SetLabel(const std::string& Value)
{
	auto* Label = Soy::StringToNSString(Value);
	mControl.label = Label;
}

void Swift::TList::GetValue(ArrayBridge<std::string>&& Values)
{
    for (id item in mControl.value) 
    {
        auto string = Soy::NSStringToString(item);
        Values.PushBack(string);
    }
}

void Swift::TList::SetValue(const ArrayBridge<std::string>&& Values)
{
	auto ValuesNs = [[NSMutableArray alloc] init];
	for ( auto a=0; a < Values.GetSize(); a++ )
	{
		auto* item = Soy::StringToNSString(Values[a]);
		[ValuesNs addObject:item];
	}

	auto SetValue = [=]()
	{
		mControl.value = ValuesNs;
	};
	RunJobOnMainThread(SetValue,false);
}
