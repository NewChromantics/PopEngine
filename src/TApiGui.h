#pragma once
#include "TBind.h"


//	base gui control classes
class SoyWindow;
class SoySlider;
class SoyLabel;
class SoyTextBox;
class SoyTickBox;


namespace ApiGui
{
	void	Bind(Bind::TContext& Context);

	DECLARE_BIND_TYPENAME(Gui_Window);
	DECLARE_BIND_TYPENAME(Slider);
	DECLARE_BIND_TYPENAME(Label);
	DECLARE_BIND_TYPENAME(TextBox);
	DECLARE_BIND_TYPENAME(TickBox);

	class TWindowWrapper;
	class TSliderWrapper;
	class TLabelWrapper;
	class TTextBoxWrapper;
	class TTickBoxWrapper;
}



class ApiGui::TWindowWrapper : public Bind::TObjectWrapper<ApiGui::Gui_Window_TypeName,SoyWindow>
{
public:
	TWindowWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
public:
	std::shared_ptr<SoyWindow>&	mWindow = mObject;
};


class ApiGui::TSliderWrapper : public Bind::TObjectWrapper<ApiGui::Slider_TypeName,SoySlider>
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




class ApiGui::TLabelWrapper : public Bind::TObjectWrapper<ApiGui::Label_TypeName,SoyLabel>
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


class ApiGui::TTextBoxWrapper : public Bind::TObjectWrapper<ApiGui::TextBox_TypeName,SoyTextBox>
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



class ApiGui::TTickBoxWrapper : public Bind::TObjectWrapper<ApiGui::TickBox_TypeName,SoyTickBox>
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

