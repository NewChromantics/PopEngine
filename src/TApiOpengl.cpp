#include "TApiOpengl.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

using namespace v8;

const char Window_TypeName[] = "OpenglWindow";

const char DrawQuad_FunctionName[] = "DrawQuad";
const char ClearColour_FunctionName[] = "ClearColour";
const char SetViewport_FunctionName[] = "SetViewport";
const char SetUniform_FunctionName[] = "SetUniform";
const char Render_FunctionName[] = "Render";
const char RenderChain_FunctionName[] = "RenderChain";
const char Execute_FunctionName[] = "Execute";
const char GetEnums_FunctionName[] = "GetEnums";

#define DECLARE_IMMEDIATE_FUNC_NAME(NAME)	\
const char Immediate_##NAME##_FunctionName[] = #NAME

#define DEFINE_IMMEDIATE(NAME)	DECLARE_IMMEDIATE_FUNC_NAME(NAME)
DEFINE_IMMEDIATE(disable);
DEFINE_IMMEDIATE(enable);
DEFINE_IMMEDIATE(cullFace);
DEFINE_IMMEDIATE(bindBuffer);
DEFINE_IMMEDIATE(bufferData);
DEFINE_IMMEDIATE(bindFramebuffer);
DEFINE_IMMEDIATE(framebufferTexture2D);
DEFINE_IMMEDIATE(bindTexture);
DEFINE_IMMEDIATE(texImage2D);
DEFINE_IMMEDIATE(useProgram);
DEFINE_IMMEDIATE(texParameteri);
DEFINE_IMMEDIATE(vertexAttribPointer);
DEFINE_IMMEDIATE(enableVertexAttribArray);
DEFINE_IMMEDIATE(texSubImage2D);
DEFINE_IMMEDIATE(readPixels);
DEFINE_IMMEDIATE(viewport);
DEFINE_IMMEDIATE(scissor);
DEFINE_IMMEDIATE(activeTexture);
DEFINE_IMMEDIATE(drawElements);
#undef DEFINE_IMMEDIATE


static bool ShowImmediateFunctionCalls = false;




void ApiOpengl::Bind(TV8Container& Container)
{
	Container.BindObjectType( TWindowWrapper::GetObjectTypeName(), TWindowWrapper::CreateTemplate, TV8ObjectWrapperBase::Allocate<TWindowWrapper> );
	Container.BindObjectType("OpenglShader", TShaderWrapper::CreateTemplate, nullptr );
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
		
		auto& Isolate = *context->GetIsolate();
		auto This = this->GetHandle();
		mContainer.ExecuteFunc( context, "OnRender", This );
	};
	mContainer.RunScoped( Runner );
}


void TWindowWrapper::Construct(const v8::CallbackInfo& Arguments)
{
	using namespace v8;
	auto& Isolate = Arguments.GetIsolate();
	
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
}

v8::Local<v8::Value> TWindowWrapper::DrawQuad(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;

	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );
	
	if ( Arguments.Length() >= 1 )
	{
		auto ShaderHandle = Arguments[0];
		auto& Shader = TShaderWrapper::Get( Arguments[0] );

		auto OnShaderBindHandle = Arguments[1];
		std::function<void()> OnShaderBind = []{};
		if ( !OnShaderBindHandle->IsUndefined() )
		{
			auto OnShaderBindHandleFunc = v8::Local<Function>::Cast( OnShaderBindHandle );
			OnShaderBind = [&]
			{
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


v8::Local<v8::Value> TWindowWrapper::ClearColour(const v8::CallbackInfo& Params)
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


v8::Local<v8::Value> TWindowWrapper::SetViewport(const v8::CallbackInfo& Params)
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


template<typename TYPE>
v8::Persistent<TYPE,CopyablePersistentTraits<TYPE>> MakeLocal(v8::Isolate* Isolate,Local<TYPE> LocalHandle)
{
	Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
	PersistentHandle.Reset( Isolate, LocalHandle );
	return PersistentHandle;
}

v8::Local<v8::Value> TWindowWrapper::Render(const v8::CallbackInfo& Params)
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

	auto TargetPersistent = v8::GetPersistent( *Isolate, Arguments[0] );
	auto* TargetImage = &v8::GetObject<TImageWrapper>(Arguments[0]);
	auto RenderCallbackPersistent = v8::GetPersistent( *Isolate, Arguments[1] );
	bool ReadBackPixelsAfterwards = false;
	if ( Arguments[2]->IsBoolean() )
		ReadBackPixelsAfterwards = Local<Number>::Cast(Arguments[2])->BooleanValue();
	else if ( !Arguments[2]->IsUndefined() )
		throw Soy::AssertException("3rd argument(ReadBackPixels) must be bool or undefined.");
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
		auto CallbackFunctionLocalFunc = v8::Local<Function>::Cast( CallbackFunctionLocal );
		auto FunctionThis = Context->Global();
		Container->ExecuteFunc( Context, CallbackFunctionLocalFunc, FunctionThis, GetArrayBridge(CallbackParams) );
	};
	
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
			//	gr: this auto execute automatically
			TargetImage->GetTexture( *OpenglContext, []{}, OnError );
		
			//	setup render target
			auto& TargetTexture = TargetImage->GetTexture();
			Opengl::TRenderTargetFbo RenderTarget( TargetTexture );
			RenderTarget.mGenerateMipMaps = false;
			RenderTarget.Bind();
			
			//	hack! need to turn render target into it's own javasript object
			pThis->mActiveRenderTarget = &RenderTarget;
			RenderTarget.SetViewportNormalised( Soy::Rectf(0,0,1,1) );
			try
			{
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
			if ( ReadBackPixelsAfterwards )
			{
				TargetImage->ReadOpenglPixels();
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



v8::Local<v8::Value> TWindowWrapper::RenderChain(const v8::CallbackInfo& Params)
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
	bool ReadBackPixelsAfterwards = false;
	if ( Arguments[2]->IsBoolean() )
		ReadBackPixelsAfterwards = Local<Number>::Cast(Arguments[2])->BooleanValue();
	else if ( !Arguments[2]->IsUndefined() )
		throw Soy::AssertException("3rd argument(ReadBackPixels) must be bool or undefined.");
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
				
				Opengl::TRenderTargetFbo RenderTarget( CurrentBuffer->GetTexture() );
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

			if ( ReadBackPixelsAfterwards )
			{
				TargetImage->ReadOpenglPixels();
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



v8::Local<v8::Value> TWindowWrapper::Execute(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;
	
	auto Window = Arguments.This();
	auto WindowPersistent = v8::GetPersistent( *Isolate, Window );
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( Params.GetIsolate(), Resolver );

	bool StealThread = false;
	if ( Arguments[1]->IsBoolean() )
		StealThread = v8::SafeCast<Boolean>(Arguments[1])->BooleanValue();
	else if ( !Arguments[1]->IsUndefined() )
		throw Soy::AssertException("2nd argument(StealThread) must be bool or undefined (default to false).");

	auto RenderCallbackPersistent = v8::GetPersistent( *Isolate, Arguments[0] );
	auto* Container = &Params.mContainer;
	
	auto ExecuteRenderCallback = [=](Local<v8::Context> Context)
	{
		auto* Isolate = Container->mIsolate;
		BufferArray<v8::Local<v8::Value>,2> CallbackParams;
		auto WindowLocal = WindowPersistent->GetLocal(*Isolate);
		CallbackParams.PushBack( WindowLocal );
		auto CallbackFunctionLocal = RenderCallbackPersistent->GetLocal(*Isolate);
		auto CallbackFunctionLocalFunc = v8::Local<Function>::Cast( CallbackFunctionLocal );
		auto FunctionThis = Context->Global();
		Container->ExecuteFunc( Context, CallbackFunctionLocalFunc, FunctionThis, GetArrayBridge(CallbackParams) );
	};
	
	auto OnCompleted = [ResolverPersistent,Isolate](Local<Context> Context)
	{
		//	gr: can't do this unless we're in the javascript thread...
		auto ResolverLocal = ResolverPersistent->GetLocal( *Isolate );
		auto Message = String::NewFromUtf8( Isolate, "Yay!");
		ResolverLocal->Resolve( Message );
	};
	
	auto OpenglRender = [Isolate,ResolverPersistent,Container,OnCompleted,ExecuteRenderCallback]
	{
		try
		{
			//	immediately call the javascript callback
			Container->RunScoped( ExecuteRenderCallback );
			
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
	auto& OpenglContext = *This.mWindow->GetContext();
	
	if ( StealThread )
	{
		if ( !OpenglContext.Lock() )
			throw Soy::AssertException("Failed to steal thread (lock context)");
		
		try
		{
			//	immediately call the javascript callback
			Container->RunScoped( ExecuteRenderCallback );
			OpenglContext.Unlock();
		}
		catch(...)
		{
			OpenglContext.Unlock();
			throw;
		}
		return v8::Undefined(Params.mIsolate);
	}
	else
	{
		OpenglContext.PushJob( OpenglRender );
	
		//	return the promise
		auto Promise = Resolver->GetPromise();
		return Promise;
	}
}



v8::Local<v8::Value> TWindowWrapper::GetEnums(const v8::CallbackInfo& Params)
{
	auto* Isolate = &Params.GetIsolate();
	//	make an associative array of opengl enums for immediate use
	auto ArrayHandle = Object::New( Isolate );
	
	auto PushEnum = [&](std::string Name,uint32_t Value)
	{
		//std::string Name( GlName );
		//	strip GL_ off the start
		Soy::StringTrimLeft(Name,"GL_",true);
		
		auto KeyHandle = v8::GetString( *Isolate, Name );
		auto ValueHandle = Number::New( Isolate, Value );
		ArrayHandle->Set( KeyHandle, ValueHandle );
	};
#define PUSH_ENUM(NAME)	PushEnum( #NAME, NAME )
	PUSH_ENUM( GL_TEXTURE_2D );
	PUSH_ENUM( GL_DEPTH_TEST );
	PUSH_ENUM( GL_STENCIL_TEST );
	PUSH_ENUM( GL_BLEND );
	PUSH_ENUM( GL_DITHER );
	PUSH_ENUM( GL_POLYGON_OFFSET_FILL );
	PUSH_ENUM( GL_SAMPLE_COVERAGE );
	PUSH_ENUM( GL_SCISSOR_TEST );
	PUSH_ENUM( GL_FRAMEBUFFER_COMPLETE );
	PUSH_ENUM( GL_CULL_FACE );
	PUSH_ENUM( GL_BACK );
	PUSH_ENUM( GL_TEXTURE_WRAP_S );
	PUSH_ENUM( GL_CLAMP_TO_EDGE );
	PUSH_ENUM( GL_TEXTURE_WRAP_T );
	PUSH_ENUM( GL_CLAMP_TO_EDGE );
	PUSH_ENUM( GL_TEXTURE_MIN_FILTER );
	PUSH_ENUM( GL_NEAREST );
	PUSH_ENUM( GL_TEXTURE_MAG_FILTER );
	PUSH_ENUM( GL_ARRAY_BUFFER );
	PUSH_ENUM( GL_ELEMENT_ARRAY_BUFFER );
	PUSH_ENUM( GL_FLOAT );
	PUSH_ENUM( GL_STATIC_DRAW );
	PUSH_ENUM( GL_FRAMEBUFFER );
	PUSH_ENUM( GL_RGBA );
	PUSH_ENUM( GL_RGB );
	PUSH_ENUM( GL_RGB32F );
	PUSH_ENUM( GL_RGBA32F );
	PUSH_ENUM( GL_COLOR_ATTACHMENT0 );
	PUSH_ENUM( GL_UNSIGNED_BYTE );
	PUSH_ENUM( GL_UNSIGNED_SHORT );
	PUSH_ENUM( GL_R32F );
	PUSH_ENUM( GL_RED );
	PUSH_ENUM( GL_POINTS );
	PUSH_ENUM( GL_LINE_STRIP );
	PUSH_ENUM( GL_LINE_LOOP );
	PUSH_ENUM( GL_LINES );
	PUSH_ENUM( GL_TRIANGLE_STRIP );
	PUSH_ENUM( GL_TRIANGLE_FAN );
	PUSH_ENUM( GL_TRIANGLES );

	auto MaxTextures = 32;
	for ( int t=0;	t<MaxTextures;	t++ )
	{
		std::stringstream Name;
		Name << "GL_TEXTURE" << t;
		auto Value = GL_TEXTURE0 + t;
		PushEnum( Name.str(), Value );
	}

	return ArrayHandle;
	
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
	Container.BindFunction<Render_FunctionName>( InstanceTemplate, Render );
	Container.BindFunction<RenderChain_FunctionName>( InstanceTemplate, RenderChain );
	Container.BindFunction<Execute_FunctionName>( InstanceTemplate, Execute );

#define BIND_IMMEDIATE(NAME)	\
	Container.BindFunction<Immediate_## NAME ##_FunctionName>( InstanceTemplate, Immediate_ ## NAME );{}

#define DEFINE_IMMEDIATE(NAME)	BIND_IMMEDIATE(NAME)
	DEFINE_IMMEDIATE(disable);
	DEFINE_IMMEDIATE(enable);
	DEFINE_IMMEDIATE(cullFace);
	DEFINE_IMMEDIATE(bindBuffer);
	DEFINE_IMMEDIATE(bufferData);
	DEFINE_IMMEDIATE(bindFramebuffer);
	DEFINE_IMMEDIATE(framebufferTexture2D);
	DEFINE_IMMEDIATE(bindTexture);
	DEFINE_IMMEDIATE(texImage2D);
	DEFINE_IMMEDIATE(useProgram);
	DEFINE_IMMEDIATE(texParameteri);
	DEFINE_IMMEDIATE(vertexAttribPointer);
	DEFINE_IMMEDIATE(enableVertexAttribArray);
	DEFINE_IMMEDIATE(texSubImage2D);
	DEFINE_IMMEDIATE(readPixels);
	DEFINE_IMMEDIATE(viewport);
	DEFINE_IMMEDIATE(scissor);
	DEFINE_IMMEDIATE(activeTexture);
	DEFINE_IMMEDIATE(drawElements);
#undef DEFINE_IMMEDIATE
	
	Container.BindFunction<GetEnums_FunctionName>( InstanceTemplate, GetEnums );

	//point_templ.SetAccessor(String::NewFromUtf8(isolate, "x"), GetPointX, SetPointX);
	//point_templ.SetAccessor(String::NewFromUtf8(isolate, "y"), GetPointY, SetPointY);
	
	return ConstructorFunc;
}


template<typename TYPE>
TYPE GetGlValue(Local<Value> Argument);

template<>
GLenum GetGlValue(Local<Value> Argument)
{
	if ( !Argument->IsNumber() )
	{
		std::stringstream Error;
		Error << "Expecting argument as number, but is " << v8::GetTypeName(Argument);
		if ( Argument->IsString() )
			Error << " (" << v8::GetString(Argument) << ")";
		throw Soy::AssertException(Error.str());
	}

	auto Value32 = Argument.As<Number>()->Uint32Value();
	auto Value = size_cast<GLenum>(Value32);
	return Value;
}

template<>
GLint GetGlValue(Local<Value> Argument)
{
	if ( !Argument->IsNumber() )
	{
		std::stringstream Error;
		Error << "Expecting argument as number, but is " << v8::GetTypeName(Argument);
		if ( Argument->IsString() )
			Error << " (" << v8::GetString(Argument) << ")";
		throw Soy::AssertException(Error.str());
	}
	
	auto Value32 = Argument.As<Number>()->Int32Value();
	auto Value = size_cast<GLint>(Value32);
	return Value;
}

/*
template<>
GLsizeiptr GetGlValue(Local<Value> Argument)
{
	return 0;
}
*/

template<>
GLboolean GetGlValue(Local<Value> Argument)
{
	auto ValueBool = Argument.As<Boolean>()->BooleanValue();
	return ValueBool;
}

//	gr: this for glVertexAttributePointer is an offset, so it's a number cast to a void*
template<>
const GLvoid* GetGlValue(Local<Value> Argument)
{
	auto Value32 = Argument.As<Number>()->Uint32Value();
	void* Pointer = (void*)Value32;
	return Pointer;
}


template<typename RETURN,typename ARG0>
v8::Local<v8::Value> Immediate_Func(const char* Context,RETURN(*FunctionPtr)(ARG0),const v8::CallbackInfo& Arguments,const ARG0& Arg0)
{
	if ( ShowImmediateFunctionCalls )
		std::Debug << Context << std::endl;
	FunctionPtr( Arg0 );
	Opengl::IsOkay(Context);
	return v8::Undefined(&Arguments.GetIsolate());
}

template<typename RETURN,typename ARG0,typename ARG1>
v8::Local<v8::Value> Immediate_Func(const char* Context,RETURN(*FunctionPtr)(ARG0,ARG1),const v8::CallbackInfo& Arguments,const ARG0& Arg0,const ARG0& Arg1)
{
	if ( ShowImmediateFunctionCalls )
		std::Debug << Context << std::endl;
	FunctionPtr( Arg0, Arg1 );
	Opengl::IsOkay(Context);
	return v8::Undefined(&Arguments.GetIsolate());
}

template<typename RETURN,typename ARG0>
v8::Local<v8::Value> Immediate_Func(const char* Context,RETURN(*FunctionPtr)(ARG0),const v8::CallbackInfo& Arguments)
{
	if ( ShowImmediateFunctionCalls )
		std::Debug << Context << std::endl;
	auto Arg0 = GetGlValue<ARG0>( Arguments.mParams[0] );
	FunctionPtr( Arg0 );
	Opengl::IsOkay(Context);
	return v8::Undefined(&Arguments.GetIsolate());
}

template<typename RETURN,typename ARG0,typename ARG1>
v8::Local<v8::Value> Immediate_Func(const char* Context,RETURN(*FunctionPtr)(ARG0,ARG1),const v8::CallbackInfo& Arguments)
{
	auto Arg0 = GetGlValue<ARG0>( Arguments.mParams[0] );
	auto Arg1 = GetGlValue<ARG1>( Arguments.mParams[1] );
	return Immediate_Func( Context, FunctionPtr, Arguments, Arg0, Arg1 );
}

template<typename RETURN,typename ARG0,typename ARG1,typename ARG2,typename ARGARRAY>
v8::Local<v8::Value> Immediate_Func(const char* Context,RETURN(*FunctionPtr)(ARG0,ARG1,ARG2),ARGARRAY& Arguments,v8::Isolate* Isolate)
{
	if ( ShowImmediateFunctionCalls )
		std::Debug << Context << std::endl;
	auto Arg0 = GetGlValue<ARG0>( Arguments[0] );
	auto Arg1 = GetGlValue<ARG1>( Arguments[1] );
	auto Arg2 = GetGlValue<ARG2>( Arguments[2] );
	FunctionPtr( Arg0, Arg1, Arg2 );
	Opengl::IsOkay(Context);
	return v8::Undefined(Isolate);
}

template<typename RETURN,typename ARG0,typename ARG1,typename ARG2,typename ARG3,typename ARGARRAY>
v8::Local<v8::Value> Immediate_Func(const char* Context,RETURN(*FunctionPtr)(ARG0,ARG1,ARG2,ARG3),ARGARRAY& Arguments,v8::Isolate* Isolate)
{
	if ( ShowImmediateFunctionCalls )
		std::Debug << Context << std::endl;
	auto Arg0 = GetGlValue<ARG0>( Arguments[0] );
	auto Arg1 = GetGlValue<ARG1>( Arguments[1] );
	auto Arg2 = GetGlValue<ARG2>( Arguments[2] );
	auto Arg3 = GetGlValue<ARG3>( Arguments[3] );
	if ( ShowImmediateFunctionCalls )
		std::Debug << Context << "(" << Arg0 << "," << Arg1 << "," << Arg2 << "," << Arg3 << ")" << std::endl;
	FunctionPtr( Arg0, Arg1, Arg2, Arg3 );
	Opengl::IsOkay(Context);
	return v8::Undefined(Isolate);
}

template<typename RETURN,typename ARG0,typename ARG1,typename ARG2,typename ARG3,typename ARG4>
v8::Local<v8::Value> Immediate_Func(const char* Context,RETURN(*FunctionPtr)(ARG0,ARG1,ARG2,ARG3,ARG4),const v8::CallbackInfo& Arguments)
{
	if ( ShowImmediateFunctionCalls )
		std::Debug << Context << std::endl;
	auto Arg0 = GetGlValue<ARG0>( Arguments.mParams[0] );
	auto Arg1 = GetGlValue<ARG1>( Arguments.mParams[1] );
	auto Arg2 = GetGlValue<ARG2>( Arguments.mParams[2] );
	auto Arg3 = GetGlValue<ARG3>( Arguments.mParams[3] );
	auto Arg4 = GetGlValue<ARG4>( Arguments.mParams[4] );
	FunctionPtr( Arg0, Arg1, Arg2, Arg3, Arg4 );
	Opengl::IsOkay(Context);
	return v8::Undefined(&Arguments.GetIsolate());
}


template<typename RETURN,typename ARG0,typename ARG1,typename ARG2,typename ARG3,typename ARG4,typename ARG5,typename ARGARRAY>
v8::Local<v8::Value> Immediate_Func(const char* Context,RETURN(*FunctionPtr)(ARG0,ARG1,ARG2,ARG3,ARG4,ARG5),const ARGARRAY& Arguments,v8::Isolate* Isolate)
{
	if ( ShowImmediateFunctionCalls )
		std::Debug << Context << std::endl;
	auto Arg0 = GetGlValue<ARG0>( Arguments[0] );
	auto Arg1 = GetGlValue<ARG1>( Arguments[1] );
	auto Arg2 = GetGlValue<ARG2>( Arguments[2] );
	auto Arg3 = GetGlValue<ARG3>( Arguments[3] );
	auto Arg4 = GetGlValue<ARG4>( Arguments[4] );
	auto Arg5 = GetGlValue<ARG5>( Arguments[5] );
	FunctionPtr( Arg0, Arg1, Arg2, Arg3, Arg4, Arg5 );
	Opengl::IsOkay(Context);
	return v8::Undefined(Isolate);
}


v8::Local<v8::Value> TWindowWrapper::Immediate_disable(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glDisable", glDisable, Arguments );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_enable(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glEnable", glEnable, Arguments );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_cullFace(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glCullFace", glCullFace, Arguments );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_bindBuffer(const v8::CallbackInfo& Arguments)
{
	//	gr; buffers are allocated as-required, high-level they're just an id
	auto Binding = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto BufferNameJs = GetGlValue<int>( Arguments.mParams[1] );
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.mParams.This() );

	//	gr: in CORE (but not ES), we need a VAO. To keep this simple, we keep one per Buffer
	auto VaoName = This.mImmediateObjects.GetVao(BufferNameJs).mName;
	//	bind the VAO whenever we use the buffer associated with it
	Opengl::BindVertexArray( VaoName );
	Opengl_IsOkay();
	
	//	get/alloc our instance
	auto BufferName = This.mImmediateObjects.GetBuffer(BufferNameJs).mName;
	//	gr: glIsBuffer check is useless here, but we can do it immediate after
	//A name returned by glGenBuffers, but not yet associated with a buffer object by calling glBindBuffer, is not the name of a buffer object.
	
	auto Return = Immediate_Func( "glBindBuffer", glBindBuffer, Arguments, Binding, BufferName );
	
	//	if bound, then NOW is should be valid
	if ( !glIsBuffer( BufferName ) )
	{
		std::stringstream Error;
		Error << BufferNameJs << "(" << BufferName << ") is not a valid buffer id";
		throw Soy::AssertException(Error.str());
	}
	return Return;
}

v8::Local<v8::Value> TWindowWrapper::Immediate_bufferData(const v8::CallbackInfo& Arguments)
{
	auto target = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto Arg1 = Arguments.mParams[1];
	auto usage = GetGlValue<GLenum>( Arguments.mParams[2] );

	if ( Arg1->IsNumber() )
	{
		//	uninitialised but allocated buffer
		auto SizeValue = Arg1.As<Number>();
		auto size = SizeValue->Uint32Value();
		glBufferData( target, size, nullptr, usage );
	}
	else if ( Arg1->IsFloat32Array() || Arg1->IsUint16Array() )
	{
		//	webgl2 calls have these two additional params
		auto srcOffset = 0;
		auto length = 0;
		
		auto srcOffsetHandle = Arguments.mParams[3];
		auto lengthHandle = Arguments.mParams[4];
		if ( !srcOffsetHandle->IsUndefined() || !lengthHandle->IsUndefined() )
		{
			std::stringstream Error;
			Error << "Need to handle srcoffset(" << v8::GetTypeName(srcOffsetHandle) << ") and length (" << v8::GetTypeName(lengthHandle) << ") in bufferdata";
			throw Soy::AssertException(Error.str());
		}
		
		//	gr: don't currently know if I can send a mix of ints and floats to gl buffer data for different things (vertex vs index)
		//	so for now explicity get floats or uint16's
		if ( Arg1->IsFloat32Array() )
		{
			Array<float> Data;
			v8::EnumArray( Arg1, GetArrayBridge(Data), "glBufferData" );
			auto size = Data.GetDataSize();
			glBufferData( target, size, Data.GetArray(), usage );
		}
		else if ( Arg1->IsUint16Array() )
		{
			Array<uint16_t> Data;
			EnumArray<Uint16Array>( Arg1, GetArrayBridge(Data) );
			//v8::EnumArray( Arg1, GetArrayBridge(Data), "glBufferData" );
			auto size = Data.GetDataSize();
			glBufferData( target, size, Data.GetArray(), usage );
		}
	}
	else
	{
		std::stringstream Error;
		Error << "Not yet handling " << v8::GetTypeName(Arg1) << " for glBufferData. Need to work out what we're allowed to send";
		throw Soy::AssertException(Error.str());
	}
	
	Opengl::IsOkay("glBufferData");
	return v8::Undefined( Arguments.mIsolate );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_bindFramebuffer(const v8::CallbackInfo& Arguments)
{
	//	gr; buffers are allocated as-required, high-level they're just an id
	auto Binding = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto BufferNameJs = GetGlValue<int>( Arguments.mParams[1] );
	auto& This = v8::GetObject<TWindowWrapper>( Arguments.mParams.This() );
	
	//	get/alloc our instance
	auto BufferName = This.mImmediateObjects.GetFrameBuffer(BufferNameJs).mName;
	//	gr: glIsBuffer check is useless here, but we can do it immediate after
	//A name returned by glGenBuffers, but not yet associated with a buffer object by calling glBindBuffer, is not the name of a buffer object.
	
	auto Return = Immediate_Func( "glBindFramebuffer", glBindFramebuffer, Arguments, Binding, BufferName );
	
	//	if bound, then NOW is should be valid
	if ( !glIsFramebuffer( BufferName ) )
	{
		std::stringstream Error;
		Error << BufferNameJs << "(" << BufferName << ") is not a valid Framebuffer id";
		throw Soy::AssertException(Error.str());
	}
	return Return;
}

v8::Local<v8::Value> TWindowWrapper::Immediate_framebufferTexture2D(const v8::CallbackInfo& Arguments)
{
	//GLAPI void APIENTRY glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
	auto target = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto attachment = GetGlValue<GLenum>( Arguments.mParams[1] );
	auto textarget = GetGlValue<GLenum>( Arguments.mParams[2] );
	auto TextureHandle = Arguments.mParams[3];
	auto level = GetGlValue<GLint>( Arguments.mParams[4] );
	
	auto OpenglContext = Arguments.GetThis<TWindowWrapper>().mWindow->GetContext();
	
	auto& Image = v8::GetObject<TImageWrapper>(TextureHandle);
	Image.GetTexture( *OpenglContext, []{}, [](const std::string& Error){} );
	auto& Texture = Image.GetTexture();
	auto TextureName = Texture.mTexture.mName;
	
	//GLAPI void APIENTRY glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
	glFramebufferTexture2D( target, attachment, textarget, TextureName, level );
	Opengl::IsOkay("glFramebufferTexture2D");
	return v8::Undefined( Arguments.mIsolate );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_bindTexture(const v8::CallbackInfo& Arguments)
{
	auto Binding = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto Arg1 = Arguments.mParams[1];
	
	GLuint TextureName = GL_ASSET_INVALID;
	//	webgl passes in null to unbind (or 0?)
	if ( !Arg1->IsNull() )
	{
		auto OpenglContext = Arguments.GetThis<TWindowWrapper>().mWindow->GetContext();
		//	get texture
		auto& Image = v8::GetObject<TImageWrapper>( Arguments.mParams[1] );
		Image.GetTexture( *OpenglContext, []{}, [](const std::string& Error){} );
		auto& Texture = Image.GetTexture();
		TextureName = Texture.mTexture.mName;
	}
	
	return Immediate_Func( "glBindTexture", glBindTexture, Arguments, Binding, TextureName );
}



void GetPixelData(const char* Context,Local<Value> DataHandle,ArrayBridge<uint8_t>&& PixelData8,v8::Isolate* Isolate)
{
	if ( DataHandle->IsNull() )
	{
		PixelData8.Clear();
		return;
	}
	
	//	try and detect an Image
	if ( DataHandle->IsFloat32Array())
	{
		Array<float> FloatArray;
		v8::EnumArray( DataHandle, GetArrayBridge(FloatArray), Context );
		auto FloatArrayAs8 = GetArrayBridge(FloatArray).GetSubArray<uint8_t>(0, FloatArray.GetDataSize() );
		PixelData8.Copy(FloatArrayAs8);
		return;
	}
	
	if ( DataHandle->IsObject() )
	{
		auto ObjectHandle = DataHandle.As<Object>();
		
		//	assume is ImageData for now {width, height,data}
		auto widthstring = String::NewFromUtf8(Isolate,"width");
		auto heightstring = String::NewFromUtf8(Isolate,"height");
		auto datastring = String::NewFromUtf8(Isolate,"data");
		if ( ObjectHandle->HasOwnProperty(widthstring) && ObjectHandle->HasOwnProperty(heightstring) && ObjectHandle->HasOwnProperty(datastring) )
		{
			auto DataDataHandle = ObjectHandle->Get(datastring);
			if ( DataDataHandle->IsUint8ClampedArray() )
			{
				v8::EnumArray<Uint8ClampedArray>(DataDataHandle, PixelData8 );
				return;
			}
		}
	}

	
	std::stringstream Error;
	Error << "don't know how to handle " << Context << " data of " << v8::GetTypeName(DataHandle);
	throw Soy::AssertException(Error.str());
}

v8::Local<v8::Value> TWindowWrapper::Immediate_texImage2D(const v8::CallbackInfo& Arguments)
{
	/*
	 https://developer.mozilla.org/en-US/docs/Web/API/WebGLRenderingContext/texImage2D
	 //Webgl1
	 void gl.texImage2D(target, level, internalformat, width, height, border, format, type, ArrayBufferView? pixels);
	 void gl.texImage2D(target, level, internalformat, format, type, ImageData? pixels);
	 void gl.texImage2D(target, level, internalformat, format, type, HTMLImageElement? pixels);
	 void gl.texImage2D(target, level, internalformat, format, type, HTMLCanvasElement? pixels);
	 void gl.texImage2D(target, level, internalformat, format, type, HTMLVideoElement? pixels);
	 void gl.texImage2D(target, level, internalformat, format, type, ImageBitmap? pixels);
	 
	 // WebGL2:
	 void gl.texImage2D(target, level, internalformat, width, height, border, format, type, GLintptr offset);
	 void gl.texImage2D(target, level, internalformat, width, height, border, format, type, HTMLCanvasElement source);
	 void gl.texImage2D(target, level, internalformat, width, height, border, format, type, HTMLImageElement source);
	 void gl.texImage2D(target, level, internalformat, width, height, border, format, type, HTMLVideoElement source);
	 void gl.texImage2D(target, level, internalformat, width, height, border, format, type, ImageBitmap source);
	 void gl.texImage2D(target, level, internalformat, width, height, border, format, type, ImageData source);
	 void gl.texImage2D(target, level, internalformat, width, height, border, format, type, ArrayBufferView srcData, srcOffset);
	 */
	//	gr: we're emulating webgl2
	auto binding = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto level = GetGlValue<GLint>( Arguments.mParams[1] );
	auto internalformat = GetGlValue<GLint>( Arguments.mParams[2] );
	GLsizei width;
	GLsizei height;
	GLint border;
	int externalformatIndex;
	int externaltypeIndex;
	int DataHandleIndex;
	
	//	gr: but sometimes we get webgl1 calls...
	if ( Arguments.mParams[5]->IsObject() )
	{
		//	need to grab texture size
		glGetTexLevelParameteriv(binding, level, GL_TEXTURE_WIDTH, &width );
		Opengl::IsOkay("glGetTexLevelParameteriv(GL_TEXTURE_WIDTH)");
		glGetTexLevelParameteriv(binding, level, GL_TEXTURE_HEIGHT, &height );
		Opengl::IsOkay("glGetTexLevelParameteriv(GL_TEXTURE_HEIGHT)");
		border = 0;
		externalformatIndex = 3;
		externaltypeIndex = 4;
		DataHandleIndex = 5;
	}
	else
	{
		width = GetGlValue<GLsizei>( Arguments.mParams[3] );
		height = GetGlValue<GLsizei>( Arguments.mParams[4] );
		border = GetGlValue<GLint>( Arguments.mParams[5] );
		externalformatIndex = 6;
		externaltypeIndex = 7;
		DataHandleIndex = 8;
	}
	
	//std::Debug << "glTexImage2D(" << width << "x" << height << ")" << std::endl;
	
	auto externalformat = GetGlValue<GLenum>( Arguments.mParams[externalformatIndex] );
	auto externaltype = GetGlValue<GLenum>( Arguments.mParams[externaltypeIndex] );
	auto DataHandle = Arguments.mParams[DataHandleIndex];
	
	Array<uint8_t> PixelData;
	GetPixelData( "glTexImage2D", DataHandle, GetArrayBridge(PixelData), &Arguments.GetIsolate() );
	
	if ( PixelData.GetDataSize() != 0 )
	{
		//	check format match
		auto ExpectedFormat = Opengl::GetDownloadPixelFormat(externalformat);
		auto UploadMeta = SoyPixelsMeta( width, height, ExpectedFormat );
		if ( PixelData.GetDataSize() != UploadMeta.GetDataSize() )
		{
			std::stringstream Error;
			Error << "glTexImage2D( " << UploadMeta << ", " << UploadMeta.GetDataSize() << " bytes) but data mismatch: " << PixelData.GetDataSize() << " bytes";
			throw Soy::AssertException(Error.str());
		}
	}
	
	glTexImage2D( binding, level, internalformat, width, height, border, externalformat, externaltype, PixelData.GetArray() );
	Opengl::IsOkay("glTexImage2D");
	return v8::Undefined( Arguments.mIsolate );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_useProgram(const v8::CallbackInfo& Arguments)
{
	GLuint ShaderName = GL_ASSET_INVALID;
	auto Arg0 = Arguments.mParams[0];
	if ( !Arg0->IsNull() )
	{
		auto& Shader = v8::GetObject<TShaderWrapper>( Arguments.mParams[0] );
		ShaderName = Shader.mShader->mProgram.mName;
	}
	return Immediate_Func( "glUseProgram", glUseProgram, Arguments, ShaderName );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_texParameteri(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glTexParameteri", glTexParameteri, Arguments.mParams, &Arguments.GetIsolate() );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_vertexAttribPointer(const v8::CallbackInfo& Arguments)
{
	//	webgl
	//	void gl.vertexAttribPointer(index, size, type, normalized, stride, offset);
	//GLAPI void APIENTRY glVertexAttribPointer (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
	//	glVertexAttribPointer takes a pointer, but it's actually a byte offset.
	
	//	our api sends a name instead of an attrib/vert location, so swap itout

	GLint Program = GL_ASSET_INVALID;
	glGetIntegerv( GL_CURRENT_PROGRAM, &Program );
	Opengl::IsOkay("glGet(GL_CURRENT_PROGRAM)");
	auto AttribName = v8::GetString( Arguments.mParams[0] );
	auto AttribLocation = glGetAttribLocation(Program, AttribName.c_str() );
	Opengl::IsOkay("glGetAttribLocation(AttribName)");
	auto AttribLocationHandle = v8::Number::New( Arguments.mIsolate, AttribLocation );

	BufferArray<Local<Value>,6> NewArguments;
	NewArguments.PushBack( AttribLocationHandle );
	NewArguments.PushBack( Arguments.mParams[1] );
	NewArguments.PushBack( Arguments.mParams[2] );
	NewArguments.PushBack( Arguments.mParams[3] );
	NewArguments.PushBack( Arguments.mParams[4] );
	NewArguments.PushBack( Arguments.mParams[5] );

	return Immediate_Func( "glVertexAttribPointer", glVertexAttribPointer, NewArguments, &Arguments.GetIsolate() );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_enableVertexAttribArray(const v8::CallbackInfo& Arguments)
{
	//	our api sends a name instead of an attrib/vert location, so swap itout
	
	GLint Program = GL_ASSET_INVALID;
	glGetIntegerv( GL_CURRENT_PROGRAM, &Program );
	Opengl::IsOkay("glGet(GL_CURRENT_PROGRAM)");
	auto AttribName = v8::GetString( Arguments.mParams[0] );
	auto AttribLocation = (GLuint)glGetAttribLocation(Program, AttribName.c_str() );
	Opengl::IsOkay("glGetAttribLocation(AttribName)");
	
	return Immediate_Func( "glEnableVertexAttribArray", glEnableVertexAttribArray, Arguments, AttribLocation );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_texSubImage2D(const v8::CallbackInfo& Arguments)
{
	/*
	 https://developer.mozilla.org/en-US/docs/Web/API/WebGLRenderingContext/texImage2D
	 // WebGL 1:
	 void gl.texSubImage2D(target, level, xoffset, yoffset, format, type, OBJECT
	 void gl.texSubImage2D(target, level, xoffset, yoffset, format, type, offset);
	 void gl.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, OBJECT );
	 void gl.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, OBJECT, srcOffset);
	 */
	//	gr: we're emulating webgl2
	auto binding = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto level = GetGlValue<GLint>( Arguments.mParams[1] );
	auto xoffset = GetGlValue<GLint>( Arguments.mParams[2] );
	auto yoffset = GetGlValue<GLint>( Arguments.mParams[3] );
	GLsizei width;
	GLsizei height;
	int externalformatIndex;
	int externaltypeIndex;
	int DataHandleIndex;
	
	if ( Arguments.mParams[6]->IsObject() )
	{
		//	need to grab texture size
		glGetTexLevelParameteriv(binding, level, GL_TEXTURE_WIDTH, &width );
		Opengl::IsOkay("glGetTexLevelParameteriv(GL_TEXTURE_WIDTH)");
		glGetTexLevelParameteriv(binding, level, GL_TEXTURE_HEIGHT, &height );
		Opengl::IsOkay("glGetTexLevelParameteriv(GL_TEXTURE_HEIGHT)");
		externalformatIndex = 4;
		externaltypeIndex = 5;
		DataHandleIndex = 6;
	}
	else
	{
		width = GetGlValue<GLsizei>( Arguments.mParams[4] );
		height = GetGlValue<GLsizei>( Arguments.mParams[5] );
		externalformatIndex = 6;
		externaltypeIndex = 7;
		DataHandleIndex = 8;
	}
	
	auto externalformat = GetGlValue<GLenum>( Arguments.mParams[externalformatIndex] );
	auto externaltype = GetGlValue<GLenum>( Arguments.mParams[externaltypeIndex] );
	auto DataHandle = Arguments.mParams[DataHandleIndex];
	
	Array<uint8_t> PixelData;
	GetPixelData( "glTexSubImage2D", DataHandle, GetArrayBridge(PixelData), &Arguments.GetIsolate() );
	
	glTexSubImage2D( binding, level, xoffset, yoffset, width, height, externalformat, externaltype, PixelData.GetArray() );
	Opengl::IsOkay("glTexSubImage2D");
	return v8::Undefined( Arguments.mIsolate );
}

namespace Opengl
{
	size_t	GetPixelDataSize(GLint DataType);
}

size_t Opengl::GetPixelDataSize(GLint DataType)
{
	switch( DataType )
	{
		case GL_UNSIGNED_BYTE:			return 1;
		case GL_UNSIGNED_SHORT_5_6_5:	return 2;
		case GL_UNSIGNED_SHORT_4_4_4_4:	return 2;
		case GL_UNSIGNED_SHORT_5_5_5_1:	return 2;
		case GL_UNSIGNED_SHORT:			return 2;
		case GL_FLOAT:					return 4;
	}
	std::stringstream Error;
	
	Error << "Unknown opengl Data type 0x";
	auto* DataType8 = reinterpret_cast<uint8_t*>( &DataType );
	Soy::ByteToHex(DataType8[0], Error );
	Soy::ByteToHex(DataType8[1], Error );
	Soy::ByteToHex(DataType8[2], Error );
	Soy::ByteToHex(DataType8[3], Error );
	throw Error;
}

v8::Local<v8::Value> TWindowWrapper::Immediate_readPixels(const v8::CallbackInfo& Arguments)
{
	/*
	// WebGL1:
	void gl.readPixels(x, y, width, height, format, type, pixels);

	// WebGL2:
	void gl.readPixels(x, y, width, height, format, type, GLintptr offset);
	void gl.readPixels(x, y, width, height, format, type, ArrayBufferView pixels, GLuint dstOffset);
	*/
	auto x = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto y = GetGlValue<GLint>( Arguments.mParams[1] );
	auto width = GetGlValue<GLint>( Arguments.mParams[2] );
	auto height = GetGlValue<GLsizei>( Arguments.mParams[3] );
	auto format = GetGlValue<GLsizei>( Arguments.mParams[4] );
	auto type = GetGlValue<GLint>( Arguments.mParams[5] );
	
	//	we need to alloc a buffer to read into, then push it back to the output
	Array<uint8_t> PixelBuffer;
	auto PixelFormat = Opengl::GetDownloadPixelFormat(format);
	auto ChannelCount = SoyPixelsFormat::GetChannelCount( PixelFormat );
	auto TotalComponentCount = ChannelCount * width * height;

	auto OutputHandle = Arguments.mParams[6];
	if ( !OutputHandle->IsNull() )
	{
		//	alloc buffer to read into
		auto ComponentSize = Opengl::GetPixelDataSize(type);
		PixelBuffer.SetSize( TotalComponentCount * ComponentSize );
	}
	
	glReadPixels( x, y, width, height, format, type, PixelBuffer.GetArray() );
	Opengl::IsOkay("glReadPixels");
	
	//	push data into output
	if ( OutputHandle->IsNull() )
	{
	}
	else if ( OutputHandle->IsFloat32Array() )
	{
		auto OffsetHandle = Arguments.mParams[7];
		if ( !OffsetHandle->IsUndefined() )
		{
			throw Soy::AssertException("Need to handle offset of readpixels output");
		}
		
		//	same formats, we can just copy
		if ( type == GL_FLOAT )
		{
			auto OutputArray = OutputHandle.As<TypedArray>();
			if ( OutputArray->Length() != TotalComponentCount )
			{
				std::stringstream Error;
				Error << "Expecting output array[" <<  OutputArray->Length() << " to be " << TotalComponentCount << " in length";
				throw Soy::AssertException(Error.str());
			}
			auto OutputBufferContents = OutputArray->Buffer()->GetContents();
			auto OutputContentsArray = GetRemoteArray( static_cast<uint8_t*>( OutputBufferContents.Data() ), OutputBufferContents.ByteLength() );
			OutputContentsArray.Copy( PixelBuffer );
		}
		else
		{
			std::stringstream Error;
			Error << "Need to convert from " << PixelFormat << " pixels to float32 array output";
			throw Soy::AssertException(Error.str());
		}
	}
	else
	{
		std::stringstream Error;
		Error << "glreadPixels into output buffer of " << v8::GetTypeName(OutputHandle);
		throw Soy::AssertException(Error.str());
	}
	
	return v8::Undefined( Arguments.mIsolate );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_viewport(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glViewport", glViewport, Arguments.mParams, &Arguments.GetIsolate() );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_scissor(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glScissor", glScissor, Arguments.mParams, &Arguments.GetIsolate() );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_activeTexture(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glActiveTexture", glActiveTexture, Arguments );
}

v8::Local<v8::Value> TWindowWrapper::Immediate_drawElements(const v8::CallbackInfo& Arguments)
{
	//	webgl doesnt take indexes, they're in a buffer!
	//	void gl.drawElements(mode, count, type, offset);
	//	which in Capi is
	//	GLAPI void APIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count);
	
	//	webgl expects a type... but shouldn't this have alreayd been setup in the buffer? hmm
	//A GLenum specifying the type of the values in the element array buffer. Possible values are:
	//gl.UNSIGNED_BYTE
	//gl.UNSIGNED_SHORT
	//gl.UNSIGNED_INT (OES_element_index_uint)
	
	//	extra annoying, in webgl offset
	//		A GLintptr specifying an offset in the element array buffer. Must be a valid multiple of the size of the given type.
	//	so we need to know the type... if it's not zero

	auto primitivemode = Arguments.mParams[0];
	auto count = Arguments.mParams[1];
	auto type = Arguments.mParams[2];
	auto offset = Arguments.mParams[3];

	//	here we need to verify type (eg. UNSIGNED_SHORT) matches the ARRAY_ELEMENT_BUFFER's type
	auto TheType = GetGlValue<GLenum>(type);

	GLint Program = GL_ASSET_INVALID;
	glGetIntegerv( GL_CURRENT_PROGRAM, &Program );
	Opengl::IsOkay("glGet(GL_CURRENT_PROGRAM)");
	if ( Program == GL_ASSET_INVALID )
		throw Soy::AssertException("Attempt to glDrawElements without a program bound");

	//	validate program will do a dry-run of a render to see if everything is in it's current state
	//	https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glValidateProgram.xhtml
	glValidateProgram( Program );
	Opengl::IsOkay("glValidateProgram");
	GLint Validated = GL_FALSE;
	glGetProgramiv(	Program, GL_VALIDATE_STATUS, &Validated );
	Opengl::IsOkay("glGetProgramiv");
	if ( Validated != GL_TRUE )
	{
		GLchar LogString[1000];
		GLsizei Length = sizeofarray(LogString);
		glGetProgramInfoLog( Program, Length, &Length, LogString );
		Opengl::IsOkay("glGetProgramInfoLog");
		LogString[Length-1] = '\0';
		std::Debug << "Verification output: " << LogString << std::endl;
		throw Soy::AssertException("glValidateProgram failed");
	}
	
	
	//	gr: current issue; draw elements errors, but drawarrays doesnt
	static bool UseDrawElements = false;
	
	//	GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to an enabled array and the buffer object's data store is currently mapped.
	
	if ( UseDrawElements )
	{
		BufferArray<Local<Value>,4> GlArguments;
		GlArguments.PushBack( primitivemode );
		GlArguments.PushBack( count );
		GlArguments.PushBack( type );
		GlArguments.PushBack( offset );
		
		auto Prim = GetGlValue<GLenum>(type);
		
		throw Soy::AssertException("Still trying to sort these");
		GLushort Indexes[6] =
		{
			0,1,2,
			1,3,2,
		};
		glDrawElements( GL_TRIANGLES, 2, GL_UNSIGNED_SHORT, Indexes );	//	1 2 3
		Opengl::IsOkay("glDrawElements");
		return v8::Undefined(&Arguments.GetIsolate());
		
		
		glDrawElements( GL_TRIANGLES, 1, GL_UNSIGNED_SHORT, nullptr );	//	1 2 3
		Opengl::IsOkay("glDrawElements");
		return v8::Undefined(&Arguments.GetIsolate());
		
		
		//	webgl call: .drawElements(e.TRIANGLES, 6, e.UNSIGNED_SHORT, 0)
		//GLAPI void APIENTRY glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
		return Immediate_Func( "glDrawElements", glDrawElements, GlArguments, &Arguments.GetIsolate() );
	}
	else
	{
		glDisable(GL_CULL_FACE);
		//glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
		
		//	count = number of indexes
		//	gr: drawarrays goes in order, not vertex re-use. 0 1 2 3 4 5 6
		glDrawArrays( GL_TRIANGLES, 0, 3 );	//	0 1 2
		Opengl::IsOkay("glDrawArrays 0 1 2");
		glDrawArrays( GL_TRIANGLES, 1, 3 );	//	1 2 3
		Opengl::IsOkay("glDrawArrays 1 2 3");
		return v8::Undefined(&Arguments.GetIsolate());
		/*
		BufferArray<Local<Value>,3> GlArguments;
		GlArguments.PushBack( primitivemode );
		GlArguments.PushBack( offset );
		GlArguments.PushBack( count );

		auto Prim = GetGlValue<GLenum>(primitivemode);
		auto Offset = GetGlValue<GLint>(offset);
		auto Count = GetGlValue<GLint>(count);
		return Immediate_Func( "glDrawArrays", glDrawArrays, GlArguments, &Arguments.GetIsolate() );
		*/
	}
}


void TRenderWindow::Clear(Opengl::TRenderTarget &RenderTarget)
{
	auto FrameBufferSize = RenderTarget.GetSize();
	
	Soy::Rectf Viewport(0,0,1,1);
	RenderTarget.SetViewportNormalised( Viewport );
	
	//Opengl::ClearColour( Soy::TRgb(51/255.f,204/255.f,255/255.f) );
	Opengl::ClearDepth();
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	
	
	auto OpenglContext = this->GetContext();
	Opengl_IsOkay();
}


void TRenderWindow::ClearColour(Soy::TRgb Colour)
{
	Opengl::ClearColour( Colour );
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
		"const vec4 Rect = vec4(0,0,1,1);\n"
		"in vec2 TexCoord;\n"
		"out vec2 uv;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);\n"
		"   gl_Position.xy *= Rect.zw;\n"
		"   gl_Position.xy += Rect.xy;\n"
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
		auto& Window = v8::GetObject<TWindowWrapper>(RenderTargetHandle);
		auto OpenglContext = Window.mWindow->GetContext();
		auto VertSource = v8::GetString( Arguments[1] );
		auto FragSource = v8::GetString( Arguments[2] );

		//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
		//		but it also needs to know of the V8container to run stuff
		//		cyclic hell!
		auto* NewShader = new TShaderWrapper();
		NewShader->mHandle.Reset( Isolate, Arguments.This() );
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


v8::Local<v8::Value> TShaderWrapper::SetUniform(const v8::CallbackInfo& Params)
{
	auto ThisHandle = Params.mParams.This()->GetInternalField(0);
	auto& This = v8::GetObject<TShaderWrapper>( ThisHandle );
	return This.DoSetUniform( Params );
}

v8::Local<v8::Value> TShaderWrapper::DoSetUniform(const v8::CallbackInfo& Params)
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
		throw Soy::AssertException(Error.str());
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
			//	gr: we're not using the shader state, so we currently need to manually track bind count at high level
			auto BindIndexHandle = Arguments[2];
			if ( !BindIndexHandle->IsNumber() )
				throw Soy::AssertException("Currently need to pass texture bind index (increment from 0). SetUniform(Name,Image,BindIndex)");
			auto BindIndex = BindIndexHandle.As<Number>()->Int32Value();
			
			//	get the image
			auto* Image = &v8::GetObject<TImageWrapper>(ValueHandle);
			//	gr: planning ahead
			auto OnTextureLoaded = [Image,pShader,Uniform,BindIndex]()
			{
				pShader->SetUniform( Uniform, Image->GetTexture(), BindIndex );
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
			if ( Uniform.GetArraySize() > 1 )
			{
				for ( int i=Floats.GetSize();	i<UniformFloatCount;	i++ )
					Floats.PushBack(0);
			}
		}
		
		Shader.SetUniform( Uniform, GetArrayBridge(Floats) );
	}
	else if ( Uniform.mType == SoyGraphics::TElementType::Bool )
	{
		auto ValueBool = Local<Boolean>::Cast( ValueHandle );
		auto Bool = ValueBool->Value();
		Shader.SetUniform( Uniform, Bool );
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

Opengl::TAsset OpenglObjects::GetObject(int JavascriptName,Array<std::pair<int,Opengl::TAsset>>& Buffers,std::function<void(GLuint,GLuint*)> Alloc,const char* AllocFuncName)
{
	for ( int i=0;	i<Buffers.GetSize();	i++ )
	{
		auto& Pair = Buffers[i];
		if ( Pair.first == JavascriptName )
			return Pair.second;
	}
	
	Opengl::TAsset NewBuffer;
	Alloc(1,&NewBuffer.mName);
	Opengl::IsOkay(AllocFuncName);
	if ( !NewBuffer.IsValid() )
		throw Soy::AssertException("Failed to create new buffer");
	
	Buffers.PushBack( std::make_pair(JavascriptName,NewBuffer) );
	return NewBuffer;
}


Opengl::TAsset OpenglObjects::GetVao(int JavascriptName)
{
	return GetObject( JavascriptName, mVaos, glGenVertexArrays, "glGenVertexArrays" );
}

Opengl::TAsset OpenglObjects::GetBuffer(int JavascriptName)
{
	return GetObject( JavascriptName, mBuffers, glGenBuffers, "glGenBuffers" );
}

Opengl::TAsset OpenglObjects::GetFrameBuffer(int JavascriptName)
{
	return GetObject( JavascriptName, mFrameBuffers, glGenFramebuffers, "glGenFramebuffers" );
}
