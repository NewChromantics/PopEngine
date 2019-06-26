#include "TApiOpengl.h"
//#include "TApiOpenglContext.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"


namespace ApiOpengl
{
	const char Namespace[] = "Pop.Opengl";

	
	DEFINE_BIND_TYPENAME(Window);
	DEFINE_BIND_TYPENAME(Shader);
	DEFINE_BIND_TYPENAME(TriangleBuffer);
}


DEFINE_BIND_TYPENAME(Window);
DEFINE_BIND_TYPENAME(Shader);

DEFINE_BIND_FUNCTIONNAME(DrawQuad);
DEFINE_BIND_FUNCTIONNAME(DrawGeometry);
DEFINE_BIND_FUNCTIONNAME(ClearColour);
DEFINE_BIND_FUNCTIONNAME(EnableBlend);
DEFINE_BIND_FUNCTIONNAME(SetViewport);
DEFINE_BIND_FUNCTIONNAME(SetUniform);
DEFINE_BIND_FUNCTIONNAME(Render);
DEFINE_BIND_FUNCTIONNAME(RenderChain);
DEFINE_BIND_FUNCTIONNAME(RenderToRenderTarget);
DEFINE_BIND_FUNCTIONNAME(GetScreenRect);
DEFINE_BIND_FUNCTIONNAME(SetFullscreen);
DEFINE_BIND_FUNCTIONNAME(IsFullscreen);



void ApiOpengl::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<TWindowWrapper>( Namespace );
	Context.BindObjectType<TShaderWrapper>( Namespace );
	Context.BindObjectType<TTriangleBufferWrapper>( Namespace );
}


TWindowWrapper::~TWindowWrapper()
{
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}
}


Bind::TContext& TWindowWrapper::GetOpenglJsCoreContext()
{
	return this->GetContext();
	
	if ( mOpenglJsCoreContext )
		return *mOpenglJsCoreContext;
	
	Bind::TContext& MainContext = this->GetContext();
	//	auto GlobalOther = JSContextGetGlobalObject( ParamsJs.mContext );
	mOpenglJsCoreContext = MainContext.mInstance.CreateContext("Opengl JsCore Context");
	return *mOpenglJsCoreContext;
}


void TWindowWrapper::RenderToRenderTarget(Bind::TCallback& Params)
{
	auto& This = Params.This<TWindowWrapper>();
	auto& OpenglContext = *This.GetOpenglContext();
	if ( !OpenglContext.IsLockedToThisThread() )
		throw Soy::AssertException("RenderToRenderTarget not being called on opengl thread");

	std::string ReadBack;
	if ( Params.IsArgumentString(2) )
		ReadBack = Params.GetArgumentString(2);
	auto ReadBackPixelsAfterwards = SoyPixelsFormat::ToType( ReadBack );

	
	
	//	get current rendertarget
	if ( !This.mActiveRenderTarget )
		throw Soy::AssertException("Expecting a current render target");
	auto* CurrentRenderTarget = This.mActiveRenderTarget;
	auto UnbindCurrent = [&]()
	{
		CurrentRenderTarget->Unbind();
	};
	auto RebindCurrent = [&]()
	{
		CurrentRenderTarget->Bind();
		This.mActiveRenderTarget = CurrentRenderTarget;
		
		//	mark that texture has changed
		auto& TargetImage = Params.GetArgumentPointer<TImageWrapper>(0);
		TargetImage.OnOpenglTextureChanged(OpenglContext);

		//	read back pixels if requested
		if ( ReadBackPixelsAfterwards != SoyPixelsFormat::Invalid )
			TargetImage.ReadOpenglPixels(ReadBackPixelsAfterwards);

		//	restore state after functions above, which might still mess around with things like viewport
		CurrentRenderTarget->SetViewportNormalised( Soy::Rectf(0,0,1,1) );
	};
	Soy::TScopeCall RestoreRenderTarget( UnbindCurrent, RebindCurrent );
	
	
	//	render
	auto ExecuteRenderCallback = [&](Bind::TLocalContext& Context)
	{
		//	setup variables
		auto& TargetImage = Params.GetArgumentPointer<TImageWrapper>(0);
		auto RenderCallbackFunc = Params.GetArgumentFunction(1);
		
		//	make sure texture is generated for target
		{
			std::string TextureException;
			auto OnError = [&](const std::string& Error)
			{
				TextureException = Error;
			};
			auto OnLoaded = []{};
			TargetImage.GetTexture( OpenglContext, OnLoaded, OnError );
			if ( TextureException.size() != 0 )
				throw Soy::AssertException(TextureException);
		}
		
		auto& TargetTexture = TargetImage.GetTexture();
		
		Opengl::TRenderTargetFbo RenderTarget( "Window::Render", TargetTexture );
		RenderTarget.mGenerateMipMaps = false;
		RenderTarget.Bind();
		RenderTarget.SetViewportNormalised( Soy::Rectf(0,0,1,1) );

		//	hack! need to turn render target into it's own javasript object
		//	that's why the image render target is the render context
		This.mActiveRenderTarget = &RenderTarget;
		auto RenderTargetObject = Params.ThisObject();
		
		Bind::TCallback CallbackParams(Context);
		//CallbackParams.SetThis( Params.mThis );
		CallbackParams.SetArgumentObject( 0, RenderTargetObject );
		RenderCallbackFunc.Call( CallbackParams );
	};
	ExecuteRenderCallback( Params.mLocalContext );
}


void TWindowWrapper::OnRender(Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext)
{
	//  call javascript
	auto Runner = [&](Bind::TLocalContext& Context)
	{
		LockContext();
		
		if ( !mWindow )
			std::Debug << "Should therealways  be a window here in OnRender?" << std::endl;
		else
			mWindow->Clear( RenderTarget );

		//	our ol' hack
		mActiveRenderTarget = &RenderTarget;
		auto RenderTargetObject = this->GetHandle(Context);
		
		//	gr: allow this to fail silently if the user has assigned nothing
		//	gr: kinda want a specific "is undefined" exception so we don't miss important things
		static bool SwallowException = false;
		try
		{
			auto This = this->GetHandle(Context);
			auto ThisOnRender = This.GetFunction("OnRender");
			JsCore::TCallback Callback(Context);
			Callback.SetArgumentObject(0, RenderTargetObject);
			ThisOnRender.Call( Callback );
		}
		catch(std::exception& e)
		{
			if ( SwallowException )
				return;
			throw;
		}
	};
	
	auto& Context = GetOpenglJsCoreContext();
	Context.Execute(Runner);
}

void TWindowWrapper::OnMouseFunc(const TMousePos& MousePos,SoyMouseButton::Type MouseButton,const std::string& MouseFuncName)
{
	//  call javascript
	auto Runner = [=](Bind::TLocalContext& Context)
	{
		try
		{
			auto This = this->GetHandle(Context);
			auto ThisOnRender = This.GetFunction(MouseFuncName);
			Bind::TCallback Params(Context);
			Params.SetThis( This );
			Params.SetArgumentInt( 0, MousePos.x );
			Params.SetArgumentInt( 1, MousePos.y );
			Params.SetArgumentInt( 2, MouseButton );
			ThisOnRender.Call( Params );
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in " << MouseFuncName << ": " << e.what() << std::endl;
		}
	};
	GetContext().Queue( Runner );
}


void TWindowWrapper::OnKeyFunc(SoyKeyButton::Type Button,const std::string& FuncName)
{
	//  call javascript
	auto Runner = [=](Bind::TLocalContext& Context)
	{
		try
		{
			std::string KeyString( &Button, 1 );
			
			auto This = this->GetHandle(Context);
			auto ThisOnRender = This.GetFunction(FuncName);
			Bind::TCallback Params(Context);
			Params.SetThis( This );
			Params.SetArgumentString( 0, KeyString );
			ThisOnRender.Call( Params );
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in " << FuncName << ": " << e.what() << std::endl;
		}
	};
	GetContext().Queue( Runner );
}


bool TWindowWrapper::OnTryDragDrop(ArrayBridge<std::string>& Filenames)
{
	bool Result = false;
	//  call javascript
	auto Runner = [&](Bind::TLocalContext& Context)
	{
		try
		{
			auto This = this->GetHandle(Context);
			auto ThisFunc = This.GetFunction("OnTryDragDrop");
			Bind::TCallback Params(Context);
			Params.SetThis( This );
			Params.SetArgumentArray( 0, GetArrayBridge(Filenames) );
			ThisFunc.Call( Params );
			Result = Params.GetReturnBool();
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in OnTryDragDrop: " << e.what() << std::endl;
		}
	};
	
	try
	{
		GetContext().Execute( Runner );
		return Result;
	}
	catch(std::exception& e)
	{
		std::Debug << "Exception in OnTryDragDrop: " << e.what() << std::endl;
		return false;
	}
}


void TWindowWrapper::OnClosed()
{
	auto Runner = [=](Bind::TLocalContext& Context)
	{
		try
		{
			auto This = this->GetHandle(Context);
			auto ThisFunc = This.GetFunction("OnClosed");
			Bind::TCallback Params(Context);
			Params.SetThis( This );
			ThisFunc.Call( Params );
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in OnClosed: " << e.what() << std::endl;
		}
	};

	GetContext().Queue( Runner );
}

void TWindowWrapper::OnDragDrop(ArrayBridge<std::string>& FilenamesOrig)
{
	//	copy for queue
	Array<std::string> Filenames( FilenamesOrig );
	
	//  call javascript
	auto Runner = [=](Bind::TLocalContext& Context)
	{
		try
		{
			auto This = this->GetHandle(Context);
			auto ThisFunc = This.GetFunction("OnDragDrop");
			Bind::TCallback Params(Context);
			Params.SetThis( This );
			Params.SetArgumentArray( 0, GetArrayBridge(Filenames) );
			ThisFunc.Call( Params );
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in OnDragDrop: " << e.what() << std::endl;
		}
	};
	
	GetContext().Queue( Runner );
}




void TWindowWrapper::Construct(Bind::TCallback& Params)
{
	auto WindowName = Params.GetArgumentString(0);
	
	TOpenglParams WindowParams;
	WindowParams.mDoubleBuffer = false;
	WindowParams.mAutoRedraw = true;

	if ( Params.IsArgumentBool(1) )
	{
		WindowParams.mAutoRedraw = Params.GetArgumentBool(1);
	}
	
	//	named options
	if ( Params.IsArgumentObject(1) )
	{
		auto WindowParamsObject = Params.GetArgumentObject(1);
		if ( WindowParamsObject.HasMember("Fullscreen") )
			WindowParams.mFullscreen = WindowParamsObject.GetBool("Fullscreen");
	}
	
	//	get first monitor size
	Soy::Rectf Rect(0, 0, 0, 0);
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

	mWindow.reset( new TRenderWindow( WindowName, Rect, WindowParams ) );
	
	auto OnRender = [this](Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext)
	{
		this->OnRender( RenderTarget, LockContext );
	};

	mWindow->mOnRender = OnRender;
	mWindow->mOnMouseDown = [this](const TMousePos& Pos,SoyMouseButton::Type Button)	{	this->OnMouseFunc(Pos,Button,"OnMouseDown");	};
	mWindow->mOnMouseUp = [this](const TMousePos& Pos,SoyMouseButton::Type Button)		{	this->OnMouseFunc(Pos,Button,"OnMouseUp");	};
	mWindow->mOnMouseMove = [this](const TMousePos& Pos,SoyMouseButton::Type Button)	{	this->OnMouseFunc(Pos,Button,"OnMouseMove");	};
	mWindow->mOnKeyDown = [this](SoyKeyButton::Type Button)			{	this->OnKeyFunc(Button,"OnKeyDown");	};
	mWindow->mOnKeyUp = [this](SoyKeyButton::Type Button)			{	this->OnKeyFunc(Button,"OnKeyUp");	};
	mWindow->mOnTryDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	return this->OnTryDragDrop(Filenames);	};
	mWindow->mOnDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	this->OnDragDrop(Filenames);	};
	mWindow->mOnClosed = [this]()	{	this->OnClosed();	};
}

void TWindowWrapper::DrawQuad(Bind::TCallback& Params)
{
	auto& OpenglContext = *mWindow->GetContext();
	if ( !OpenglContext.IsLockedToThisThread() )
		throw Soy::AssertException("Function not being called on opengl thread");

	auto Arg_Shader = 0;
	auto Arg_OnShaderFunc = 1;
	
	auto& Shader = Params.GetArgumentPointer<TShaderWrapper>(Arg_Shader);
	auto ShaderObject = Params.GetArgumentObject(Arg_Shader);

	std::function<void()> OnShaderBind = []{};
	if ( !Params.IsArgumentUndefined(Arg_OnShaderFunc) )
	{
		OnShaderBind = [&]
		{
			auto CallbackFunc = Params.GetArgumentFunction(Arg_OnShaderFunc);
			auto This = Params.ThisObject();
			Bind::TCallback CallbackParams( Params.mLocalContext );
			CallbackParams.SetThis( This );
			CallbackParams.SetArgumentObject(0,ShaderObject);
			CallbackFunc.Call( CallbackParams );
		};
	}
	
	auto& Geometry = mWindow->GetBlitQuad();
	mWindow->DrawGeometry( Geometry, *Shader.mShader, OnShaderBind );
}


void TWindowWrapper::DrawGeometry(Bind::TCallback& Params)
{
	auto& OpenglContext = *mWindow->GetContext();
	if ( !OpenglContext.IsLockedToThisThread() )
		throw Soy::AssertException("Function not being called on opengl thread");
	
	auto Arg_Geometry = 0;
	auto Arg_Shader = 1;
	auto Arg_OnShaderFunc = 2;
	
	auto& Geometry = Params.GetArgumentPointer<ApiOpengl::TTriangleBufferWrapper>(Arg_Geometry);
	auto& Shader = Params.GetArgumentPointer<TShaderWrapper>(Arg_Shader);
	auto ShaderObject = Params.GetArgumentObject(Arg_Shader);
	
	std::function<void()> OnShaderBind = []{};
	if ( !Params.IsArgumentUndefined(Arg_OnShaderFunc) )
	{
		OnShaderBind = [&]
		{
			auto CallbackFunc = Params.GetArgumentFunction(Arg_OnShaderFunc);
			auto This = Params.ThisObject();
			Bind::TCallback CallbackParams( Params.mLocalContext );
			CallbackParams.SetThis( This );
			CallbackParams.SetArgumentObject(0,ShaderObject);
			CallbackFunc.Call( CallbackParams );
		};
	}
	
	mWindow->DrawGeometry( *Geometry.mGeometry, *Shader.mShader, OnShaderBind );
}

void TWindowWrapper::ClearColour(Bind::TCallback& Params)
{
	auto& This = Params.This<TWindowWrapper>();
	
	if ( Params.GetArgumentCount() != 3 )
		throw Soy::AssertException("Expecting 3 arguments for ClearColour(r,g,b)");

	auto Red = Params.GetArgumentFloat(0);
	auto Green = Params.GetArgumentFloat(1);
	auto Blue = Params.GetArgumentFloat(2);
	Soy::TRgb Colour( Red, Green, Blue );
	
	This.mWindow->ClearColour( Colour );
}


void TWindowWrapper::EnableBlend(Bind::TCallback& Params)
{
	auto& This = Params.This<TWindowWrapper>();

	auto Enable = Params.GetArgumentBool(0);
	This.mWindow->EnableBlend( Enable );
}


void TWindowWrapper::SetViewport(Bind::TCallback& Params)
{
	auto& This = Params.This<TWindowWrapper>();

	BufferArray<float,4> Viewportxywh;
	Params.GetArgumentArray( 0, GetArrayBridge(Viewportxywh) );
	//v8::EnumArray( Arguments[0], GetArrayBridge(Viewportxywh), "SetViewport" );
	Soy::Rectf ViewportRect( Viewportxywh[0], Viewportxywh[1], Viewportxywh[2], Viewportxywh[3] );
	
	if ( !This.mActiveRenderTarget )
		throw Soy::AssertException("No active render target");
	
	This.mActiveRenderTarget->SetViewportNormalised( ViewportRect );
}

//	window specific
void TWindowWrapper::GetScreenRect(Bind::TCallback& Params)
{
	auto& This = Params.This<TWindowWrapper>();

	auto ScreenRect = This.mWindow->GetScreenRect();
	
	BufferArray<int32_t,4> ScreenRect4;
	ScreenRect4.PushBack(ScreenRect.x);
	ScreenRect4.PushBack(ScreenRect.y);
	ScreenRect4.PushBack(ScreenRect.w);
	ScreenRect4.PushBack(ScreenRect.h);
	
	Params.Return( GetArrayBridge(ScreenRect4) );
}


void TWindowWrapper::SetFullscreen(Bind::TCallback& Params)
{
	auto& This = Params.This<TWindowWrapper>();
	auto Fullscreen = true;
	if ( !Params.IsArgumentUndefined(0) )
		Fullscreen = Params.GetArgumentBool(0);
	
	This.mWindow->SetFullscreen(Fullscreen);
}

void TWindowWrapper::IsFullscreen(Bind::TCallback& Params)
{
	auto& This = Params.This<TWindowWrapper>();
	auto Fullscreen = This.mWindow->IsFullscreen();
	Params.Return( Fullscreen );
}


void TWindowWrapper::Render(Bind::TCallback& Params)
{
	throw Soy::AssertException("TWindowWrapper::Render is causing JS corruption");
	
	Soy::TScopeTimerPrint Timer("Render()", 5);
	
	auto& This = Params.This<TWindowWrapper>();
	auto OpenglContext = This.mWindow->GetContext();
	if ( !OpenglContext )
		throw Soy::AssertException("Opengl context not created yet");

	auto* pThis = &This;
	auto WindowHandle = Params.ThisObject();
	auto WindowPersistent = Bind::TPersistent( Params.mLocalContext, WindowHandle, "WindowHandle" );
	
	auto Promise = Params.mContext.CreatePromise( Params.mLocalContext, __FUNCTION__);
	//	return the promise
	Params.Return( Promise );
	
	auto* pContext = &Params.mContext;
	auto* pOpenglBindContext = &This.GetOpenglJsCoreContext();
	//auto* pOpenglBindContext = pContext;
	
	auto Resolve = [=](Bind::TLocalContext& Context)
	{
		//	testing to see if the target is at fault
		//auto Target = TargetPersistent->GetObject();
		//Promise.Resolve( Target );
		Promise.ResolveUndefined(Context);
	};
	
	auto OnCompleted = [=]()
	{
		pContext->Queue( Resolve );
	};
	//OnCompleted();	//	testing
	
	
	auto TargetHandle = Params.GetArgumentObject(0);
	auto CallbackHandle = Params.GetArgumentFunction(1);
	std::string ReadBack;
	if ( Params.IsArgumentString(2) )
		ReadBack = Params.GetArgumentString(2);
	
	//	can't fetch object from persistent in non-js func, but we just want the image
	auto* pTargetImage = &TargetHandle.This<TImageWrapper>();
	auto TargetPersistent = Bind::TPersistent( Params.mLocalContext, TargetHandle, "TargetPersistent" );
	auto RenderCallbackPersistent = Bind::TPersistent( Params.mLocalContext, CallbackHandle, "CallbackHandle" );
	auto ReadBackPixelsAfterwards = SoyPixelsFormat::ToType( ReadBack );
	
	
	auto ExecuteRenderCallback = [=](Bind::TLocalContext& Context)
	{
		auto Func = RenderCallbackPersistent.GetFunction(Context);
		auto Window = WindowPersistent.GetObject(Context);
		auto Target = TargetPersistent.GetObject(Context);
		
		Bind::TCallback CallbackParams(Context);
		CallbackParams.SetArgumentObject( 0, Window );
		CallbackParams.SetArgumentObject( 1, Target );
		//	todo: return this result to the promise
		Func.Call( CallbackParams );
	};
	
	
	
	auto OpenglRender = [=]
	{
		if ( !OpenglContext->IsLockedToThisThread() )
			throw Soy::AssertException("Function not being called on opengl thread");
		try
		{
			//	gr: we were storing this pointer which may be getting deleted
			//	with V8 we can't access this pointer out of thread, but maybe we can cache both
			auto& TargetImage = *pTargetImage;
			
			//	get the texture from the image
			std::string GenerateTextureError;
			auto OnError = [&](const std::string& Error)
			{
				throw Soy::AssertException(Error);
			};
			//	gr: this auto execute automatically
			TargetImage.GetTexture( *OpenglContext, []{}, OnError );
		
			//	setup render target
			auto TargetTexturePtr = TargetImage.GetTexturePtr();
			auto& TargetTexture = *TargetTexturePtr;
			
			Opengl::TRenderTargetFbo RenderTarget( "Window::Render", TargetTexture );
			RenderTarget.mGenerateMipMaps = false;
			RenderTarget.Bind();
			
			//	hack! need to turn render target into it's own javasript object
			pThis->mActiveRenderTarget = &RenderTarget;
			RenderTarget.SetViewportNormalised( Soy::Rectf(0,0,1,1) );
			try
			{
				Soy::TScopeTimerPrint Timer("Opengl.Render callback",30);
				//	immediately call the javascript callback
				auto& Context = *pOpenglBindContext;
				Context.Execute( ExecuteRenderCallback );
				
				pThis->mActiveRenderTarget = nullptr;
				RenderTarget.Unbind();
			}
			catch(std::exception& e)
			{
				pThis->mActiveRenderTarget = nullptr;
				RenderTarget.Unbind();
				throw;
			}

			TargetImage.OnOpenglTextureChanged(*OpenglContext);
			if ( ReadBackPixelsAfterwards != SoyPixelsFormat::Invalid )
			{
				TargetImage.ReadOpenglPixels(ReadBackPixelsAfterwards);
			}

			OnCompleted();
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Bind::TLocalContext& Context)
			{
				Promise.Reject( Context, ExceptionString );
			};
			pContext->Queue( OnError );
		}
	};
	
	OpenglContext->PushJob( OpenglRender );
}

/*
void TWindowWrapper::RenderChain(Bind::TCallback& Params)
{
	auto& This = Params.This<TWindowWrapper>();
	auto* Isolate = Params.mIsolate;
	
	auto* pThis = &This;
	auto WindowHandle = Params.ThisObject();
	auto WindowPersistent = Params.mContext.CreatePersistent( WindowHandle );
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( Params.GetIsolate(), Resolver );

	auto TargetPersistent = v8::GetPersistent( *Isolate, Arguments[0] );
	auto* TargetImage = &v8::GetObject<TImageWrapper>(Arguments[0]);
	auto RenderCallbackPersistent = v8::GetPersistent( *Isolate, Arguments[1] );
	auto ReadBackHandle = Arguments[2];
	auto ReadBackPixelsAfterwards = GetPixelFormat( ReadBackHandle );
	auto TempPersistent = v8::GetPersistent( *Isolate, Arguments[3] );
	auto* TempImage = &v8::GetObject<TImageWrapper>(Arguments[3]);
	auto IterationCount = Local<Number>::Cast( Arguments[4] )->Int32Value();
	auto* Container = &Params.mContainer;
	
	auto OnCompleted = [=](Local<Context> Context)
	{
		//	gr: can't do this unless we're in the javascript thread...
		auto ResolverLocal = ResolverPersistent->GetLocal(*Isolate);
		auto Message = String::NewFromUtf8( Isolate, "Yay!");
		ResolverLocal->Resolve( Message );
	};
	
	
	auto OpenglContext = This.mWindow->GetContext();
	auto OpenglRender = [=]
	{
		try
		{
			//	get the texture from the image
			std::string GenerateTextureError;
			auto OnError = [&](const std::string& Error)
			{
				throw Soy::AssertException(Error);
			};
			TargetImage->GetTexture( *OpenglContext, []{}, OnError );
			TempImage->GetTexture( *OpenglContext, []{}, OnError );
			
			//	targets for chain
			auto& FinalTargetTexture = TargetImage->GetTexture();
			auto& TempTargetTexture = TempImage->GetTexture();
			
			//	do back/front buffer order so FinalTarget is always last front-target
			BufferArray<TImageWrapper*,2> Targets;
			if ( IterationCount % 2 == 1 )
			{
				Targets.PushBack( TempImage );
				Targets.PushBack( TargetImage );
			}
			else
			{
				Targets.PushBack( TargetImage );
				Targets.PushBack( TempImage );
			}
			
			for ( int it=0;	it<IterationCount;	it++ )
			{
				auto* PreviousBuffer = Targets[ (it+0) % Targets.GetSize() ];
				auto* CurrentBuffer = Targets[ (it+1) % Targets.GetSize() ];
				
				Opengl::TRenderTargetFbo RenderTarget( "Window::RenderChain", CurrentBuffer->GetTexture() );
				RenderTarget.mGenerateMipMaps = false;
				RenderTarget.Bind();
				pThis->mActiveRenderTarget = &RenderTarget;
				RenderTarget.SetViewportNormalised( Soy::Rectf(0,0,1,1) );
				try
				{
					auto ExecuteRenderCallback = [=](Local<v8::Context> Context)
					{
						auto* Isolate = Container->mIsolate;
						BufferArray<v8::Local<v8::Value>,4> CallbackParams;
						auto WindowLocal = WindowPersistent->GetLocal(*Isolate);
						auto CurrentLocal = CurrentBuffer->GetHandle();
						auto PreviousLocal = PreviousBuffer->GetHandle();
						auto IterationLocal = Number::New( Isolate, it );
						CallbackParams.PushBack( WindowLocal );
						CallbackParams.PushBack( CurrentLocal );
						CallbackParams.PushBack( PreviousLocal );
						CallbackParams.PushBack( IterationLocal );
						auto CallbackFunctionLocal = RenderCallbackPersistent->GetLocal(*Isolate);
						auto CallbackFunctionLocalFunc = v8::Local<Function>::Cast( CallbackFunctionLocal );
						auto FunctionThis = Context->Global();
						Container->ExecuteFunc( Context, CallbackFunctionLocalFunc, FunctionThis, GetArrayBridge(CallbackParams) );
					};
					
					//	immediately call the javascript callback
					Container->RunScoped( ExecuteRenderCallback );
					pThis->mActiveRenderTarget = nullptr;
					RenderTarget.Unbind();
					CurrentBuffer->OnOpenglTextureChanged();
				}
				catch(std::exception& e)
				{
					pThis->mActiveRenderTarget = nullptr;
					RenderTarget.Unbind();
					throw;
				}
				
			}

			if ( ReadBackPixelsAfterwards != SoyPixelsFormat::Invalid )
			{
				TargetImage->ReadOpenglPixels(ReadBackPixelsAfterwards);
			}
			
			//	queue the completion, doesn't need to be done instantly
			Container->QueueScoped( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Local<Context> Context)
			{
				auto ResolverLocal = ResolverPersistent->GetLocal(*Isolate);
				//	gr: does this need to be an exception? string?
				auto Error = String::NewFromUtf8( Isolate, ExceptionString.c_str() );
				//auto Exception = v8::GetException( *Context->GetIsolate(), ExceptionString)
				//ResolverLocal->Reject( Exception );
				ResolverLocal->Reject( Error );
			};
			Container->QueueScoped( OnError );
		}
	};
	OpenglContext->PushJob( OpenglRender );
	
	//	return the promise
	auto Promise = Resolver->GetPromise();
	return Promise;
}
*/



void TWindowWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<DrawQuad_FunctionName>( &TWindowWrapper::DrawQuad );
	Template.BindFunction<DrawGeometry_FunctionName>( &TWindowWrapper::DrawGeometry );
	Template.BindFunction<SetViewport_FunctionName>( SetViewport );
	Template.BindFunction<ClearColour_FunctionName>( ClearColour );
	Template.BindFunction<EnableBlend_FunctionName>( EnableBlend );
	Template.BindFunction<Render_FunctionName>( Render );
	//Template.BindFunction<RenderChain_FunctionName>( RenderChain );
	Template.BindFunction<RenderToRenderTarget_FunctionName>( RenderToRenderTarget );
	Template.BindFunction<GetScreenRect_FunctionName>( GetScreenRect );
	Template.BindFunction<SetFullscreen_FunctionName>( SetFullscreen );
	Template.BindFunction<IsFullscreen_FunctionName>( IsFullscreen );
}

void TRenderWindow::Clear(Opengl::TRenderTarget &RenderTarget)
{
	Soy::Rectf Viewport(0,0,1,1);
	RenderTarget.SetViewportNormalised( Viewport );
	
	Opengl::ClearDepth();
	EnableBlend(false);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	
	auto OpenglContext = this->GetContext();
	Opengl_IsOkay();
}


void TRenderWindow::ClearColour(Soy::TRgb Colour)
{
	Opengl::ClearColour( Colour );
}


void TRenderWindow::EnableBlend(bool Enable)
{
	if ( Enable )
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(GL_BLEND);
	}
}

Opengl::TGeometry& TRenderWindow::GetBlitQuad()
{
	if ( !mBlitQuad )
	{
		//	make mesh
		struct TVertex
		{
			vec2f	uv;
		};
		class TMesh
		{
			public:
			TVertex	mVertexes[4];
		};
		TMesh Mesh;
		Mesh.mVertexes[0].uv = vec2f( 0, 0);
		Mesh.mVertexes[1].uv = vec2f( 1, 0);
		Mesh.mVertexes[2].uv = vec2f( 1, 1);
		Mesh.mVertexes[3].uv = vec2f( 0, 1);
		Array<uint32_t> Indexes;
		
		Indexes.PushBack( 0 );
		Indexes.PushBack( 1 );
		Indexes.PushBack( 2 );
		
		Indexes.PushBack( 2 );
		Indexes.PushBack( 3 );
		Indexes.PushBack( 0 );
		
		//	for each part of the vertex, add an attribute to describe the overall vertex
		SoyGraphics::TGeometryVertex Vertex;
		auto& UvAttrib = Vertex.mElements.PushBack();
		UvAttrib.mName = "TexCoord";
		UvAttrib.SetType<vec2f>();
		UvAttrib.mIndex = 0;	//	gr: does this matter?
		
		Array<uint8> MeshData;
		MeshData.PushBackReinterpret( Mesh );
		mBlitQuad.reset( new Opengl::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex ) );
	}
	
	return *mBlitQuad;
}


void TRenderWindow::DrawGeometry(Opengl::TGeometry& Geometry,Opengl::TShader& Shader,std::function<void()>& OnBind)
{
	//	do bindings
	auto ShaderBound = Shader.Bind();
	OnBind();
	Geometry.Draw();
	Opengl_IsOkay();
}




TShaderWrapper::~TShaderWrapper()
{
	//	todo: opengl deferrefed delete
}


void TShaderWrapper::Construct(Bind::TCallback& Params)
{
	auto& This = Params.This<TShaderWrapper>();
	
	//	access to context!
	auto& RenderContext = Params.GetArgumentPointer<TOpenglContextWrapper>(0);

	auto VertSource = Params.GetArgumentString(1);
	auto FragSource = Params.GetArgumentString(2);

	auto OpenglContext = RenderContext.GetOpenglContext();

	//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
	//		but it also needs to know of the V8container to run stuff
	//		cyclic hell!
	This.CreateShader( OpenglContext, VertSource.c_str(), FragSource.c_str() );
}


void TShaderWrapper::SetUniform(Bind::TCallback& Params)
{
	auto& This = Params.This<TShaderWrapper>();

	auto& Shader = *This.mShader;
	
	auto UniformName = Params.GetArgumentString(0);
	auto Uniform = Shader.GetUniform( UniformName.c_str() );
	if ( !Uniform.IsValid() )
	{
		std::stringstream Error;
		Error << "Shader missing uniform \"" << UniformName << "\"";
		//	gr: webgl gives a warning, but doesn't throw. Lets emulate that with debug output
		//throw Soy::AssertException(Error.str());
		//std::Debug << Error.str() << std::endl;
		return;
	}

	try
	{
		This.DoSetUniform( Params, Uniform );
	}
	catch(std::exception& e)
	{
		//	extra context
		std::stringstream Error;
		Error << "SetUniform(" << UniformName << ") exception: " << e.what();
		throw Soy::AssertException(Error.str());
	}
}

void TShaderWrapper::DoSetUniform(Bind::TCallback& Params,const SoyGraphics::TUniform& Uniform)
{
	auto& Shader = *mShader;

	if ( SoyGraphics::TElementType::IsImage(Uniform.mType) )
	{
		auto& Context = *mOpenglContext;
		auto BindIndex = Context.mCurrentTextureSlot++;
		auto& Image = Params.GetArgumentPointer<TImageWrapper>(1);

		//	gr: currently this needs to be immediate... but we should be on the render thread anyway?
		//	gr: planning ahead
		auto OnTextureLoaded = [&]()
		{
			auto& Texture = Image.GetTexture();
			//std::Debug << "Binding " << Texture.mTexture.mName << " to " << BindIndex << std::endl;
			Shader.SetUniform( Uniform, Texture, BindIndex );
		};
		auto OnTextureError = [](const std::string& Error)
		{
			std::Debug << "Error loading texture " << Error << std::endl;
			std::Debug << "Todo: relay to promise" << std::endl;
		};
		Image.GetTexture( Context, OnTextureLoaded, OnTextureError );
	}
	else if ( SoyGraphics::TElementType::IsFloat(Uniform.mType) )
	{
		BufferArray<float,1024*4> Floats;
		Params.GetArgumentArray( 1, GetArrayBridge(Floats) );
		
		//	Pad out if the uniform is an array and we're short...
		//	maybe need more strict alignment when enumerating sub arrays above
		auto UniformFloatCount = Uniform.GetFloatCount();
		if ( Floats.GetSize() < UniformFloatCount )
		{
			//std::Debug << "Warning: Uniform " << Uniform.mName << " only given " << Floats.GetSize() << "/" << UniformFloatCount << " floats" << std::endl;
			if ( Uniform.GetArraySize() > 1 )
			{
				for ( auto i=Floats.GetSize();	i<UniformFloatCount;	i++ )
					Floats.PushBack(0);
			}
		}
		else if ( Floats.GetSize() > UniformFloatCount )
		{
			std::Debug << "Warning: Uniform " << Uniform.mName << " given " << Floats.GetSize() << "/" << UniformFloatCount << " floats" << std::endl;
		}
		
		Shader.SetUniform( Uniform, GetArrayBridge(Floats) );
	}
	else if ( Uniform.mType == SoyGraphics::TElementType::Bool )
	{
		auto Bool =	Params.GetArgumentBool( 1 );
		Shader.SetUniform( Uniform, Bool );
	}
	else if ( Uniform.mType == SoyGraphics::TElementType::Int32 )
	{
		auto Integer = Params.GetArgumentInt( 1 );
		Shader.SetUniform( Uniform, Integer );
	}
	else
	{
		std::stringstream Error;
		Error << "Unhandled uniform type " << Uniform.mName << " for " << Uniform.mName;
		throw Soy::AssertException(Error.str());
	}
}

void TShaderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<SetUniform_FunctionName>( SetUniform );
}

void TShaderWrapper::CreateShader(std::shared_ptr<Opengl::TContext>& pContext,const char* VertSource,const char* FragSource)
{
	auto& Context = *pContext;
	//	this needs to be deffered along with the context..
	//	the TShader constructor needs to return a promise really
	if ( !Context.IsInitialised() )
		throw Soy::AssertException("Opengl context not yet initialised");
	
	std::string VertSourceStr( VertSource );
	std::string FragSourceStr( FragSource );
	mShader.reset( new Opengl::TShader( VertSourceStr, FragSourceStr, "Shader", Context ) );

	mOpenglContext = pContext;
	mShaderDealloc = [=]
	{
		pContext->QueueDelete( mShader );
	};
}




ApiOpengl::TTriangleBufferWrapper::~TTriangleBufferWrapper()
{
	//	todo: opengl deferrefed delete
}


void ApiOpengl::TTriangleBufferWrapper::CreateTemplate(Bind::TTemplate& Template)
{
}

void ApiOpengl::TTriangleBufferWrapper::Construct(Bind::TCallback& Params)
{
	//	access to context!
	auto& RenderContext = Params.GetArgumentPointer<TOpenglContextWrapper>(0);
	
	Array<float> VertexData;
	Params.GetArgumentArray(1, GetArrayBridge(VertexData) );
	
	auto VertexSize = Params.GetArgumentInt(2);

	auto VertexCount = VertexData.GetSize() / VertexSize;
	auto VertexDataOverflow = VertexData.GetSize() % VertexSize;
	if ( VertexDataOverflow > 0 )
	{
		std::stringstream Error;
		Error << "Vertex data (x" << VertexData.GetSize() << ") misaligned with size (x" << VertexSize << ")";
		throw Soy_AssertException(Error);
	}
	if ( VertexCount < 3 )
	{
		std::stringstream Error;
		Error << "Vertex data (x" << VertexData.GetSize() << ") needs to make at least 3 vertexes (got " << VertexCount << ")";
		throw Soy_AssertException(Error);
	}

	Array<uint32_t> IndexData;
	Params.GetArgumentArray(3, GetArrayBridge(IndexData) );

	//	gr: we could save this data and defer it to opengl-thread access
	CreateGeometry( GetArrayBridge(VertexData), VertexSize, GetArrayBridge(IndexData) );
}


template<typename VERTEXTYPE>
VERTEXTYPE GetVertex(ArrayBridge<float>& VertexFloats,int Index);

template<>
vec2f GetVertex<vec2f>(ArrayBridge<float>& VertexFloats,int Index)
{
	Index *= 2;
	return vec2f( VertexFloats[Index+0], VertexFloats[Index+1] );
}

template<typename VERTEXTYPE,size_t VERTEXSIZE>
Opengl::TGeometry* CreateGeometry(const std::string& VertexAttribName,ArrayBridge<float>& VertexFloats,ArrayBridge<uint32_t>& Indexes)
{
	//	make mesh
	const int VertexCount = VertexFloats.GetSize() / VERTEXSIZE;
	
	Array<VERTEXTYPE> Vertexes;
	for ( int i=0;	i<VertexCount;	i++ )
		Vertexes.PushBack( GetVertex<VERTEXTYPE>( VertexFloats, i ) );
	
	//	for each part of the vertex, add an attribute to describe the overall vertex
	SoyGraphics::TGeometryVertex Vertex;
	auto& UvAttrib = Vertex.mElements.PushBack();
	UvAttrib.mName = VertexAttribName;
	UvAttrib.SetType<VERTEXTYPE>();
	UvAttrib.mIndex = 0;	//	gr: does this matter?
	
	auto MeshData = GetArrayBridge(Vertexes).template GetSubArray<uint8_t>( 0, Vertexes.GetDataSize() );

	return new Opengl::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex );
}


void ApiOpengl::TTriangleBufferWrapper::CreateGeometry(ArrayBridge<float>&& VertexFloats,size_t VertexSize,ArrayBridge<uint32_t>&& Indexes)
{
	if ( VertexSize == 2 )
	{
		mGeometry.reset( ::CreateGeometry<vec2f,2>( "TexCoord", VertexFloats, Indexes ) );
		return;
	}
	/*
	if ( VertexSize == 3 )
	{
		mGeometry.reset( CreateGeometry<vec3f>( "Vertex", VertexFloats, VertexSize, Indexes ) );
		return;
	}
	
	if ( VertexSize == 4 )
	{
		mGeometry.reset( CreateGeometry<vec4f>( "Vertex", VertexFloats, VertexSize, Indexes ) );
		return;
	}
	 */
	
	throw Soy::AssertException("Currently only supporting 2,3,4 vertex sizes");
}

