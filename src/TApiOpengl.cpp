#include "TApiOpengl.h"
#include "TApiOpenglContext.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

using namespace v8;

namespace ApiOpengl
{
	const char Namespace[] = "Pop.Opengl";
}

const char Window_TypeName[] = "Window";
const char Shader_TypeName[] = "Shader";

const char DrawQuad_FunctionName[] = "DrawQuad";
const char ClearColour_FunctionName[] = "ClearColour";
const char EnableBlend_FunctionName[] = "EnableBlend";
const char SetViewport_FunctionName[] = "SetViewport";
const char SetUniform_FunctionName[] = "SetUniform";
const char Render_FunctionName[] = "Render";
const char RenderChain_FunctionName[] = "RenderChain";
const char GetScreenRect_FunctionName[] = "GetScreenRect";



void ApiOpengl::Bind(TV8Container& Container)
{
	Container.CreateGlobalObjectInstance("", Namespace);

	Container.BindObjectType( TWindowWrapper::GetObjectTypeName(), TWindowWrapper::CreateTemplate, TV8ObjectWrapperBase::Allocate<TWindowWrapper>, Namespace );
	Container.BindObjectType( TOpenglImmediateContextWrapper::GetObjectTypeName(), TOpenglImmediateContextWrapper::CreateTemplate, TV8ObjectWrapperBase::Allocate<TOpenglImmediateContextWrapper>, Namespace );
	Container.BindObjectType( Shader_TypeName, TShaderWrapper::CreateTemplate, nullptr, Namespace );
}


TWindowWrapper::~TWindowWrapper()
{
	if ( mWindow )
	{
		mWindow->WaitToFinish();
		mWindow.reset();
	}
}

void TWindowWrapper::OnRender(Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext)
{
	//  call javascript
	auto Runner = [&](Local<Context> context)
	{
		LockContext();
		
		if ( !mWindow )
			std::Debug << "Should there be a window here in OnRender?" << std::endl;
		else
			mWindow->Clear( RenderTarget );
		
		//	gr: allow this to fail silently if the user has assigned nothing
		//	gr: kinda want a specific "is undefined" exception so we don't miss important things
		static bool SwallowException = false;
		try
		{
			auto This = this->GetHandle();
			auto Func = v8::GetFunction( context, This, "OnRender" );
			BufferArray<Local<Value>,1> Args;
			mContainer.ExecuteFunc( context, Func, This, GetArrayBridge(Args) );
		}
		catch(std::exception& e)
		{
			if ( SwallowException )
				return;
			throw;
		}
	};
	mContainer.RunScoped( Runner );
}

void TWindowWrapper::OnMouseFunc(const TMousePos& MousePos,SoyMouseButton::Type MouseButton,const std::string& MouseFuncName)
{
	//  call javascript
	auto Runner = [=](Local<Context> Context)
	{
		try
		{
			auto This = this->GetHandle();
			auto Func = v8::GetFunction( Context, This, MouseFuncName );
			BufferArray<Local<Value>,3> Args;
		
			Args.PushBack( v8::Number::New(Context->GetIsolate(), MousePos.x ) );
			Args.PushBack( v8::Number::New(Context->GetIsolate(), MousePos.y ) );
			Args.PushBack( v8::Number::New(Context->GetIsolate(), MouseButton ) );
			
			mContainer.ExecuteFunc( Context, Func, This, GetArrayBridge(Args) );
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in " << MouseFuncName << ": " << e.what() << std::endl;
		}
	};
	mContainer.QueueScoped( Runner );
}


bool TWindowWrapper::OnTryDragDrop(ArrayBridge<std::string>& Filenames)
{
	//  call javascript
	bool Result = false;
	auto Runner = [&](Local<Context> Context)
	{
		auto& Isolate = *Context->GetIsolate();
		auto This = this->GetHandle();
		auto Func = v8::GetFunction( Context, This, "OnTryDragDrop" );
		
		auto GetFilename = [&](size_t Index)
		{
			return v8::GetString( Isolate, Filenames[Index] );
		};
		auto FilenamesArray = v8::GetArray( Isolate, Filenames.GetSize(), GetFilename );
		BufferArray<Local<Value>,2> Args;
		
		Args.PushBack( FilenamesArray );
		auto ResultHandle = mContainer.ExecuteFunc( Context, Func, This, GetArrayBridge(Args) );
		auto ResultBoolHandle = v8::SafeCast<Boolean>( ResultHandle );
		Result = ResultBoolHandle->BooleanValue();
	};
	
	try
	{
		mContainer.RunScoped( Runner );
		return Result;
	}
	catch(std::exception& e)
	{
		std::Debug << "Exception in OnTryDragDrop: " << e.what() << std::endl;
		return false;
	}
}


void TWindowWrapper::OnDragDrop(ArrayBridge<std::string>& FilenamesOrig)
{
	//	copy for queue
	Array<std::string> Filenames( FilenamesOrig );
	//  call javascript
	auto Runner = [=](Local<Context> Context)
	{
		auto& Isolate = *Context->GetIsolate();
		auto This = this->GetHandle();
		auto Func = v8::GetFunction( Context, This, "OnDragDrop" );
		
		auto GetFilename = [&](size_t Index)
		{
			return v8::GetString( Isolate, Filenames[Index] );
		};
		auto FilenamesArray = v8::GetArray( Isolate, Filenames.GetSize(), GetFilename );
		BufferArray<Local<Value>,2> Args;
		
		Args.PushBack( FilenamesArray );
		
		try
		{
			mContainer.ExecuteFunc( Context, Func, This, GetArrayBridge(Args) );
		}
		catch (std::exception& e)
		{
			std::Debug << "Exception in OnDragDrop: " << e.what() << std::endl;
		}
	};
	
	mContainer.QueueScoped( Runner );
}




void TWindowWrapper::Construct(v8::TCallback& Arguments)
{
	using namespace v8;
	
	auto WindowNameHandle = Arguments.mParams[0];
	auto AutoRedrawHandle = Arguments.mParams[1];

	auto WindowName = v8::GetString( WindowNameHandle );

	TOpenglParams Params;
	Params.mDoubleBuffer = false;
	Params.mAutoRedraw = true;
	if ( AutoRedrawHandle->IsBoolean() )
	{
		Params.mAutoRedraw = v8::SafeCast<Boolean>(AutoRedrawHandle)->BooleanValue();
	}
	mWindow.reset( new TRenderWindow( WindowName, Params ) );
	
	auto OnRender = [this](Opengl::TRenderTarget& RenderTarget,std::function<void()> LockContext)
	{
		this->OnRender( RenderTarget, LockContext );
	};

	mWindow->mOnRender = OnRender;
	mWindow->mOnMouseDown = [this](const TMousePos& Pos,SoyMouseButton::Type Button)	{	this->OnMouseFunc(Pos,Button,"OnMouseDown");	};
	mWindow->mOnMouseUp = [this](const TMousePos& Pos,SoyMouseButton::Type Button)		{	this->OnMouseFunc(Pos,Button,"OnMouseUp");	};
	mWindow->mOnMouseMove = [this](const TMousePos& Pos,SoyMouseButton::Type Button)	{	this->OnMouseFunc(Pos,Button,"OnMouseMove");	};
	mWindow->mOnTryDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	return this->OnTryDragDrop(Filenames);	};
	mWindow->mOnDragDrop = [this](ArrayBridge<std::string>& Filenames)	{	this->OnDragDrop(Filenames);	};
}

v8::Local<v8::Value> TWindowWrapper::DrawQuad(v8::TCallback& Params)
{
	auto& Arguments = Params.mParams;

	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );
	
	if ( Arguments.Length() >= 1 )
	{
		auto ShaderHandle = Arguments[0];
		auto& Shader = TShaderWrapper::Get( ShaderHandle );

		auto OnShaderBindHandle = Arguments[1];
		std::function<void()> OnShaderBind = []{};
		if ( !OnShaderBindHandle->IsUndefined() )
		{
			OnShaderBind = [&]
			{
				auto OnShaderBindHandleFunc = v8::Local<Function>::Cast( OnShaderBindHandle );
				auto OnShaderBindThis = Params.mContext->Global();
				BufferArray<Local<Value>,1> Args;
				Args.PushBack( ShaderHandle );
				Params.mContainer.ExecuteFunc( Params.mContext, OnShaderBindHandleFunc, OnShaderBindThis, GetArrayBridge(Args) );
			};
		}
		
		This.mWindow->DrawQuad( *Shader.mShader, OnShaderBind );
	}
	else
	{
		This.mWindow->DrawQuad();
	}
	
	return v8::Undefined(Params.mIsolate);
}


v8::Local<v8::Value> TWindowWrapper::ClearColour(v8::TCallback& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );
	
	if ( Arguments.Length() != 3 )
		throw Soy::AssertException("Expecting 3 arguments for ClearColour(r,g,b)");

	auto Red = Local<Number>::Cast( Arguments[0] );
	auto Green = Local<Number>::Cast( Arguments[1] );
	auto Blue = Local<Number>::Cast( Arguments[2] );
	Soy::TRgb Colour( Red->Value(), Green->Value(), Blue->Value() );
		
	This.mWindow->ClearColour( Colour );
	return v8::Undefined(Params.mIsolate);
}


v8::Local<v8::Value> TWindowWrapper::EnableBlend(v8::TCallback& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );
	
	auto EnableHandle = Arguments[0];
	bool Enable = true;
	if ( !EnableHandle->IsUndefined() )
		Enable = v8::SafeCast<Boolean>(EnableHandle)->BooleanValue();
	
	This.mWindow->EnableBlend( Enable );
	
	return v8::Undefined(Params.mIsolate);
}


v8::Local<v8::Value> TWindowWrapper::SetViewport(v8::TCallback& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );

	BufferArray<float,4> Viewportxywh;
	v8::EnumArray( Arguments[0], GetArrayBridge(Viewportxywh), "SetViewport" );
	Soy::Rectf ViewportRect( Viewportxywh[0], Viewportxywh[1], Viewportxywh[2], Viewportxywh[3] );
	
	if ( !This.mActiveRenderTarget )
		throw Soy::AssertException("No active render target");
	
	This.mActiveRenderTarget->SetViewportNormalised( ViewportRect );
	
	return v8::Undefined(Params.mIsolate);
}

//	window specific
v8::Local<v8::Value> TWindowWrapper::GetScreenRect(v8::TCallback& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );

	auto ScreenRect = This.mWindow->GetScreenRect();
	
	BufferArray<int32_t,4> ScreenRect4;
	ScreenRect4.PushBack(ScreenRect.x);
	ScreenRect4.PushBack(ScreenRect.y);
	ScreenRect4.PushBack(ScreenRect.w);
	ScreenRect4.PushBack(ScreenRect.h);
	auto ScreenRectArray = v8::GetArray( Params.GetIsolate(), GetArrayBridge(ScreenRect4) );
	
	return ScreenRectArray;
}


SoyPixelsFormat::Type GetPixelFormat(Local<Value> Handle,bool UndefinedIsInvalid=true)
{
	if ( Handle->IsString() )
	{
		auto ReadBackFormatString = v8::GetString(Handle);
		auto Format = SoyPixelsFormat::ToType(ReadBackFormatString);
		return Format;
	}
	
	if ( Handle->IsUndefined() && UndefinedIsInvalid )
		return SoyPixelsFormat::Invalid;
	
	throw Soy::AssertException("Argument must be string(format eg. 'RGBA') or undefined.");
}

v8::Local<v8::Value> TWindowWrapper::Render(v8::TCallback& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;

	auto* pThis = &This;
	auto Window = Arguments.This();
	auto WindowPersistent = v8::GetPersistent( *Isolate, Window );
	
	//	gr: got a crash here where v8 was writing to 0xaaaaaaaaa
	//		which is scribbled memory (freshly initialised)
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( Params.GetIsolate(), Resolver );

	auto TargetHandle = Arguments[0];
	auto CallbackHandle = v8::SafeCast<Function>(Arguments[1]);
	auto ReadbackHandle = Arguments[2];
	
	auto TargetPersistent = v8::GetPersistent( *Isolate, TargetHandle );
	auto* TargetImage = &v8::GetObject<TImageWrapper>(TargetHandle);
	auto RenderCallbackPersistent = v8::GetPersistent( *Isolate, CallbackHandle );
	auto ReadBackPixelsAfterwards = GetPixelFormat( ReadbackHandle );
	
	auto* Container = &Params.mContainer;
	
	auto ExecuteRenderCallback = [=](Local<v8::Context> Context)
	{
		auto* Isolate = Container->mIsolate;
		BufferArray<v8::Local<v8::Value>,2> CallbackParams;
		auto WindowLocal = WindowPersistent->GetLocal(*Isolate);
		auto TargetLocal = TargetPersistent->GetLocal(*Isolate);
		CallbackParams.PushBack( WindowLocal );
		CallbackParams.PushBack( TargetLocal );
		auto CallbackFunctionLocal = RenderCallbackPersistent->GetLocal(*Isolate);
		auto FunctionThis = Context->Global();
		auto ExecuteResult = Container->ExecuteFunc( Context, CallbackFunctionLocal, FunctionThis, GetArrayBridge(CallbackParams) );
		//	todo: return this result to the promise
	};
	
	auto OnCompleted = [=](Local<Context> Context)
	{
		auto ResolverLocal = ResolverPersistent->GetLocal(*Isolate);
		auto TargetLocal = TargetPersistent->GetLocal(*Isolate);
		ResolverLocal->Resolve( TargetLocal );
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
			//	gr: this auto execute automatically
			TargetImage->GetTexture( *OpenglContext, []{}, OnError );
		
			//	setup render target
			auto& TargetTexture = TargetImage->GetTexture();
			Opengl::TRenderTargetFbo RenderTarget( "Window::Render", TargetTexture );
			RenderTarget.mGenerateMipMaps = false;
			RenderTarget.Bind();
			
			//	hack! need to turn render target into it's own javasript object
			pThis->mActiveRenderTarget = &RenderTarget;
			RenderTarget.SetViewportNormalised( Soy::Rectf(0,0,1,1) );
			try
			{
				Soy::TScopeTimerPrint Timer("Opengl.Render callback",10);
				//	immediately call the javascript callback
				Container->RunScoped( ExecuteRenderCallback );
				pThis->mActiveRenderTarget = nullptr;
				RenderTarget.Unbind();
			}
			catch(std::exception& e)
			{
				pThis->mActiveRenderTarget = nullptr;
				RenderTarget.Unbind();
				throw;
			}

			TargetImage->OnOpenglTextureChanged();
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


v8::Local<v8::Value> TWindowWrapper::RenderChain(v8::TCallback& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;
	
	auto* pThis = &This;
	auto Window = Arguments.This();
	auto WindowPersistent = v8::GetPersistent( *Isolate, Window );
	
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




Local<FunctionTemplate> TWindowWrapper::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);
	
	
	//	add members
	Container.BindFunction<DrawQuad_FunctionName>( InstanceTemplate, DrawQuad );
	Container.BindFunction<SetViewport_FunctionName>( InstanceTemplate, SetViewport );
	Container.BindFunction<ClearColour_FunctionName>( InstanceTemplate, ClearColour );
	Container.BindFunction<EnableBlend_FunctionName>( InstanceTemplate, EnableBlend );
	Container.BindFunction<Render_FunctionName>( InstanceTemplate, Render );
	Container.BindFunction<RenderChain_FunctionName>( InstanceTemplate, RenderChain );
	Container.BindFunction<GetScreenRect_FunctionName>( InstanceTemplate, GetScreenRect );
	
	
	return ConstructorFunc;
}

void TRenderWindow::Clear(Opengl::TRenderTarget &RenderTarget)
{
	auto FrameBufferSize = RenderTarget.GetSize();
	
	Soy::Rectf Viewport(0,0,1,1);
	RenderTarget.SetViewportNormalised( Viewport );
	
	//Opengl::ClearColour( Soy::TRgb(51/255.f,204/255.f,255/255.f) );
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
		Array<size_t> Indexes;
		
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

void TRenderWindow::DrawQuad()
{
	//	allocate objects we need!
	if ( !mDebugShader )
	{
		auto& BlitQuad = GetBlitQuad();
		auto& Context = *GetContext();
		
		auto VertShader =
		"#version 410\n"
		//"uniform vec4 Rect;\n"
		"uniform vec4 VertexRect = vec4(0,0,1,1);\n"
		"in vec2 TexCoord;\n"
		"out vec2 uv;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);\n"
		"   gl_Position.xy *= VertexRect.zw;\n"
		"   gl_Position.xy += VertexRect.xy;\n"
		//	move to view space 0..1 to -1..1
		"	gl_Position.xy *= vec2(2,2);\n"
		"	gl_Position.xy -= vec2(1,1);\n"
		"	uv = vec2(TexCoord.x,1-TexCoord.y);\n"
		"}\n";
		auto FragShader =
		"#version 410\n"
		"in vec2 uv;\n"
		//"out vec4 FragColor;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = vec4(uv.x,uv.y,0,1);\n"
		"}\n";
		
		mDebugShader.reset( new Opengl::TShader( VertShader, FragShader, "Blit shader", Context ) );
	}
	
	DrawQuad( *mDebugShader, []{} );
}


void TRenderWindow::DrawQuad(Opengl::TShader& Shader,std::function<void()> OnBind)
{
	auto& BlitQuad = GetBlitQuad();
	
	//	do bindings
	auto ShaderBound = Shader.Bind();
	OnBind();
	BlitQuad.Draw();
	Opengl_IsOkay();
}





TShaderWrapper::~TShaderWrapper()
{
	//	todo: opengl deferrefed delete
}


void TShaderWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	if ( Arguments.Length() != 3 )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "missing arguments (Window,FragSource,VertSource)"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	
	//	gr: auto catch this
	try
	{
		auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
		
		//	access to context!
		auto RenderTargetHandle = Arguments[0];
		auto& WindowBase = v8::GetObject<TV8ObjectWrapperBase>(RenderTargetHandle);
		auto& Window = dynamic_cast<TOpenglContextWrapper&>( WindowBase );
		auto OpenglContext = Window.GetOpenglContext();
		auto VertSource = v8::GetString( Arguments[1] );
		auto FragSource = v8::GetString( Arguments[2] );

		//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
		//		but it also needs to know of the V8container to run stuff
		//		cyclic hell!
		auto* NewShader = new TShaderWrapper();
		NewShader->mHandle = v8::GetPersistent( *Isolate, Arguments.This() );
		NewShader->mContainer = &Container;

		NewShader->CreateShader( OpenglContext, VertSource.c_str(), FragSource.c_str() );
		
		//	set fields
		This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewShader ) );
		
		// return the new object back to the javascript caller
		Arguments.GetReturnValue().Set( This );
	}
	catch(std::exception& e)
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, e.what() ));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
}


v8::Local<v8::Value> TShaderWrapper::SetUniform(v8::TCallback& Params)
{
	auto ThisHandle = Params.mParams.This()->GetInternalField(0);
	auto& This = v8::GetObject<TShaderWrapper>( ThisHandle );
	return This.DoSetUniform( Params );
}

v8::Local<v8::Value> TShaderWrapper::DoSetUniform(v8::TCallback& Params)
{
	auto& This = *this;
	auto& Arguments = Params.mParams;
	
	auto pShader = This.mShader;
	auto& Shader = *pShader;
	
	auto UniformName = v8::GetString(Arguments[0]);
	auto Uniform = Shader.GetUniform( UniformName.c_str() );
	if ( !Uniform.IsValid() )
	{
		std::stringstream Error;
		Error << "Shader missing uniform \"" << UniformName << "\"";
		//	gr: webgl gives a warning, but doesn't throw. Lets emulate that with debug output
		//throw Soy::AssertException(Error.str());
		//std::Debug << Error.str() << std::endl;
		return v8::Undefined(Params.mIsolate);
	}

	//	get type from args
	//	gr: we dont have vector types yet, so use arrays
	auto ValueHandle = Arguments[1];
	
	if ( SoyGraphics::TElementType::IsImage(Uniform.mType) )
	{
		//	for immediate mode, glActiveTexture has already been done
		//	and texture has been bound, so if we just have 1 argument, it's the index for the activetexture
		//	really we want to grab all that at a high level.
		//	we could override, but there's a possibility the shader explicitly is picking binding slots
		if ( Arguments.Length() == 2 && Arguments[1]->IsNumber() )
		{
			auto BindIndex = Arguments[1].As<Number>()->Int32Value();
			pShader->SetUniform( Uniform, BindIndex );
		}
		else
		{
			auto BindIndex = this->mCurrentTextureIndex++;
			
			//	get the image
			auto* Image = &v8::GetObject<TImageWrapper>(ValueHandle);
			//	gr: planning ahead
			auto OnTextureLoaded = [Image,pShader,Uniform,BindIndex]()
			{
				auto& Texture = Image->GetTexture();
				//std::Debug << "Binding " << Texture.mTexture.mName << " to " << BindIndex << std::endl;
				pShader->SetUniform( Uniform, Texture, BindIndex );
			};
			auto OnTextureError = [](const std::string& Error)
			{
				std::Debug << "Error loading texture " << Error << std::endl;
				std::Debug << "Todo: relay to promise" << std::endl;
			};
			Image->GetTexture( *mOpenglContext, OnTextureLoaded, OnTextureError );
		}
	}
	else if ( SoyGraphics::TElementType::IsFloat(Uniform.mType) )
	{
		BufferArray<float,1024*4> Floats;
		EnumArray( ValueHandle, GetArrayBridge(Floats), Uniform.mName );
		
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
		auto ValueBool = Local<Boolean>::Cast( ValueHandle );
		auto Bool = ValueBool->Value();
		Shader.SetUniform( Uniform, Bool );
	}
	else if ( Uniform.mType == SoyGraphics::TElementType::Int32 )
	{
		auto ValueNumber = Local<Number>::Cast( ValueHandle );
		auto Integer = ValueNumber->Int32Value();
		Shader.SetUniform( Uniform, Integer );
	}
	else
	{
		throw Soy::AssertException("Currently only image & float uniform supported");
	}

	return v8::Undefined(Params.mIsolate);
}

Local<FunctionTemplate> TShaderWrapper::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	InstanceTemplate->SetInternalFieldCount(2);
	
	Container.BindFunction<SetUniform_FunctionName>( InstanceTemplate, SetUniform );

	return ConstructorFunc;
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
