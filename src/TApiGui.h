#pragma once
#include "TBind.h"


namespace Platform
{
	class TWindow;
	class TSlider;
}

namespace ApiGui
{
	void	Bind(Bind::TContext& Context);

	DECLARE_BIND_TYPENAME(Gui_Window);
	DECLARE_BIND_TYPENAME(Slider);

	class TWindowWrapper;
	class TSliderWrapper;
}



class ApiGui::TWindowWrapper : public Bind::TObjectWrapper<ApiGui::Gui_Window_TypeName,Platform::TWindow>
{
public:
	TWindowWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
public:
	std::shared_ptr<Platform::TWindow>&	mWindow = mObject;
};


class ApiGui::TSliderWrapper : public Bind::TObjectWrapper<ApiGui::Slider_TypeName,Platform::TSlider>
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

public:
	std::shared_ptr<Platform::TSlider>&	mSlider = mObject;
};


