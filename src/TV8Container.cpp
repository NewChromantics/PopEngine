#include "TV8Container.h"

#include <SoyDebug.h>


//	normally I hate using namespace;'s...
using namespace v8;


#include "SoyOpenglWindow.h"


//	v8 template to a TWindow
class TWindowWrapper
{
public:
	static void Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static v8::Local<v8::FunctionTemplate> CreateTemplate(v8::Isolate* Isolate);
};


void TWindowWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	if ( Arguments.Length() != 1 )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "missing arg 0 (window name)"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	String::Utf8Value WindowName( Arguments[0] );
	std::Debug << "Window Wrapper constructor (" << *WindowName << ")" << std::endl;
	
	//	alloc window
	Soy::Rectf Rect( 0, 0, 300, 300 );
	TOpenglParams Params;
	auto* NewWindow = new TOpenglWindow( *WindowName, Rect, Params );
	
	//	set the field
	Arguments.This()->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWindow ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( Arguments.This() );

}

Local<FunctionTemplate> TWindowWrapper::CreateTemplate(v8::Isolate* Isolate)
{
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor );

	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	InstanceTemplate->SetInternalFieldCount(1);
	
	//point_templ.SetAccessor(String::NewFromUtf8(isolate, "x"), GetPointX, SetPointX);
	//point_templ.SetAccessor(String::NewFromUtf8(isolate, "y"), GetPointY, SetPointY);
	
	//Point* p = ...;
	//Local<Object> obj = point_templ->NewInstance();
	//obj->SetInternalField(0, External::New(isolate, p));
	
	return ConstructorFunc;
}

class PopV8Allocator : public v8::ArrayBuffer::Allocator
{
public:
	virtual void* Allocate(size_t length) override;
	virtual void* AllocateUninitialized(size_t length) override;
	virtual void Free(void* data, size_t length) override;
};







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





auto JavascriptEmpty = R"DONTPANIC(

)DONTPANIC";


auto JavascriptMain = R"DONTPANIC(

function test_function()
{
	let FragShaderSource = `
		varying vec2 oTexCoord;
		void main()
		{
			gl_FragColor = vec4(oTexCoord,0,1);
		}
	`;
	
	log("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Hello!");
	let Window2 = new OpenglWindow("Hello2!");

	let OnRender = function()
	{
		Window1.DrawQuad();
	}
	Window1.OnRender = OnRender;
	
	return "hello";
}

)DONTPANIC";



static void OnLog(CallbackInfo& Params)
{
	auto& args = Params.mParams;
	
	using namespace v8;
	
	if (args.Length() < 1)
	{
		std::Debug << "log() with no args" << std::endl;
		return;
	}

	Isolate* isolate = args.GetIsolate();
	HandleScope scope(isolate);
	for ( auto i=0;	i<args.Length();	i++ )
	{
		auto arg = args[i];
		String::Utf8Value value(arg);
		std::Debug << *value << std::endl;
	}
	
	//	 return v8::Undefined();
}
const char Log_FunctionName[] = "log";

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
	CreateContext();
	LoadScript(JavascriptEmpty);
	//LoadScript(JavascriptMain);
	//BindFunction<Log_FunctionName>(OnLog);
	//ExecuteFunc("test_function");
	
}

void TV8Contrainer::CreateContext()
{
    #error check https://stackoverflow.com/questions/33168903/c-scope-and-google-v8-script-context
	auto* isolate = mIsolate;
	v8::Isolate::Scope isolate_scope(isolate);

    //  always need a handle scope to collect locals
	v8::HandleScope handle_scope(isolate);
	Local<Context> ContextLocal = v8::Context::New(isolate);

	//  save the persistent	handle
	mContext.Reset( isolate, ContextLocal );
}


void TV8Container::LoadScript(const std::string& Source)
{
	auto* isolate = mIsolate;
	Isolate::Scope isolate_scope(isolate);
	HandleScope handle_scope(isolate);
	Local<Context> context = Local<Context>::New( isolate, mContext );
	Context::Scope context_scope( Context );

	// Create a string containing the JavaScript source code.
	auto* SourceCstr = Source.c_str();
	auto Sourcev8 = v8::String::NewFromUtf8( isolate, SourceCstr, v8::NewStringType::kNormal).ToLocalChecked();
	
	// Compile the source code.
	Local<Script> script = Script::Compile(context, Sourcev8).ToLocalChecked();

	auto MainResult = script->Run(context).ToLocalChecked();
	
	v8::String::Utf8Value MainResultStr(MainResult);
	printf("MainResultStr = %s\n", *MainResultStr);

	
	/*
	//	create new function
	auto WindowTemplate = TWindowWrapper::CreateTemplate(isolate);
	v8::Local<v8::FunctionTemplate> LogFuncWrapper = v8::FunctionTemplate::New(isolate, LogCallback);
	
	auto LogFuncWrapperValue = LogFuncWrapper->GetFunction();
	auto OpenglWindowFuncWrapperValue = WindowTemplate->GetFunction();
	
	ContextGlobal->Set( context, v8::String::NewFromUtf8(isolate, "log"), LogFuncWrapperValue);
	ContextGlobal->Set( context, v8::String::NewFromUtf8(isolate, "OpenglWindow"), OpenglWindowFuncWrapperValue);
	
	auto FuncNameKey = v8::String::NewFromUtf8( isolate, "test_function", v8::NewStringType::kNormal ).ToLocalChecked();
	
	//v8::String::NewFromUtf8(isolate, "'Hello' + ', World!'",v8::NewStringType::kNormal)
	auto FuncName = ContextGlobal->Get(FuncNameKey);
	
	auto Func = v8::Handle<v8::Function>::Cast(FuncName);
	
	v8::Handle<v8::Value> args[0];
	auto result = Func->Call( context, Func, 0, args ).ToLocalChecked();
	
	
	// Convert the result to an UTF8 string and print it.
	v8::String::Utf8Value ResultStr(result);
	printf("result = %s\n", *ResultStr);
	
	v8::String::Utf8Value MainResultStr(mainresult);
	printf("MainResultStr = %s\n", *MainResultStr);
	 */
}

void TV8Container::BindRawFunction(const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&))
{
    //  setup scope. handle scope always required to GC locals
	auto* isolate = mIsolate;
	Isolate::Scope isolate_scope(isolate);
	HandleScope handle_scope(isolate);
	//	grab a local
	Local<Context> context = Local<Context>::New( isolate, mContext );
	Context::Scope context_scope( Context );


	auto ContextGlobal = context->Global();
	/*
	//	create new function
	auto WindowTemplate = TWindowWrapper::CreateTemplate(isolate);
	auto OpenglWindowFuncWrapperValue = WindowTemplate->GetFunction();
	ContextGlobal->Set( Context, v8::String::NewFromUtf8(isolate, "OpenglWindow"), OpenglWindowFuncWrapperValue);
*/
	
	v8::Local<v8::FunctionTemplate> LogFuncWrapper = v8::FunctionTemplate::New(isolate, RawFunction );
	auto LogFuncWrapperValue = LogFuncWrapper->GetFunction();
	auto* FunctionNameCstr = FunctionName;
	ContextGlobal->Set( context, v8::String::NewFromUtf8(isolate, FunctionNameCstr), LogFuncWrapperValue);
}

void TV8Container::ExecuteFunc(const std::string& FunctionName)
{
	auto* isolate = mIsolate;
	Isolate::Scope isolate_scope(isolate);
	
	//	grab a local
	Local<Context> context = Local<Context>::New( isolate, mContext );
	//	make current context
	Context::Scope context_scope( Context );
	
	auto ContextGlobal = context->Global();

	auto* FunctionNameCstr = FunctionName.c_str();
	auto FuncNameKey = v8::String::NewFromUtf8( isolate, FunctionNameCstr, v8::NewStringType::kNormal ).ToLocalChecked();
	
	//v8::String::NewFromUtf8(isolate, "'Hello' + ', World!'",v8::NewStringType::kNormal)
	auto FuncName = ContextGlobal->Get(FuncNameKey);
	
	auto Func = Handle<Function>::Cast(FuncName);
	
	Handle<Value> args[0];
	auto Result = Func->Call( context, Func, 0, args ).ToLocalChecked();

	String::Utf8Value ResultStr(Result);
	printf("result = %s\n", *ResultStr);
}



