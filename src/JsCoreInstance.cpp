#include "JsCoreInstance.h"
#include "SoyAssert.h"

JsCore::TInstance::TInstance(const std::string& RootDirectory,const std::string& BootupFilename) :
	mContextGroup	( JSContextGroupCreate() )
{
	if ( !mContextGroup )
		throw Soy::AssertException("JSContextGroupCreate failed");
}

JsCore::TInstance::~TInstance()
{
	JSContextGroupRelease(mContextGroup);
}

std::shared_ptr<JsCore::TContext> JsCore::TInstance::CreateContext()
{
	auto Context = JSGlobalContextCreateInGroup( mContextGroup, nullptr);
	std::shared_ptr<JsCore::TContext> pContext( new TContext(Context) );
	mContexts.PushBack( pContext );
	return pContext;
}


JsCore::TContext::TContext(JSGlobalContextRef Context) :
	mContext	( Context )
{
}

JsCore::TContext::~TContext()
{
	JSGlobalContextRelease( mContext );
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

