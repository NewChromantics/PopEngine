#pragma once
#include "TBind.h"


//	base gui control classes
class SoyWindow;
class SoySlider;
class SoyLabel;
class SoyTextBox;
class SoyTickBox;
class SoyColourButton;

namespace Gui
{
	class TColourPicker;
	class TImageMap;

	class TMouseEvent;
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
	DECLARE_BIND_TYPENAME(Colour);
	DECLARE_BIND_TYPENAME(ImageMap);

	class TGuiControlWrapper;
	
	class TWindowWrapper;
	class TSliderWrapper;
	class TLabelWrapper;
	class TTextBoxWrapper;
	class TTickBoxWrapper;
	class TColourButtonWrapper;
	class TColourPickerWrapper;
	class TImageMapWrapper;
}



class ApiGui::TGuiControlWrapper
{
public:
	TGuiControlWrapper();
	
	void			WaitForDragDrop(Bind::TCallback& Arguments);

	bool			OnTryDragDrop(const ArrayBridge<std::string>& Filenames);
	void			OnDragDrop(const ArrayBridge<std::string>& Filenames);
	
protected:
	virtual Bind::TObjectWrapperBase&	GetObjectWrapper()=0;
	
private:
	Bind::TPromiseQueueObjects<Array<std::string>>	mOnDragDropPromises;
};


class ApiGui::TWindowWrapper : public Bind::TObjectWrapper<ApiGui::BindType::Gui_Window,SoyWindow>, public TGuiControlWrapper
{
public:
	TWindowWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
	void			GetScreenRect(Bind::TCallback& Params);
	void			SetFullscreen(Bind::TCallback& Params);
	void			EnableScrollbars(Bind::TCallback& Params);

protected:
	//	TGuiControlWrapper
	virtual Bind::TObjectWrapperBase&	GetObjectWrapper() override	{	return *this;	}

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

	//	FinalValue means say, mouse-up on slider (false if dragging)
	void			OnChanged(uint16_t& NewValue,bool FinalValue);
	
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
	void			OnClosed();

public:
	std::shared_ptr<Gui::TColourPicker>&	mControl = mObject;
};



class ApiGui::TColourButtonWrapper : public Bind::TObjectWrapper<ApiGui::BindType::Colour, SoyColourButton>
{
public:
	TColourButtonWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void			SetValue(Bind::TCallback& Params);

	//	FinalValue means say, mouse-up on slider (false if dragging)
	void			OnChanged(vec3x<uint8_t>& NewValue, bool FinalValue);

public:
	std::shared_ptr<SoyColourButton>&	mColourButton = mObject;
};


class ApiGui::TImageMapWrapper : public Bind::TObjectWrapper<ApiGui::BindType::ImageMap, Gui::TImageMap>
{
public:
	TImageMapWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void			SetImage(Bind::TCallback& Params);
	void			SetCursorMap(Bind::TCallback& Params);
	void			WaitForMouseEvent(Bind::TCallback& Params);

protected:
	void			OnMouseEvent(Gui::TMouseEvent& Event);
	void			FlushMouseEvents();

public:
	Bind::TPromiseQueue					mMouseEventRequests;
	std::mutex							mMouseEventsLock;
	Array<Gui::TMouseEvent>				mMouseEvents;
	std::shared_ptr<Gui::TImageMap>&	mControl = mObject;
};
