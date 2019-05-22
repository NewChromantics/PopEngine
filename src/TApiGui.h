#pragma once
#include "TBind.h"


class SoyWindow;
class SoySlider;


namespace ApiGui
{
	void	Bind(Bind::TContext& Context);

	DECLARE_BIND_TYPENAME(Gui_Window);
	DECLARE_BIND_TYPENAME(Slider);

	class TWindowWrapper;
	class TSliderWrapper;
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


