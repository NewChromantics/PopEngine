#pragma once
#include "TBind.h"


//	base gui control classes
class SoyWindow;
class SoySlider;
class SoyLabel;
class SoyTextBox;
class SoyTickBox;

namespace Gui
{
	class TColourPicker;
}

namespace ApiGui
{
	void	Bind(Bind::TContext& Context);

	DECLARE_BIND_TYPENAME(Gui_Window);
	DECLARE_BIND_TYPENAME(Slider);
	DECLARE_BIND_TYPENAME(Label);
	DECLARE_BIND_TYPENAME(TextBox);
	DECLARE_BIND_TYPENAME(TickBox);
	DECLARE_BIND_TYPENAME(ColourPicker);

	class TWindowWrapper;
	class TSliderWrapper;
	class TLabelWrapper;
	class TTextBoxWrapper;
	class TTickBoxWrapper;
	class TColourPickerWrapper;
}



class ApiGui::TWindowWrapper : public Bind::TObjectWrapper<ApiGui::BindType::Gui_Window,SoyWindow>
{
public:
	TWindowWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
	void			SetFullscreen(Bind::TCallback& Params);
	void			EnableScrollbars(Bind::TCallback& Params);
	
public:
	std::shared_ptr<SoyWindow>&	mWindow = mObject;
};


class ApiGui::TSliderWrapper : public Bind::TObjectWrapper<ApiGui::BindType::Slider,SoySlider>
{
public:
	TSliderWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
	void			SetMinMax(Bind::TCallback& Params);
	void			SetValue(Bind::TCallback& Params);

	void			OnChanged(uint16_t& NewValue);
	
public:
	std::shared_ptr<SoySlider>&	mSlider = mObject;
};




class ApiGui::TLabelWrapper : public Bind::TObjectWrapper<ApiGui::BindType::Label,SoyLabel>
{
public:
	TLabelWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
	void			SetValue(Bind::TCallback& Params);
	
public:
	std::shared_ptr<SoyLabel>&	mLabel = mObject;
};


class ApiGui::TTextBoxWrapper : public Bind::TObjectWrapper<ApiGui::BindType::TextBox,SoyTextBox>
{
public:
	TTextBoxWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
	void			SetValue(Bind::TCallback& Params);

	void			OnChanged(const std::string& NewValue);

public:
	std::shared_ptr<SoyTextBox>&	mTextBox = mObject;
};



class ApiGui::TTickBoxWrapper : public Bind::TObjectWrapper<ApiGui::BindType::TickBox,SoyTickBox>
{
public:
	TTickBoxWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
	void			SetValue(Bind::TCallback& Params);
	void			SetLabel(Bind::TCallback& Params);

	void			OnChanged(bool& NewValue);
	
public:
	std::shared_ptr<SoyTickBox>&	mControl = mObject;
};


class ApiGui::TColourPickerWrapper : public Bind::TObjectWrapper<ApiGui::BindType::ColourPicker,Gui::TColourPicker>
{
public:
	TColourPickerWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	~TColourPickerWrapper();
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void			OnChanged(vec3x<uint8_t>& NewValue);
	void			OnClosed(vec3x<uint8_t>& NewValue);

public:
	std::shared_ptr<Gui::TColourPicker>&	mControl = mObject;
};

