#include "TV8Container.h"

#include <SoyDebug.h>


//	normally I hate using namespace;'s...
using namespace v8;





class PopV8Allocator : public v8::ArrayBuffer::Allocator
{
public:
	virtual void* Allocate(size_t length) override;
	virtual void* AllocateUninitialized(size_t length) override;
	virtual void Free(void* data, size_t length) override;
};



V8Exception::V8Exception(v8::TryCatch& TryCatch,const std::string& Context) :
	mError	( Context )
{
	//	get the exception from v8
	auto Exception = TryCatch.Exception();
	
	String::Utf8Value ExceptionStr(Exception);
	auto ExceptionCStr = *ExceptionStr;
	mError += ": ";
	mError += ExceptionCStr;
}



void* PopV8Allocator::Allocate(size_t length)
{
	auto* Bytes = new uint8_t[length];
	for ( auto i=0;	i<length;	i++ )
		Bytes[i] = 0;
	return Bytes;
}

void* PopV8Allocator::AllocateUninitialized(size_t length)
{
	return Allocate( length );
}

void PopV8Allocator::Free(void* data, size_t length)
{
	auto* Bytes = static_cast<uint8_t*>( data );
	delete[] Bytes;
}



TV8Container::TV8Container() :
	mAllocator	( new PopV8Allocator )
{
	auto& Allocator = *mAllocator;
	
	//v8::V8::InitializeICUDefaultLocation(argv[0]);
	V8::InitializeICU(nullptr);
	//v8::V8::InitializeExternalStartupData(argv[0]);
	V8::InitializeExternalStartupData(nullptr);
	//std::unique_ptr<v8::Platform> platform = v8::platform::CreateDefaultPlatform();
	mPlatform.reset( v8::platform::CreateDefaultPlatform() );
	V8::InitializePlatform( mPlatform.get() );
	V8::Initialize();

	// Create a new Isolate and make it the current one.
	//	gr: current??
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = &Allocator;

	//	docs say "is owner" but there's no delete...
	mIsolate = v8::Isolate::New(create_params);
	
	
	//  for now, single context per isolate
	//	todo: abstract context to be per-script
	CreateContext();

	
}

void TV8Container::CreateContext()
{
    //#error check https://stackoverflow.com/questions/33168903/c-scope-and-google-v8-script-context
	auto* isolate = mIsolate;
	v8::Isolate::Scope isolate_scope(isolate);

    //  always need a handle scope to collect locals
	v8::HandleScope handle_scope(isolate);
	Local<Context> ContextLocal = v8::Context::New(isolate);
    
    Context::Scope context_scope( ContextLocal );
    
	//  save the persistent	handle
	mContext.Reset( isolate, ContextLocal );
}


void TV8Container::LoadScript(const std::string& Source)
{
	auto* isolate = mIsolate;
	Isolate::Scope isolate_scope(isolate);
	HandleScope handle_scope(isolate);
	Local<Context> context = Local<Context>::New( isolate, mContext );
	Context::Scope context_scope( context );

	// Create a string containing the JavaScript source code.
	auto* SourceCstr = Source.c_str();
	auto Sourcev8 = v8::String::NewFromUtf8( isolate, SourceCstr, v8::NewStringType::kNormal).ToLocalChecked();
	
	//	compile the source code.
	Local<Script> NewScript;
	{
		TryCatch trycatch(isolate);
		auto NewScriptMaybe = Script::Compile(context, Sourcev8);
		if ( NewScriptMaybe.IsEmpty() )
			throw V8Exception( trycatch, "Compiling script" );
		NewScript = NewScriptMaybe.ToLocalChecked();
	}

	{
		TryCatch trycatch(isolate);
		auto ScriptResultMaybe = NewScript->Run(context);
		if ( ScriptResultMaybe.IsEmpty() )
			throw V8Exception( trycatch, "Running script" );
		
		auto ScriptResult = ScriptResultMaybe.ToLocalChecked();
		v8::String::Utf8Value MainResultStr( ScriptResult );
		std::Debug << *MainResultStr << std::endl;
	}
}


void TV8Container::BindObjectType(const char* ObjectName,std::function<Local<FunctionTemplate>(TV8Container&)> GetTemplate)
{
    //  setup scope. handle scope always required to GC locals
    auto* isolate = mIsolate;
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    //	grab a local
    Local<Context> context = Local<Context>::New( isolate, mContext );
    Context::Scope context_scope( context );
    
    
    auto Global = context->Global();

    //	create new function
    auto Template = GetTemplate(*this);
    auto OpenglWindowFuncWrapperValue = Template->GetFunction();
    auto ObjectNameStr = v8::String::NewFromUtf8(isolate, ObjectName);
    auto SetResult = Global->Set( context, ObjectNameStr, OpenglWindowFuncWrapperValue);
}



void TV8Container::BindRawFunction(v8::Local<v8::Object> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&))
{
    //  setup scope. handle scope always required to GC locals
	auto* isolate = mIsolate;
	Isolate::Scope isolate_scope(isolate);
	HandleScope handle_scope(isolate);
	//	grab a local
	Local<Context> context = Local<Context>::New( isolate, mContext );
	Context::Scope context_scope( context );
	
	v8::Local<v8::FunctionTemplate> LogFuncWrapper = v8::FunctionTemplate::New(isolate, RawFunction );
	auto LogFuncWrapperValue = LogFuncWrapper->GetFunction();
	auto* FunctionNameCstr = FunctionName;
	auto SetResult = This->Set( context, v8::String::NewFromUtf8(isolate, FunctionNameCstr), LogFuncWrapperValue);
}


void TV8Container::BindRawFunction(v8::Local<v8::ObjectTemplate> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&))
{
	//  setup scope. handle scope always required to GC locals
	auto* isolate = mIsolate;
	Isolate::Scope isolate_scope(isolate);
	HandleScope handle_scope(isolate);
	//	grab a local
	Local<Context> context = Local<Context>::New( isolate, mContext );
	Context::Scope context_scope( context );
	
	v8::Local<v8::FunctionTemplate> FuncWrapper = v8::FunctionTemplate::New(isolate, RawFunction );
	auto FuncWrapperValue = FuncWrapper->GetFunction();
	auto* FunctionNameCstr = FunctionName;
	
	This->Set( isolate, FunctionNameCstr, FuncWrapperValue);
}


void TV8Container::ExecuteGlobalFunc(const std::string& FunctionName)
{
	auto Runner = [&](Local<Context> context)
	{
		auto Global = context->Global();
		auto This = Global;
		auto Result = ExecuteFunc( context, FunctionName, This );

		//	report anything that isn't undefined
		if ( !Result->IsUndefined() )
		{
			String::Utf8Value ResultStr(Result);
			std::Debug << *ResultStr << std::endl;
		}
	};
	RunScoped( Runner );
}


void TV8Container::RunScoped(std::function<void(v8::Local<v8::Context>)> Lambda)
{
	//  setup scope. handle scope always required to GC locals
	auto* isolate = mIsolate;
	Isolate::Scope isolate_scope(isolate);
	HandleScope handle_scope(isolate);
	//	grab a local
	Local<Context> context = Local<Context>::New( isolate, mContext );
	Context::Scope context_scope( context );

	Lambda( context );
}


Local<Value> TV8Container::ExecuteFunc(Local<Context> ContextHandle,const std::string& FunctionName,Local<Object> This)
{
	auto* isolate = ContextHandle->GetIsolate();
	try
	{
		auto* FunctionNameCstr = FunctionName.c_str();
		auto FuncNameKey = v8::String::NewFromUtf8( isolate, FunctionNameCstr, v8::NewStringType::kNormal ).ToLocalChecked();
		
		//  get the global object for this name
		auto FunctionHandle = This->Get( ContextHandle, FuncNameKey).ToLocalChecked();

		//  run the func
		auto Func = Local<Function>::Cast( FunctionHandle );
		
		Handle<Value> args[0];
		TryCatch trycatch(isolate);
		auto ResultMaybe = Func->Call( ContextHandle, This, 0, args );
		if ( ResultMaybe.IsEmpty() )
		{
			auto Exception = trycatch.Exception();
			String::Utf8Value ExceptionStr(Exception);
			throw Soy::AssertException( *ExceptionStr );
		}
		auto Result = ResultMaybe.ToLocalChecked();
		
		//	report anything that isn't undefined
		if ( !Result->IsUndefined() )
		{
			String::Utf8Value ResultStr(Result);
			std::Debug << *ResultStr << std::endl;
		}
		return Result;
	}
	catch(std::exception& e)
	{
		std::Debug << "Exception executing function" << ": " << e.what() << std::endl;
		return v8::Undefined(isolate);
	}
}


