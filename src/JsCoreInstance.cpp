#include "JsCoreInstance.h"
#include "SoyAssert.h"
#include "SoyFilesystem.h"
#include "TApiCommon.h"





class TFunction
{
public:
	TFunction(JSContextRef Context,JSValueRef Value);
	
	JSValueRef		Call(JSObjectRef This=nullptr,JSValueRef Value=nullptr);
	
private:
	JSContextRef	mContext;
	JSObjectRef		mFunctionObject = nullptr;
};

class TPromise
{
public:
	TPromise(JSContextRef Context,JSValueRef Promise,JSValueRef ResolveFunc,JSValueRef RejectFunc);
	
	void			Resolve(JSValueRef Value)	{	mResolve.Call(nullptr,Value);	}
	void			Reject(JSValueRef Value)	{	mReject.Call(nullptr,Value);	}
	
	JSObjectRef		mPromise;
	TFunction		mResolve;
	TFunction		mReject;
};

JSObjectRef GetObject(JSContextRef Context,JSValueRef Value)
{
	if ( !JSValueIsObject( Context, Value ) )
		throw Soy::AssertException("Value is not object");
	return const_cast<JSObjectRef>( Value );
}

TPromise::TPromise(JSContextRef Context,JSValueRef Promise,JSValueRef ResolveFunc,JSValueRef RejectFunc) :
mPromise	( GetObject(Context,Promise) ),
mResolve	( Context, ResolveFunc ),
mReject		( Context, RejectFunc )
{
}


TFunction::TFunction(JSContextRef Context,JSValueRef Value) :
mContext	( Context )
{
	auto ValueType = JSValueGetType( Context, Value );
	if ( ValueType != kJSTypeObject )
		throw Soy::AssertException("Value for TFunciton is not an object");
	
	mFunctionObject = const_cast<JSObjectRef>( Value );
	
	if ( !JSObjectIsFunction(Context, mFunctionObject) )
		throw Soy::AssertException("Object should be function");
	/*
	 auto ScriptType = JSValueGetType( Context, MakePromiseFunctionValue );
	 if ( Exception!=nullptr )
	 std::Debug << "An exception" << JsCore::HandleToString( Context, Exception ) << std::endl;
	 */
}

JSValueRef TFunction::TFunction::Call(JSObjectRef This,JSValueRef Arg0)
{
	if ( This == nullptr )
		This = JSContextGetGlobalObject( mContext );
	
	JSValueRef Exception = nullptr;
	auto Result = JSObjectCallAsFunction( mContext, mFunctionObject, This, 1, &Arg0, &Exception );
	
	if ( Exception!=nullptr )
		throw Soy::AssertException( JsCore::HandleToString( mContext, Exception ) );
	
	return Result;
}


TPromise JSCreatePromise(JSContextRef Context)
{
	static std::shared_ptr<TFunction> MakePromiseFunction;
	
	if ( !MakePromiseFunction )
	{
		auto* MakePromiseFunctionSource =  R"V0G0N(
		
		let MakePromise = function()
		{
			var PromData = {};
			var prom = new Promise( function(Resolve,Reject) { PromData.Resolve = Resolve; PromData.Reject = Reject; } );
			PromData.Promise = prom;
			prom.Resolve = PromData.Resolve;
			prom.Reject = PromData.Reject;
			return prom;
		}
		MakePromise;
		//MakePromise();
		)V0G0N";
		
		JSStringRef MakePromiseFunctionSourceString = JSStringCreateWithUTF8CString(MakePromiseFunctionSource);
		JSValueRef Exception = nullptr;
		
		auto MakePromiseFunctionValue = JSEvaluateScript( Context, MakePromiseFunctionSourceString, nullptr, nullptr, 0, &Exception );
		if ( Exception!=nullptr )
			std::Debug << "An exception" << JsCore::HandleToString( Context, Exception ) << std::endl;
		
		MakePromiseFunction.reset( new TFunction( Context, MakePromiseFunctionValue ) );
	}
	
	auto This = JSContextGetGlobalObject( Context );
	auto NewPromiseHandle = MakePromiseFunction->Call();
	
	auto Type = JSValueGetType( Context, NewPromiseHandle );
	
	auto NewPromiseObject = const_cast<JSObjectRef>(NewPromiseHandle);
	JSValueRef Exception = nullptr;
	auto Resolve = const_cast<JSObjectRef>(JSObjectGetProperty( Context, NewPromiseObject, JSStringCreateWithUTF8CString("Resolve"), &Exception ) );
	auto Reject = const_cast<JSObjectRef>(JSObjectGetProperty( Context, NewPromiseObject, JSStringCreateWithUTF8CString("Reject"), &Exception ) );
	
	TPromise Promise( Context, NewPromiseObject, Resolve, Reject );
	
	return Promise;
}



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


int32_t	JsCore::HandleToInt(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	JSValueRef Exception = nullptr;
	auto DoubleJs = JSValueToNumber( Context, Handle, &Exception );

	auto Int = static_cast<int32_t>( DoubleJs );
	return Int;
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
		
		ApiPop::Bind( *mContext );
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
	JSClassRef Global = nullptr;
	
	auto Context = JSGlobalContextCreateInGroup( mContextGroup, Global );
	std::shared_ptr<JsCore::TContext> pContext( new TContext( *this, Context, mRootDirectory ) );
	//mContexts.PushBack( pContext );
	return pContext;
}





JsCore::TContext::TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory) :
	mInstance		( Instance ),
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
		TCallbackInfo CallbackInfo(mInstance);
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


int32_t JsCore::TCallbackInfo::GetArgumentInt(size_t Index) const
{
	auto Handle = mArguments[Index];
	auto Value = JsCore::HandleToInt( mContext, Handle );
	return Value;
}
