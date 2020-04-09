#include "TApiGui.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"


namespace ApiGui
{
	const char Namespace[] = "Pop.Gui";

	DEFINE_BIND_TYPENAME(Gui_Window);
	DEFINE_BIND_TYPENAME(Slider);
	DEFINE_BIND_TYPENAME(Label);
	DEFINE_BIND_TYPENAME(TextBox);
	DEFINE_BIND_TYPENAME(TickBox);
	DEFINE_BIND_TYPENAME(ColourPicker);
	DEFINE_BIND_TYPENAME(Colour);
	DEFINE_BIND_TYPENAME(ImageMap);

	DEFINE_BIND_FUNCTIONNAME(SetMinMax);
	DEFINE_BIND_FUNCTIONNAME(SetValue);
	DEFINE_BIND_FUNCTIONNAME(SetLabel);

	DEFINE_BIND_FUNCTIONNAME(SetImage);
	DEFINE_BIND_FUNCTIONNAME(SetCursorMap);
	DEFINE_BIND_FUNCTIONNAME(WaitForMouseEvent);

	DEFINE_BIND_FUNCTIONNAME(SetFullscreen);
	DEFINE_BIND_FUNCTIONNAME(EnableScrollbars);
	
	DEFINE_BIND_FUNCTIONNAME(WaitForDragDrop);
	
	const auto OnTryDragDrop_FunctionName = "OnTryDragDrop";
}


void ApiGui::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<TWindowWrapper>( Namespace, "Window" );
	Context.BindObjectType<TSliderWrapper>( Namespace );
	Context.BindObjectType<TLabelWrapper>( Namespace );
	Context.BindObjectType<TTextBoxWrapper>( Namespace );
	Context.BindObjectType<TTickBoxWrapper>( Namespace );
	Context.BindObjectType<TColourPickerWrapper>(Namespace);
	Context.BindObjectType<TColourButtonWrapper>(Namespace);
	Context.BindObjectType<TImageMapWrapper>(Namespace);
}


void ApiGui::TSliderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::SetMinMax>( &TSliderWrapper::SetMinMax );
	Template.BindFunction<BindFunction::SetValue>( &TSliderWrapper::SetValue );
}

void ApiGui::TSliderWrapper::Construct(Bind::TCallback& Params)
{
	auto& ParentWindow = Params.GetArgumentPointer<TWindowWrapper>(0);

	BufferArray<int32_t,4> Rect4;
	Params.GetArgumentArray( 1, GetArrayBridge(Rect4) );
	Soy::Rectx<int32_t> Rect( Rect4[0], Rect4[1], Rect4[2], Rect4[3] );
	
	int NotchCount = 0;
	if (Params.IsArgumentBool(2))
	{
		if (Params.GetArgumentBool(2))
			throw Soy::AssertException("Slider notches parameter can either be false, or a number.");
		NotchCount = 0;
	}
	else if (Params.IsArgumentNumber(2))
	{
		NotchCount = Params.GetArgumentInt(2);
	}

	mSlider = Platform::CreateSlider( *ParentWindow.mWindow, Rect );
	mSlider->mOnValueChanged = std::bind( &TSliderWrapper::OnChanged, this, std::placeholders::_1, std::placeholders::_2);
}

void ApiGui::TSliderWrapper::SetMinMax(Bind::TCallback& Params)
{
	auto Min = Params.GetArgumentInt(0);
	auto Max = Params.GetArgumentInt(1);

	auto NotchCount = 0;
	if (!Params.IsArgumentUndefined(2))
		NotchCount = Params.GetArgumentInt(2);

	mSlider->SetMinMax( Min, Max, NotchCount);
}

void ApiGui::TSliderWrapper::SetValue(Bind::TCallback& Params)
{
	auto Value = Params.GetArgumentInt(0);
	mSlider->SetValue( Value );
}


void ApiGui::TSliderWrapper::OnChanged(uint16_t& NewValue,bool FinalValue)
{
	auto Callback = [this,NewValue, FinalValue](Bind::TLocalContext& Context)
	{
		auto This = this->GetHandle(Context);
		auto ThisOnChanged = This.GetFunction("OnChanged");
		JsCore::TCallback Callback(Context);
		Callback.SetArgumentInt(0, NewValue);
		Callback.SetArgumentBool(1, FinalValue);
		ThisOnChanged.Call( Callback );
	};
	this->mContext.Queue( Callback );
}





void ApiGui::TWindowWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::SetFullscreen>( &TWindowWrapper::SetFullscreen );
	Template.BindFunction<BindFunction::EnableScrollbars>( &TWindowWrapper::EnableScrollbars );

	Template.BindFunction<BindFunction::WaitForDragDrop>( &TGuiControlWrapper::WaitForDragDrop );
}

void ApiGui::TWindowWrapper::Construct(Bind::TCallback& Params)
{
	std::string WindowName = GetTypeName();
	if ( !Params.IsArgumentUndefined(0) )
		WindowName = Params.GetArgumentString(0);
	
	Soy::Rectx<int32_t> Rect(0, 0, 0, 0);
	
	//	if no rect, get rect from screen
	if ( !Params.IsArgumentUndefined(1) )
	{
		BufferArray<int32_t,4> Rect4;
		Params.GetArgumentArray(1, GetArrayBridge(Rect4) );
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
	
	bool Resizable = true;
	if ( !Params.IsArgumentUndefined(2))
	{
		Resizable = Params.GetArgumentBool(2);
	}
	
	mWindow = Platform::CreateWindow( WindowName, Rect, Resizable );
	mWindow->EnableScrollBars(false, false);
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
	mWindow->mOnTryDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	return this->OnTryDragDrop(Filenames);	};
	mWindow->mOnDragDrop = [this](ArrayBridge<std::string>& Filenames)		{	this->OnDragDrop(Filenames);	};

}

void ApiGui::TWindowWrapper::SetFullscreen(Bind::TCallback& Params)
{
	auto Fullscreen = Params.GetArgumentBool(0);
	mWindow->SetFullscreen(Fullscreen);
}

void ApiGui::TWindowWrapper::EnableScrollbars(Bind::TCallback& Params)
{
	//	order is xy
	auto Horz = Params.GetArgumentBool(0);
	auto Vert = Params.GetArgumentBool(1);
	mWindow->EnableScrollBars( Horz, Vert );
}



void ApiGui::TLabelWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::SetValue>( &TLabelWrapper::SetValue );
}

void ApiGui::TLabelWrapper::Construct(Bind::TCallback& Params)
{
	auto& ParentWindow = Params.GetArgumentPointer<TWindowWrapper>(0);
	
	if ( Params.IsArgumentString(1) )
	{
		auto Name = Params.GetArgumentString(1);
		mLabel = Platform::GetLabel( *ParentWindow.mWindow, Name );
	}
	else
	{
		BufferArray<int32_t,4> Rect4;
		Params.GetArgumentArray( 1, GetArrayBridge(Rect4) );
		Soy::Rectx<int32_t> Rect( Rect4[0], Rect4[1], Rect4[2], Rect4[3] );
	
		mLabel = Platform::CreateLabel( *ParentWindow.mWindow, Rect );
	}
}

void ApiGui::TLabelWrapper::SetValue(Bind::TCallback& Params)
{
	auto Value = Params.GetArgumentString(0);
	mLabel->SetValue( Value );
}




void ApiGui::TTextBoxWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::SetValue>( &TTextBoxWrapper::SetValue );
}

void ApiGui::TTextBoxWrapper::Construct(Bind::TCallback& Params)
{
	auto& ParentWindow = Params.GetArgumentPointer<TWindowWrapper>(0);
	
	BufferArray<int32_t,4> Rect4;
	Params.GetArgumentArray( 1, GetArrayBridge(Rect4) );
	Soy::Rectx<int32_t> Rect( Rect4[0], Rect4[1], Rect4[2], Rect4[3] );
	
	mTextBox = Platform::CreateTextBox( *ParentWindow.mWindow, Rect );
	mTextBox->mOnValueChanged = std::bind( &TTextBoxWrapper::OnChanged, this, std::placeholders::_1 );
}

void ApiGui::TTextBoxWrapper::SetValue(Bind::TCallback& Params)
{
	auto Value = Params.GetArgumentString(0);
	mTextBox->SetValue( Value );
}

void ApiGui::TTextBoxWrapper::OnChanged(const std::string& NewValue)
{
	auto Callback = [this,NewValue](Bind::TLocalContext& Context)
	{
		auto This = this->GetHandle(Context);
		auto ThisOnChanged = This.GetFunction("OnChanged");
		JsCore::TCallback Callback(Context);
		Callback.SetArgumentString(0, NewValue);
		ThisOnChanged.Call( Callback );
	};
	this->mContext.Queue( Callback );
}



void ApiGui::TTickBoxWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::SetValue>( &TTickBoxWrapper::SetValue );
	Template.BindFunction<BindFunction::SetLabel>( &TTickBoxWrapper::SetLabel );
}

void ApiGui::TTickBoxWrapper::Construct(Bind::TCallback& Params)
{
	auto& ParentWindow = Params.GetArgumentPointer<TWindowWrapper>(0);
	
	BufferArray<int32_t,4> Rect4;
	Params.GetArgumentArray( 1, GetArrayBridge(Rect4) );
	Soy::Rectx<int32_t> Rect( Rect4[0], Rect4[1], Rect4[2], Rect4[3] );
	
	mControl = Platform::CreateTickBox( *ParentWindow.mWindow, Rect );
	mControl->mOnValueChanged = std::bind( &TTickBoxWrapper::OnChanged, this, std::placeholders::_1 );
}


void ApiGui::TTickBoxWrapper::SetValue(Bind::TCallback& Params)
{
	auto Value = Params.GetArgumentBool(0);
	mControl->SetValue( Value );
}

void ApiGui::TTickBoxWrapper::SetLabel(Bind::TCallback& Params)
{
	auto Value = Params.GetArgumentString(0);
	mControl->SetLabel( Value );
}

void ApiGui::TTickBoxWrapper::OnChanged(bool& NewValue)
{
	auto Callback = [this,NewValue](Bind::TLocalContext& Context)
	{
		auto This = this->GetHandle(Context);
		auto ThisOnChanged = This.GetFunction("OnChanged");
		JsCore::TCallback Callback(Context);
		Callback.SetArgumentBool(0, NewValue);
		ThisOnChanged.Call( Callback );
	};
	this->mContext.Queue( Callback );
}



ApiGui::TColourPickerWrapper::~TColourPickerWrapper()
{
	if (mControl)
	{
		mControl->mOnValueChanged = nullptr;
		mControl->mOnDialogClosed = nullptr;
	}
}

void ApiGui::TColourPickerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
}

void ApiGui::TColourPickerWrapper::Construct(Bind::TCallback& Params)
{
	BufferArray<uint8_t,3> Rgb;
	Params.GetArgumentArray( 0, GetArrayBridge(Rgb) );
	
	vec3x<uint8_t> Rgb3( Rgb[0], Rgb[1], Rgb[2] );
	
	mControl = Platform::CreateColourPicker( Rgb3 );
	mControl->mOnValueChanged = std::bind( &TColourPickerWrapper::OnChanged, this, std::placeholders::_1 );
	mControl->mOnDialogClosed = std::bind( &TColourPickerWrapper::OnClosed, this );
}


void ApiGui::TColourPickerWrapper::OnChanged(vec3x<uint8_t>& NewValue)
{
	auto Callback = [this,NewValue](Bind::TLocalContext& Context)
	{
		auto RgbArray = NewValue.GetArray();
		auto This = this->GetHandle(Context);
		auto ThisOnChanged = This.GetFunction("OnChanged");
		JsCore::TCallback Callback(Context);
		Callback.SetArgumentArray( 0, GetArrayBridge(RgbArray) );
		ThisOnChanged.Call( Callback );
	};
	this->mContext.Queue( Callback );
}

void ApiGui::TColourPickerWrapper::OnClosed()
{
	auto Callback = [this](Bind::TLocalContext& Context)
	{
		auto This = this->GetHandle(Context);
		auto ThisOnChanged = This.GetFunction("OnClosed");
		JsCore::TCallback Callback(Context);
		ThisOnChanged.Call( Callback );
	};
	this->mContext.Queue( Callback );
}



void ApiGui::TColourButtonWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::SetValue>(&TColourButtonWrapper::SetValue);
}

void ApiGui::TColourButtonWrapper::Construct(Bind::TCallback& Params)
{
	auto& ParentWindow = Params.GetArgumentPointer<TWindowWrapper>(0);

	BufferArray<int32_t, 4> Rect4;
	Params.GetArgumentArray(1, GetArrayBridge(Rect4));
	Soy::Rectx<int32_t> Rect(Rect4[0], Rect4[1], Rect4[2], Rect4[3]);
	

	mColourButton = Platform::CreateColourButton(*ParentWindow.mWindow, Rect);
	mColourButton->mOnValueChanged = std::bind(&TColourButtonWrapper::OnChanged, this, std::placeholders::_1, std::placeholders::_2);
}


void ApiGui::TColourButtonWrapper::SetValue(Bind::TCallback& Params)
{
	//	HTML colour control is float, so we're accepting them
	BufferArray<float, 3> RgbArray;
	Params.GetArgumentArray(0,GetArrayBridge(RgbArray));

	if (RgbArray.GetSize() != 3)
	{
		auto ValueStr = Params.GetArgumentString(0);
		std::stringstream Error;
		Error << __PRETTY_FUNCTION__ << " expected rgb as array of 3, given (as string) " << ValueStr;
		throw Soy::AssertException(Error);
	}

	//	todo: throw if outside 0-1

	vec3x<uint8_t> Rgb;
	Rgb.x = RgbArray[0] * 255.f;
	Rgb.y = RgbArray[1] * 255.f;
	Rgb.z = RgbArray[2] * 255.f;

	mColourButton->SetValue(Rgb);
}


void ApiGui::TColourButtonWrapper::OnChanged(vec3x<uint8_t>& NewValue, bool FinalValue)
{
	BufferArray<uint8_t, 3> Rgb;
	Rgb.Copy(NewValue.GetArray());

	auto Callback = [this, Rgb, FinalValue](Bind::TLocalContext& Context)
	{
		//	API has float colours
		BufferArray<float, 3> Rgbf;
		Rgbf.PushBack(Rgb[0] / 255.f);
		Rgbf.PushBack(Rgb[1] / 255.f);
		Rgbf.PushBack(Rgb[2] / 255.f);
		auto This = this->GetHandle(Context);
		auto ThisOnChanged = This.GetFunction("OnChanged");
		JsCore::TCallback Callback(Context);
		Callback.SetArgumentArray(0, GetArrayBridge(Rgbf));
		Callback.SetArgumentBool(1, FinalValue);
		ThisOnChanged.Call(Callback);
	};
	this->mContext.Queue(Callback);
}





	
void ApiGui::TImageMapWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::SetImage>(&TImageMapWrapper::SetImage);
	Template.BindFunction<BindFunction::SetCursorMap>(&TImageMapWrapper::SetCursorMap);
	Template.BindFunction<BindFunction::WaitForMouseEvent>(&TImageMapWrapper::WaitForMouseEvent);
}

void ApiGui::TImageMapWrapper::Construct(Bind::TCallback& Params)
{
	auto& ParentWindow = Params.GetArgumentPointer<TWindowWrapper>(0);

	BufferArray<int32_t, 4> Rect4;
	Params.GetArgumentArray(1, GetArrayBridge(Rect4));
	Soy::Rectx<int32_t> Rect(Rect4[0], Rect4[1], Rect4[2], Rect4[3]);

	mControl = Platform::CreateImageMap(*ParentWindow.mWindow, Rect);
	mControl->mOnMouseEvent = std::bind(&TImageMapWrapper::OnMouseEvent, this, std::placeholders::_1);
}


void ApiGui::TImageMapWrapper::SetImage(Bind::TCallback& Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	auto& Pixels = Image.GetPixels();
	mControl->SetImage(Pixels);
}

void ApiGui::TImageMapWrapper::SetCursorMap(Bind::TCallback& Params)
{
	auto& CursorMapImage = Params.GetArgumentPointer<TImageWrapper>(0);
	auto& CursorMapPixels = CursorMapImage.GetPixels();

	Array<std::string> CursorNames;
	Params.GetArgumentArray(1, GetArrayBridge(CursorNames));

	mControl->SetCursorMap(CursorMapPixels, GetArrayBridge(CursorNames));
}

void ApiGui::TImageMapWrapper::WaitForMouseEvent(Bind::TCallback& Params)
{
	auto Promise = mMouseEventRequests.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	FlushMouseEvents();
}

void ApiGui::TImageMapWrapper::OnMouseEvent(Gui::TMouseEvent& MouseEvent)
{
	{
		std::lock_guard<std::mutex> Lock(mMouseEventsLock);
		mMouseEvents.PushBack(MouseEvent);
	}
	FlushMouseEvents();
}

void ApiGui::TImageMapWrapper::FlushMouseEvents()
{
	if (mMouseEvents.IsEmpty())
		return;
	if (!mMouseEventRequests.HasPromises())
		return;

	auto Resolve = [this](Bind::TLocalContext& Context)
	{
		//	pop event
	};
	auto& Context = mMouseEventRequests.GetContext();
	Context.Queue(Resolve);
}


ApiGui::TGuiControlWrapper::TGuiControlWrapper()
{
	this->mOnDragDropPromises.mResolveObject = [this](Bind::TLocalContext& Context,Bind::TPromise& Promise,Array<std::string>& Filenames)
	{
		Promise.Resolve( Context, GetArrayBridge(Filenames) );
	};
}

void ApiGui::TGuiControlWrapper::WaitForDragDrop(Bind::TCallback& Arguments)
{
	//	would be good to accept a filter func here!
	auto Promise = mOnDragDropPromises.AddPromise( Arguments.mLocalContext );
	Arguments.Return(Promise);
}
	
bool ApiGui::TGuiControlWrapper::OnTryDragDrop(const ArrayBridge<std::string>& Filenames)
{
	bool Result = false;
	//  call javascript immediately, maybe we can skip this if we can pre-check for a member?
	auto Runner = [&](Bind::TLocalContext& Context)
	{
		//try
		{
			auto& ThisObjectWrapper = this->GetObjectWrapper();
			auto This = ThisObjectWrapper.GetHandle(Context);
			if ( !This.HasMember( OnTryDragDrop_FunctionName ) )
			{
				std::Debug << "Window has not overloaded .OnTryDragDrop; allowing drag&drop" << std::endl;
				Result = true;
				return;
			}
			auto ThisFunc = This.GetFunction(OnTryDragDrop_FunctionName);
			Bind::TCallback Params(Context);
			Params.SetThis( This );
			Params.SetArgumentArray( 0, GetArrayBridge(Filenames) );
			ThisFunc.Call( Params );
			Result = Params.GetReturnBool();
		}
		//catch(std::exception& e)
		{
			//std::Debug << "Exception in OnTryDragDrop: " << e.what() << std::endl;
		}
	};
	
	try
	{
		auto& This = this->GetObjectWrapper();
		auto& Context = This.GetContext();
		Context.Execute( Runner );
		return Result;
	}
	catch(std::exception& e)
	{
		std::Debug << "Exception in OnTryDragDrop: " << e.what() << std::endl;
		return false;
	}
}

void ApiGui::TGuiControlWrapper::OnDragDrop(const ArrayBridge<std::string>& Filenames)
{
	Array<std::string> FilenamesCopy( Filenames );
	mOnDragDropPromises.Push(FilenamesCopy);
}



