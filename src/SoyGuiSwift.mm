#import "SoyGuiSwift.h"
#include <string>
#include "HeapArray.hpp"
#include "SoyString.h"
#include "SoyVector.h"
#include "SoyWindow.h"

namespace Swift
{
	//	gr: these might not be swift ones, but for the sake of context, let's call them that for now
	Array<PopEngineControl*>	Controls;
	PopEngineControl*			GetControl(const std::string& Name);	

	//	base class for common helpers	
	class TControl;
	//	c++ soy-conforming wrappers
	class TLabel;
	class TButton;
	class TTickBox;

	std::shared_ptr<SoyLabel>	GetLabel(const std::string& Name);
	std::shared_ptr<SoyButton>	GetButton(const std::string& Name);
	std::shared_ptr<SoyTickBox>	GetTickBox(const std::string& Name);
}
	




@implementation PopEngineControl 

- (nonnull id)initWithName:(NSString*)name
{
	self = [super init]; 
	if ( name )
		self.name = name;
	else
		self.name = @"<null>";
	Swift::Controls.PushBack(self);
	return self;
}

-(void)dealloc 
{
	[super dealloc];
	Swift::Controls.Remove(self);
}

@end




@implementation PopEngineLabel 

- (nonnull id)initWithName:(NSString*)name label:(NSString*)label;
{
	self = [super initWithName:name]; 
	self.label = label;
	return self;
}

- (nonnull id)initWithName:(NSString*)name;
{
	return [self initWithName:name label:@"PopEngineLabel"];
}

@end


@implementation PopEngineButton
{
	@public std::function<void()>	mOnClicked;
}

- (nonnull id)initWithName:(NSString*)name label:(NSString*)label;
{
	self = [super initWithName:name]; 
	self.label = label;
	return self;
}

- (nonnull id)initWithName:(NSString*)name;
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
    std::Debug << "Set Value to " << (value?"true":"false") << " (was " << (OldValue?"true":"false") << ")" << std::endl;

	auto Value = self.value;
	if ( mOnChanged )
		mOnChanged(Value);
	else
		std::Debug << "Tickbox(" << Soy::NSStringToString(self.name) << ") changed to " << (Value?"true":"false") << std::endl;
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

	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override	{	Swift::TControl::SetRect(Rect);	}
	virtual void			SetVisible(bool Visible) override					{	Swift::TControl::SetVisible(Visible);	}	
	virtual void			SetColour(const vec3x<uint8_t>& Rgb) override		{	Swift::TControl::SetColour(Rgb);	} 

	virtual void			SetValue(bool Value) override;
	virtual bool			GetValue() override;
	virtual void			SetLabel(const std::string& Label) override;

	PopEngineTickBox*		mControl;
};






PopEngineControl* Swift::GetControl(const std::string& Name)
{
	auto NameMs = Soy::StringToNSString(Name);
	for ( auto i=0;	i<Controls.GetSize();	i++ )
	{
		auto* Control = Controls[i];
		auto* ControlName = Control.name;
		
		//	gr: wow, can't do nsstring != nsstring
		//if ( ControlName != NameMs )
		if ([NameMs compare:ControlName] != NSOrderedSame) 
			continue;
		return Control;
	}
	std::stringstream Error;
	Error << "No swift control found named " << Name;
	throw Soy::AssertException(Error);
}


std::shared_ptr<SoyLabel> Swift::GetLabel(const std::string& Name)
{
	auto* Control = GetControl(Name);
	return std::shared_ptr<SoyLabel>( new TLabel(Control) );
}

std::shared_ptr<SoyButton> Swift::GetButton(const std::string& Name)
{
	auto* Control = GetControl(Name);
	return std::shared_ptr<SoyButton>( new TButton(Control) );
}

std::shared_ptr<SoyTickBox> Swift::GetTickBox(const std::string& Name)
{
	auto* Control = GetControl(Name);
	return std::shared_ptr<SoyTickBox>( new TTickBox(Control) );
}

	

void Swift::TLabel::SetValue(const std::string& Value)
{
	auto* Label = Soy::StringToNSString(Value);
	mControl.label = Label;
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
