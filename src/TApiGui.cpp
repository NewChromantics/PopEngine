#include "TApiGui.h"
#include "SoyOpenglWindow.h"


namespace ApiGui
{
	const char Namespace[] = "Pop.Gui";

	DEFINE_BIND_TYPENAME(Gui_Window);
	DEFINE_BIND_TYPENAME(Slider);
	DEFINE_BIND_FUNCTIONNAME(SetMinMax);
	DEFINE_BIND_FUNCTIONNAME(SetValue);
}


void ApiGui::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<TWindowWrapper>( Namespace, "Window" );
	Context.BindObjectType<TSliderWrapper>( Namespace );
}


void ApiGui::TSliderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<SetMinMax_FunctionName>( &TSliderWrapper::SetMinMax );
	Template.BindFunction<SetValue_FunctionName>( &TSliderWrapper::SetValue );

}

void ApiGui::TSliderWrapper::Construct(Bind::TCallback& Params)
{
	auto ParentWindow = Params.GetArgumentPointer<TWindowWrapper>(0);

	BufferArray<int32_t,4> Rect4;
	Params.GetArgumentArray( 1, GetArrayBridge(Rect4) );
	Soy::Rectx<int32_t> Rect( Rect4[0], Rect4[1], Rect4[2], Rect4[3] );
	
	mSlider = Platform::CreateSlider( *ParentWindow.mWindow, Rect );
}

void ApiGui::TSliderWrapper::SetMinMax(Bind::TCallback& Params)
{
	auto Min = Params.GetArgumentInt(0);
	auto Max = Params.GetArgumentInt(1);
	mSlider->SetMinMax( Min, Max );
}

void ApiGui::TSliderWrapper::SetValue(Bind::TCallback& Params)
{
	auto Value = Params.GetArgumentInt(0);
	mSlider->SetValue( Value );
}



void ApiGui::TWindowWrapper::CreateTemplate(Bind::TTemplate& Template)
{
}

void ApiGui::TWindowWrapper::Construct(Bind::TCallback& Params)
{
	std::string WindowName = GetTypeName();
	if ( !Params.IsArgumentUndefined(0) )
		WindowName = Params.GetArgumentString(0);

	//	named options
	if ( Params.IsArgumentObject(1) )
	{
		/*
		auto WindowParamsObject = Params.GetArgumentObject(1);
		if ( WindowParamsObject.HasMember("Fullscreen") )
			WindowParams.mFullscreen = WindowParamsObject.GetBool("Fullscreen");
		*/
	}
	
	Soy::Rectx<int32_t> Rect(0, 0, 0, 0);
	
	//	if no rect, get rect from screen
	if ( !Params.IsArgumentUndefined(2) )
	{
		BufferArray<int32_t,4> Rect4;
		Params.GetArgumentArray(2, GetArrayBridge(Rect4) );
		Rect.x = Rect4[0];
		Rect.y = Rect4[1];
		Rect.w = Rect4[2];
		Rect.h = Rect4[3];
	}
	else
	{
		//	get first monitor size
		auto SetRect = [&](const Platform::TScreenMeta& Screen)
		{
			if ( Rect.w > 0 )
				return;
			auto BorderX = Screen.mWorkRect.w / 4;
			auto BorderY = Screen.mWorkRect.h / 4;
			Rect.x = Screen.mWorkRect.x + BorderX;
			Rect.y = Screen.mWorkRect.y + BorderY;
			Rect.w = Screen.mWorkRect.w - BorderX - BorderX;
			Rect.h = Screen.mWorkRect.h - BorderY - BorderY;
		};
		Platform::EnumScreens(SetRect);
	}
	
	mWindow = Platform::CreateWindow( WindowName, Rect );
	
	/*
	mWindow->mOnRender = OnRender;
	mWindow->mOnMouseDown = [this](const TMousePos& Pos,SoyMouseButton::Type Button)	{	this->OnMouseFunc(Pos,Button,"OnMouseDown");	};
	mWindow->mOnMouseUp = [this](const TMousePos& Pos,SoyMouseButton::Type Button)		{	this->OnMouseFunc(Pos,Button,"OnMouseUp");	};
	mWindow->mOnMouseMove = [this](const TMousePos& Pos,SoyMouseButton::Type Button)	{	this->OnMouseFunc(Pos,Button,"OnMouseMove");	};
	mWindow->mOnKeyDown = [this](SoyKeyButton::Type Button)			{	this->OnKeyFunc(Button,"OnKeyDown");	};
	mWindow->mOnKeyUp = [this](SoyKeyButton::Type Button)			{	this->OnKeyFunc(Button,"OnKeyUp");	};
	mWindow->mOnTryDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	return this->OnTryDragDrop(Filenames);	};
	mWindow->mOnDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	this->OnDragDrop(Filenames);	};
	mWindow->mOnClosed = [this]()	{	this->OnClosed();	};
	*/
}
