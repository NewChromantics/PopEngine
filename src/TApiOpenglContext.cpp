#include "TApiOpenglContext.h"
#include "TApiOpengl.h"
#include "SoyOpenglWindow.h"
#include "SoyOpenglView.h"
#include "TApiCommon.h"


using namespace v8;

const char OpenglImmediateContext_TypeName[] = "OpenglImmediateContext";

const char Execute_FunctionName[] = "Execute";
const char ExecuteCompiledQueue_FunctionName[] = "ExecuteCompiledQueue";
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



template<typename TYPE>
v8::Persistent<TYPE,CopyablePersistentTraits<TYPE>> MakeLocal(v8::Isolate* Isolate,Local<TYPE> LocalHandle)
{
	Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
	PersistentHandle.Reset( Isolate, LocalHandle );
	return PersistentHandle;
}





TOpenglImmediateContextWrapper::~TOpenglImmediateContextWrapper()
{
	if ( mContext )
	{
		//	gr: dont flush on a different thread!
		//mContext->Flush(*mContext);
		mContext->WaitForThreadToFinish();
		mContext.reset();
	}
	
	mContext.reset();
}


void TOpenglImmediateContextWrapper::Construct(const v8::CallbackInfo& Arguments)
{
	using namespace v8;
	auto& Isolate = Arguments.GetIsolate();
	
	auto ParentContextHandle = Arguments.mParams[0];
	
	//auto& ParentContextOwner = v8::GetObject<TOpenglContextWrapper>(ParentContextHandle);
	auto& ParentContextOwner = v8::GetObject<TWindowWrapper>(ParentContextHandle);
	
	auto ParentContext = ParentContextOwner.mWindow->GetContext();
	if ( !ParentContext )
		throw Soy::AssertException("No parent opengl context to create shared context from");
	
	auto NewContext = ParentContext->CreateSharedContext();
	if ( !NewContext )
		throw Soy::AssertException("Failed to create new shared context");
	
	
	mContext = NewContext;
}



v8::Local<v8::Value> TOpenglImmediateContextWrapper::Execute(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<this_type>( Arguments.This() );
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
	auto OpenglContext = This.GetOpenglContext();


	auto ExecuteRenderCallback = [=](Local<v8::Context> Context)
	{
		Soy::TScopeTimerPrint Timer("Opengl.Execute callback",30);
		auto* Isolate = Container->mIsolate;
		BufferArray<v8::Local<v8::Value>,2> CallbackParams;
		auto WindowLocal = WindowPersistent->GetLocal(*Isolate);
		CallbackParams.PushBack( WindowLocal );
		auto CallbackFunctionLocal = RenderCallbackPersistent->GetLocal(*Isolate);
		auto CallbackFunctionLocalFunc = v8::Local<Function>::Cast( CallbackFunctionLocal );
		auto FunctionThis = Context->Global();
		Container->ExecuteFunc( Context, CallbackFunctionLocalFunc, FunctionThis, GetArrayBridge(CallbackParams) );
	};
	
	auto ExecuteRenderCallbackWithLock = [=](Local<v8::Context> Context)
	{
		OpenglContext->Lock();
		try
		{
			ExecuteRenderCallback(Context);
			OpenglContext->Unlock();
		}
		catch(...)
		{
			OpenglContext->Unlock();
			throw;
		}
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
	//auto& OpenglContext = *This.GetOpenglContext();
	
	if ( StealThread )
	{
		std::Debug << "Running gl execute on thread..." << std::endl;
		//	gr: we want to lock after we've gone into javascript, otherwise we get deadlocks
		//	immediately call the javascript callback
		Container->RunScoped( ExecuteRenderCallbackWithLock );
		std::Debug << "Running gl execute on thread... done" << std::endl;
		return v8::Undefined(Params.mIsolate);
	}
	else
	{
		std::Debug << "Queued gl execute..." << std::endl;
		OpenglContext->PushJob( OpenglRender );
	
		//	return the promise
		auto Promise = Resolver->GetPromise();
		return Promise;
	}
}




v8::Local<v8::Value> TOpenglImmediateContextWrapper::ExecuteCompiledQueue(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<this_type>( Arguments.This() );
	auto* Isolate = Params.mIsolate;
	
	auto Window = Arguments.This();
	auto WindowPersistent = v8::GetPersistent( *Isolate, Window );
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( Params.GetIsolate(), Resolver );
	
	
	
	//
	
	throw Soy::AssertException("Todo");
	
}


v8::Local<v8::Value> TOpenglImmediateContextWrapper::GetEnums(const v8::CallbackInfo& Params)
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



Local<FunctionTemplate> TOpenglImmediateContextWrapper::CreateTemplate(TV8Container& Container)
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
	Container.BindFunction<Execute_FunctionName>( InstanceTemplate, Execute );
	Container.BindFunction<ExecuteCompiledQueue_FunctionName>( InstanceTemplate, ExecuteCompiledQueue );

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


v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_disable(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glDisable", glDisable, Arguments );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_enable(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glEnable", glEnable, Arguments );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_cullFace(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glCullFace", glCullFace, Arguments );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_bindBuffer(const v8::CallbackInfo& Arguments)
{
	//	gr; buffers are allocated as-required, high-level they're just an id
	auto Binding = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto BufferNameJs = GetGlValue<int>( Arguments.mParams[1] );
	auto& This = v8::GetObject<this_type>( Arguments.mParams.This() );

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

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_bufferData(const v8::CallbackInfo& Arguments)
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

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_bindFramebuffer(const v8::CallbackInfo& Arguments)
{
	//	gr; buffers are allocated as-required, high-level they're just an id
	auto Binding = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto BufferNameJs = GetGlValue<int>( Arguments.mParams[1] );
	auto& This = v8::GetObject<this_type>( Arguments.mParams.This() );
	
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

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_framebufferTexture2D(const v8::CallbackInfo& Arguments)
{
	//GLAPI void APIENTRY glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
	auto target = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto attachment = GetGlValue<GLenum>( Arguments.mParams[1] );
	auto textarget = GetGlValue<GLenum>( Arguments.mParams[2] );
	auto TextureHandle = Arguments.mParams[3];
	auto level = GetGlValue<GLint>( Arguments.mParams[4] );
	
	auto& This = Arguments.GetThis<TOpenglImmediateContextWrapper>();
	
	auto& OpenglContext = *Arguments.GetThis<this_type>().GetOpenglContext();
	
	auto& Image = v8::GetObject<TImageWrapper>(TextureHandle);
	Image.GetTexture( OpenglContext, []{}, [](const std::string& Error){} );
	auto& Texture = Image.GetTexture();
	auto TextureName = Texture.mTexture.mName;
	This.mLastFrameBufferTexture = &Image;
	
	//GLAPI void APIENTRY glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
	glFramebufferTexture2D( target, attachment, textarget, TextureName, level );
	Opengl::IsOkay("glFramebufferTexture2D");
	return v8::Undefined( Arguments.mIsolate );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_bindTexture(const v8::CallbackInfo& Arguments)
{
	auto Binding = GetGlValue<GLenum>( Arguments.mParams[0] );
	auto Arg1 = Arguments.mParams[1];
	auto& This = Arguments.GetThis<this_type>();
	
	GLuint TextureName = GL_ASSET_INVALID;
	//	webgl passes in null to unbind (or 0?)
	if ( !Arg1->IsNull() )
	{
		auto& OpenglContext = *Arguments.GetThis<this_type>().GetOpenglContext();
		//	get texture
		auto& Image = v8::GetObject<TImageWrapper>( Arguments.mParams[1] );
		Image.GetTexture( OpenglContext, []{}, [](const std::string& Error){} );
		auto& Texture = Image.GetTexture();
		TextureName = Texture.mTexture.mName;
		This.mLastBoundTexture = &Image;
	}
	else
	{
		This.mLastBoundTexture = nullptr;
	}
	
	return Immediate_Func( "glBindTexture", glBindTexture, Arguments, Binding, TextureName );
}



void GetPixelData(const char* Context,Local<Value> DataHandle,ArrayBridge<uint8_t>&& PixelData8,v8::Isolate* Isolate)
{
	Soy::TScopeTimerPrint Timer(__func__,10);

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

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_texImage2D(const v8::CallbackInfo& Arguments)
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
	
	auto& This = Arguments.GetThis<TOpenglImmediateContextWrapper>();
	
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

	//	grab the target texture, and try and use our own functions, which will use client stuff
	auto* BoundTexture = This.GetBoundTexture(binding);
	//	gr: here we really wanna handle null, to get clientside allocation/buffering
	if ( BoundTexture && !PixelData.IsEmpty() )
	{
		if ( externaltype == GL_UNSIGNED_BYTE )
		{
			auto Format = Opengl::GetDownloadPixelFormat(externalformat);
			SoyPixelsRemote SourcePixels( PixelData.GetArray(), width, height, PixelData.GetDataSize(), Format );
			auto& BoundTextureTexture = BoundTexture->GetTexture();
			SoyGraphics::TTextureUploadParams UploadParams;
			UploadParams.mAllowClientStorage = true;
			BoundTextureTexture.Write( SourcePixels );
			return v8::Undefined( Arguments.mIsolate );
		}
		else
		{
			std::Debug << "Couldn't use Opengl::Texture::Write as uploading data format is " << externaltype << " (not GL_UNSIGNED_BYTE)" << std::endl;
		}
	}

	//glFinish();
	//Opengl::IsOkay("pre glFinish");

	Soy::TScopeTimerPrint Timer(__func__,10);
	glTexImage2D( binding, level, internalformat, width, height, border, externalformat, externaltype, PixelData.GetArray() );
	Opengl::IsOkay("glTexImage2D");
	
	//	see if the flush is slow!
	//glFinish();
	//Opengl::IsOkay("glFinish");
	
	if ( BoundTexture )
	{
		BoundTexture->OnOpenglTextureChanged();
	}
	else
		std::Debug << "Bound texture changed (" << width << "x" << height << ") but unknown bound" << std::endl;
	
	return v8::Undefined( Arguments.mIsolate );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_useProgram(const v8::CallbackInfo& Arguments)
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

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_texParameteri(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glTexParameteri", glTexParameteri, Arguments.mParams, &Arguments.GetIsolate() );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_vertexAttribPointer(const v8::CallbackInfo& Arguments)
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

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_enableVertexAttribArray(const v8::CallbackInfo& Arguments)
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

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_texSubImage2D(const v8::CallbackInfo& Arguments)
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
	
	//glFinish();
	//Opengl::IsOkay("pre glFinish");
	
	Soy::TScopeTimerPrint Timer(__func__,10);
	glTexSubImage2D( binding, level, xoffset, yoffset, width, height, externalformat, externaltype, PixelData.GetArray() );
	Opengl::IsOkay("glTexSubImage2D");

	//	see if the flush is slow!
	glFinish();
	Opengl::IsOkay("glFinish");

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


TImageWrapper* TOpenglImmediateContextWrapper::GetBoundFrameBufferTexture()
{
	//	texture we THINK is bound
	if ( !mLastFrameBufferTexture )
		return nullptr;

	try
	{
		//	check the current texture attached to the frame buffer is this texture
		GLint BoundTextureName = 0;
		glGetFramebufferAttachmentParameteriv( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &BoundTextureName );
		Opengl::IsOkay("glGetFramebufferAttachmentParameteriv( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &BoundTextureName )");
	
		auto& Texture = mLastFrameBufferTexture->GetTexture();
		auto LastFrameBufferTextureName = Texture.mTexture.mName;
		if ( LastFrameBufferTextureName != BoundTextureName )
		{
			std::stringstream Error;
			Error << "Bound texture is " << BoundTextureName << ", last bound is " << LastFrameBufferTextureName << " so fast read skipped";
			throw Soy::AssertException(Error.str());
		}
		return mLastFrameBufferTexture;
	}
	catch(std::exception& e)
	{
		std::Debug << "Exception getting BoundFrameBufferTexture: " << e.what();
		return nullptr;
	}
}


namespace Opengl
{
	SoyPixelsFormat::Type GetDownloadPixelFormat(GLsizei Format,GLsizei Type)
	{
		auto NaiveType = Opengl::GetDownloadPixelFormat(Format);
		if ( Type == GL_FLOAT )
		{
			auto FloatType = SoyPixelsFormat::GetFloatFormat(NaiveType);
			return FloatType;
		}
		else if ( Type == GL_UNSIGNED_BYTE )
		{
			return NaiveType;
		}
		else
		{
			std::stringstream Error;
			Error << "Trying to get proper type for pixel format(" << Format << ") with type " << Type << ". naive format: " << NaiveType;
			throw Soy::AssertException(Error.str());
		}
	}
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_readPixels(const v8::CallbackInfo& Arguments)
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
	auto ComponentSize = Opengl::GetPixelDataSize(type);

	auto& This = Arguments.GetThis<TOpenglImmediateContextWrapper>();
	
	//	we need to alloc a buffer to read into, then push it back to the output
	std::shared_ptr<Array<uint8_t>> pPixelBuffer;
	
	//	gr: fix this so float+GL_RGBA = float4
	auto ReadPixelFormat = Opengl::GetDownloadPixelFormat( format, type );
	auto ChannelCount = SoyPixelsFormat::GetChannelCount( ReadPixelFormat );
	auto TotalComponentCount = ChannelCount * width * height;
	auto ReadMeta = SoyPixelsMeta(width,height,ReadPixelFormat);
	auto OutputHandle = Arguments.mParams[6];
	auto PixelBufferDataSize = OutputHandle->IsNull() ? 0 : TotalComponentCount * ComponentSize;

	
	auto AsyncTextureHandle = Arguments.mParams[7];
	TImageWrapper* AsyncTexture = nullptr;
	if ( AsyncTextureHandle->IsObject() )
		AsyncTexture = &v8::GetObject<TImageWrapper>(AsyncTextureHandle);
	

	//	reading an image from the frame buffer
	bool DataRead = false;
	
	//bool BigTexture = (width*height) > (4*4);
	bool BigTexture = true;

	//	gr: getting mismatch here, but now passing texture directly from tensorflow... so... that's okay?
	/*
	auto* LastFrameBufferTexture = This.GetBoundFrameBufferTexture();
	if ( AsyncTexture && LastFrameBufferTexture )
	{
		if ( AsyncTexture != LastFrameBufferTexture )
		{
			std::Debug << "Async texture (" << AsyncTexture->GetMeta() << ") and LastFrameBufferTexture(" << LastFrameBufferTexture->GetMeta() << ") are different" << std::endl;
		}
		else
			std::Debug << "Async texture and LastFrameBufferTexture are SAME" << std::endl;
	}
*/
	/*
	if ( !DataRead && BigTexture && LastFrameBufferTexture )
	{
		auto& Texture = *LastFrameBufferTexture;
		std::Debug << "Known last texture: " << Texture.GetMeta() << " vs readmeta " << ReadMeta << " read componentsize: " << ComponentSize << std::endl;
		
		if ( Texture.mOpenglLastPixelReadBufferVersion == Texture.GetLatestVersion() )
		{
			auto& LastBuffer = *Texture.mOpenglLastPixelReadBuffer;
			if ( LastBuffer.GetDataSize() == PixelBuffer.GetDataSize() )
			{
				std::Debug << "(LASTKNOWN) Using cached pixel buffer(" << LastBuffer.GetDataSize() <<" bytes) in readpixels into PixelBuffer(" << PixelBuffer.GetDataSize() << " bytes)" << std::endl;
				PixelBuffer.Copy( LastBuffer );
				DataRead = true;
			}
			else
			{
				std::Debug << "(LASTKNOWN) Skipping cached pixel buffer(" << LastBuffer.GetDataSize() <<" bytes) vs PixelBuffer(" << PixelBuffer.GetDataSize() << " bytes)" << std::endl;
			}
		}
		else
		{
			std::Debug << "Last known texture pixel buffer (" << Texture.mOpenglLastPixelReadBufferVersion << ") is out of date (" << Texture.GetLatestVersion() << ")" << std::endl;
		}
	}
	*/
	
	static bool DebugAsyncUsage = false;
	
	
	//	gr: we now pass the texture manually into readpixels in tensorflow so I know exactly which one I'm looking at
	//	gr: but we technically might not know if it's actually bound.... maybe safety check will still be needed when we re-use this context
	//	gr: if it's out of data, this is probably the async read.
	if ( !DataRead && BigTexture && AsyncTexture )
	{
		auto& Texture = *AsyncTexture;
		
		//	gr: formats seem to be wrong here (texture says its float1, but reading float4, maybe reading internal meta wrong??)
		//		BUT on the plus side, we cached data at float4. so the cache matches!
		//		maybe store a meta with the pixel buffer for what was read rather than what opengl says.
		//		maybe tensorflow is reading 4 channels of a 1 channel image??
		if ( DebugAsyncUsage )
			std::Debug << "AsyncTexture texture: " << Texture.GetMeta() << " vs readmeta " << ReadMeta << "(componentsize: " << ComponentSize << ")" << std::endl;
		
		if ( Texture.mOpenglLastPixelReadBufferVersion == Texture.GetLatestVersion() )
		{
			auto& LastBuffer = *Texture.mOpenglLastPixelReadBuffer;
			if ( LastBuffer.GetDataSize() == PixelBufferDataSize )
			{
				if ( DebugAsyncUsage )
					std::Debug << "(ASYNC) Using cached pixel buffer(" << LastBuffer.GetDataSize() <<" bytes) in readpixels into PixelBuffer(" << PixelBufferDataSize << " bytes)" << std::endl;
				Soy::TScopeTimerPrint Timer("Copying cached AsyncTexture pixel buffer", 10);
				pPixelBuffer = Texture.mOpenglLastPixelReadBuffer;
				DataRead = true;
			}
			else
			{
				if ( DebugAsyncUsage )
					std::Debug << "(ASYNC) Skipping cached pixel buffer(" << LastBuffer.GetDataSize() <<" bytes) vs PixelBuffer(" << PixelBufferDataSize << " bytes)" << std::endl;
			}
		}
		else
		{
			if ( DebugAsyncUsage )
				std::Debug << "ASYNC texture pixel buffer (" << Texture.mOpenglLastPixelReadBufferVersion << ") is out of date (" << Texture.GetLatestVersion() << ")" << std::endl;
		}

	}
	
	
	auto TimerWarningMs = 10;
	//	always show timing if we're outputting to a buffer
	if ( PixelBufferDataSize > 0 )
		TimerWarningMs = 0;
	
	static bool PreFlush = false;
	if ( PreFlush && PixelBufferDataSize>0 )
	{
		Soy::TScopeTimerPrint Timer("ReadPixels pre flush", 10 );
		glFinish();
		Opengl::IsOkay("ReadPixels pre flush");
	}

	
	static bool UsePbo = true;
	if ( !DataRead && UsePbo && PixelBufferDataSize>0 )
	{
		try
		{
			SoyPixelsMeta PboMeta( width, height, ReadPixelFormat );
			
			Opengl::TPbo Pbo( PboMeta );
			{
				std::stringstream TimerName;
				TimerName << "PBO glReadPixels( " << PboMeta << ")";
				Soy::TScopeTimerPrint ReadPixelsTimer( TimerName.str().c_str(), 10 );
				Pbo.ReadPixels(type);
			}

			std::stringstream TimerName_Lock;
			TimerName_Lock << "PBO Lock( " << PboMeta << ")";
			Soy::TScopeTimerPrint ReadPixelsTimer_Lock( TimerName_Lock.str().c_str(), 10 );
			auto* pData = Pbo.LockBuffer();
			auto PboArray = GetRemoteArray<uint8_t>( pData, PboMeta.GetDataSize() );
			ReadPixelsTimer_Lock.Stop();
			
			{
				std::stringstream TimerName;
				TimerName << "PBO Copy( " << PboMeta << ")";
				Soy::TScopeTimerPrint ReadPixelsTimer( TimerName.str().c_str(), TimerWarningMs );
				
				pPixelBuffer.reset( new Array<uint8_t>() );
				pPixelBuffer->Copy(PboArray);
			}
			Pbo.UnlockBuffer();
			DataRead = true;
		}
		catch(std::exception& e)
		{
			std::Debug << "PBO read failed" << e.what() << std::endl;
		}
	}

	
	if ( !DataRead )
	{
		std::stringstream TimerName;
		TimerName << "glReadPixels( " << x << "," << y << "," << width << "x" << height << ")";
		Soy::TScopeTimerPrint ReadPixelsTimer( TimerName.str().c_str(), TimerWarningMs );
		
		if ( PixelBufferDataSize > 0 )
		{
			if ( pPixelBuffer != nullptr )
				throw Soy::AssertException("shouldn't this not be allocated yet?");
			
			pPixelBuffer.reset( new Array<uint8_t>() );
			pPixelBuffer->SetSize(PixelBufferDataSize);
		}
		
		glReadPixels( x, y, width, height, format, type, pPixelBuffer->GetArray() );
		Opengl::IsOkay("glReadPixels");
	}
	
	if ( AsyncTexture )
	{
		AsyncTexture->SetOpenglLastPixelReadBuffer(pPixelBuffer);
	}
	/*
	if ( LastFrameBufferTexture )
	{
		LastFrameBufferTexture->SetOpenglLastPixelReadBuffer(pPixelBuffer);
	}
	*/
	//	push data into output
	if ( OutputHandle->IsNull() )
	{
	}
	else if ( OutputHandle->IsFloat32Array() )
	{
		//std::Debug << "Writing out Float32Array... from PixelBuffer(" << PixelBuffer.GetDataSize() << " bytes)" << std::endl;
		
		//	gr: for our hacky readpixelsasync, we're passing the texture as the 7th arg
		auto OffsetHandle = Arguments.mParams[7];
		if ( !OffsetHandle->IsUndefined() && !OffsetHandle->IsObject() )
		{
			throw Soy::AssertException("Need to handle offset of readpixels output");
		}
		
		//	same formats, we can just copy
		if ( type == GL_FLOAT )
		{
			Soy::TScopeTimerPrint Timer("Output pixels to float array", 10 );
			auto OutputArray = OutputHandle.As<TypedArray>();
			if ( OutputArray->Length() != TotalComponentCount )
			{
				std::stringstream Error;
				Error << "Expecting output array[" <<  OutputArray->Length() << " to be " << TotalComponentCount << " in length";
				throw Soy::AssertException(Error.str());
			}
			auto OutputBufferContents = OutputArray->Buffer()->GetContents();
			auto OutputContentsArray = GetRemoteArray( static_cast<uint8_t*>( OutputBufferContents.Data() ), OutputBufferContents.ByteLength() );
			OutputContentsArray.Copy( *pPixelBuffer );
		}
		else
		{
			std::stringstream Error;
			Error << "Need to convert from " << ReadPixelFormat << " pixels to float32 array output";
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

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_viewport(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glViewport", glViewport, Arguments.mParams, &Arguments.GetIsolate() );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_scissor(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glScissor", glScissor, Arguments.mParams, &Arguments.GetIsolate() );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_activeTexture(const v8::CallbackInfo& Arguments)
{
	return Immediate_Func( "glActiveTexture", glActiveTexture, Arguments );
}

v8::Local<v8::Value> TOpenglImmediateContextWrapper::Immediate_drawElements(const v8::CallbackInfo& Arguments)
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
	
	
	//	workout if the right buffers/vao are bound?
	
	
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
		auto Result = Immediate_Func( "glDrawElements", glDrawElements, GlArguments, &Arguments.GetIsolate() );
		
		auto& This = Arguments.GetThis<this_type>();
		auto* LastFrameBufferTexture = This.GetBoundFrameBufferTexture();
		if ( LastFrameBufferTexture )
			LastFrameBufferTexture->OnOpenglTextureChanged();
		else
			std::Debug << "Warning, drew primitives, but unknown bound texture, so not updated version" << std::endl;
		
		return Result;
	}
	else
	{
		glDisable(GL_CULL_FACE);
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
		
		auto& This = Arguments.GetThis<this_type>();
		
		//	gr: hack! find the right data for here!
		auto JsName = 1000;
		auto ArrayBuffer = This.mImmediateObjects.GetBuffer(JsName);
		auto VaoBuffer = This.mImmediateObjects.GetVao(JsName);
		Opengl::BindVertexArray( VaoBuffer.mName );
		glBindBuffer( GL_ARRAY_BUFFER, ArrayBuffer.mName );
		
		//auto ArrayBufferJsName = This.mImmediateObjects.GetBufferJavascriptName(Opengl::TAsset(ArrayBufferName));
		
		//	check correct vertex array is bound
		//	gr: need to get VAO name
		GLint ArrayBufferName = 0;
		glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &ArrayBufferName );
		auto ArrayBufferJsName = This.mImmediateObjects.GetBufferJavascriptName(Opengl::TAsset(ArrayBufferName));
		//std::Debug << "Array buffer bound=" << ArrayBufferJsName << std::endl;
		
		//	count = number of indexes
		//	gr: drawarrays goes in order, not vertex re-use. 0 1 2 3 4 5 6
		glDrawArrays( GL_TRIANGLES, 0, 3 );	//	0 1 2
		Opengl::IsOkay("glDrawArrays 0 1 2");
		glDrawArrays( GL_TRIANGLES, 1, 3 );	//	1 2 3
		Opengl::IsOkay("glDrawArrays 1 2 3");
		
		
		auto* LastFrameBufferTexture = This.GetBoundFrameBufferTexture();
		if ( LastFrameBufferTexture )
			LastFrameBufferTexture->OnOpenglTextureChanged();
		else
			std::Debug << "Warning, drew primitives, but unknown bound texture, so not updated version" << std::endl;

		
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


TImageWrapper* TOpenglImmediateContextWrapper::GetBoundTexture(GLenum Binding)
{
	if ( !mLastBoundTexture )
		return nullptr;
	
	//	verify the last bound texture against the current one, and if it's the same, return it
	auto& Texture = mLastBoundTexture->GetTexture();
	
	GLint BoundTextureName = GL_ASSET_INVALID;
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &BoundTextureName );
	Opengl::IsOkay("glGetIntegerv( GL_TEXTURE_BINDING_2D)");

	if ( Texture.mTexture.mName != BoundTextureName )
	{
		std::Debug << "Last bound texture doesn't match current" << std::endl;
		return nullptr;
	}
	
	return mLastBoundTexture;
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

int OpenglObjects::GetBufferJavascriptName(Opengl::TAsset Asset)
{
	auto& Buffers = mBuffers;
	for ( int i=0;	i<Buffers.GetSize();	i++ )
	{
		auto& Pair = Buffers[i];
		if ( Pair.second == Asset )
			return Pair.first;
	}
	throw Soy::AssertException("Failed to find javascript buffer for asset");
}

