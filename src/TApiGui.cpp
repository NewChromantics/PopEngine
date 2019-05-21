#include "TApiGui.h"



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


class Platform::TSlider
{
public:
	TSlider()	{}
	
	void	SetMinMax(int Min,int Max)	{}
	void	SetValue(int Value)	{}
};



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
	
	//mSlider.reset( new Platform::TSlider( ParentWindow.mWindow, GetArrayBridge(Rect4) ) );
	mSlider.reset( new Platform::TSlider() );
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
}
