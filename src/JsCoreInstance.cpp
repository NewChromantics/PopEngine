#include "JsCoreInstance.h"
#include "SoyAssert.h"
#include "SoyFilesystem.h"
#include "TApiCommon.h"



std::string	JsCore::HandleToString(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	JSValueRef Exception = nullptr;
	auto StringJs = JSValueToStringCopy( Context, Handle, &Exception );
	
	size_t maxBufferSize = JSStringGetMaximumUTF8CStringSize(StringJs);
	char utf8Buffer[maxBufferSize];
	size_t bytesWritten = JSStringGetUTF8CString(StringJs, utf8Buffer, maxBufferSize);
	//	the last byte is a null \0 which std::string doesn't need.
	std::string utf_string = std::string(utf8Buffer, bytesWritten -1);
	return utf_string;
}

	
	
	

JsCore::TInstance::TInstance(const std::string& RootDirectory,const std::string& ScriptFilename) :
	mContextGroup	( JSContextGroupCreate() ),
	mRootDirectory	( RootDirectory )
{
	if ( !mContextGroup )
		throw Soy::AssertException("JSContextGroupCreate failed");
	
	
	//	bind first
	try
	{
		//	create a context
		mContext = CreateContext();
		
		ApiCommon::Bind( *mContext );
		/*
		ApiOpengl::Bind( *mV8Container );
		ApiOpencl::Bind( *mV8Container );
		ApiDlib::Bind( *mV8Container );
		ApiMedia::Bind( *mV8Container );
		ApiWebsocket::Bind( *mV8Container );
		ApiHttp::Bind( *mV8Container );
		ApiSocket::Bind( *mV8Container );
		
		//	gr: start the thread immediately, there should be no problems having the thread running before queueing a job
		this->Start();
		*/
		std::string BootupSource;
		Soy::FileToString( mRootDirectory + ScriptFilename, BootupSource );
		/*
		auto* Container = mV8Container.get();
		auto LoadScript = [=](v8::Local<v8::Context> Context)
		{
			Container->LoadScript( Context, BootupSource, ScriptFilename );
		};
		
		mV8Container->QueueScoped( LoadScript );
		 */
		mContext->LoadScript( BootupSource, ScriptFilename );
	}
	catch(std::exception& e)
	{
		//	clean up
		mContext.reset();
		throw;
	}
}

JsCore::TInstance::~TInstance()
{
	JSContextGroupRelease(mContextGroup);
}

std::shared_ptr<JsCore::TContext> JsCore::TInstance::CreateContext()
{
	auto Context = JSGlobalContextCreateInGroup( mContextGroup, nullptr);
	std::shared_ptr<JsCore::TContext> pContext( new TContext( Context, mRootDirectory ) );
	//mContexts.PushBack( pContext );
	return pContext;
}





JsCore::TContext::TContext(JSGlobalContextRef Context,const std::string& RootDirectory) :
	mContext		( Context ),
	mRootDirectory	( RootDirectory )
{
}

JsCore::TContext::~TContext()
{
	JSGlobalContextRelease( mContext );
}

void JsCore::TContext::LoadScript(const std::string& Source,const std::string& Filename)
{
	auto ThisHandle = JSObjectRef(nullptr);
	auto SourceJs = JSStringCreateWithUTF8CString(Source.c_str());
	auto FilenameJs = JSStringCreateWithUTF8CString(Filename.c_str());
	auto LineNumber = 0;
	JSValueRef Exception = nullptr;
	auto ResultHandle = JSEvaluateScript( mContext, SourceJs, ThisHandle, FilenameJs, LineNumber, &Exception );
	ThrowException(Exception);
	
}

void JsCore::TContext::ThrowException(JSValueRef ExceptionHandle)
{
	auto ExceptionType = JSValueGetType( mContext, ExceptionHandle );
	//	not an exception
	if ( ExceptionType == kJSTypeUndefined || ExceptionType == kJSTypeNull )
		return;

	auto ExceptionString = HandleToString( mContext, ExceptionHandle );
	throw Soy::AssertException(ExceptionString);
}

void JsCore::TContext::BindRawFunction(const char* FunctionName,JSObjectCallAsFunctionCallback Function)
{
	auto FunctionNameJs = JSStringCreateWithUTF8CString(FunctionName);
	JSObjectRef This = JSContextGetGlobalObject( mContext );
	auto Attributes = kJSPropertyAttributeNone;

	auto FunctionHandle = JSObjectMakeFunctionWithCallback( mContext, FunctionNameJs, Function );
	JSValueRef Exception = nullptr;
	JSObjectSetProperty( mContext, This, FunctionNameJs, FunctionHandle, Attributes, &Exception );
	ThrowException(Exception);
}

JSValueRef JsCore::TContext::CallFunc(std::function<JSValueRef(TCallbackInfo&)> Function,JSContextRef Context,JSObjectRef FunctionJs,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception)
{
	try
	{
		TCallbackInfo CallbackInfo;
		CallbackInfo.mContext = mContext;//Context;
		CallbackInfo.mThis = This;
		for ( auto a=0;	a<ArgumentCount;	a++ )
		{
			CallbackInfo.mArguments.PushBack( Arguments[a] );
		}
		auto Result = Function( CallbackInfo );
		return Result;
	}
	catch (std::exception& e)
	{
		auto ExceptionStr = JSStringCreateWithUTF8CString( e.what() );
		Exception = JSValueMakeString( Context, ExceptionStr );
		return JSValueMakeUndefined( Context );
	}
}


/*
JSValueRef ObjectCallAsFunctionCallback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
	cout << "Hello World" << endl;
	return JSValueMakeUndefined(ctx);
}


JsCore::TInstance::
{
	JSObjectRef globalObject = JSContextGetGlobalObject(globalContext);
	
	JSStringRef logFunctionName = JSStringCreateWithUTF8CString("log");
	JSObjectRef functionObject = JSObjectMakeFunctionWithCallback(globalContext, logFunctionName, &ObjectCallAsFunctionCallback);
	
	JSObjectSetProperty(globalContext, globalObject, logFunctionName, functionObject, kJSPropertyAttributeNone, nullptr);
	
	JSStringRef logCallStatement = JSStringCreateWithUTF8CString("log()");
	
	JSEvaluateScript(globalContext, logCallStatement, nullptr, nullptr, 1,nullptr);
	
 
	JSGlobalContextRelease(globalContext);
	JSStringRelease(logFunctionName);
	JSStringRelease(logCallStatement);
	}

*/


std::string JsCore::TCallbackInfo::GetArgumentString(size_t Index) const
{
	auto Handle = mArguments[Index];
	auto String = JsCore::HandleToString( mContext, Handle );
	return String;
}
