#include "ChakraBind.h"
#include "SoyDebug.h"
#include "SoyFileSystem.h"

#include "TBind.h"

#define THROW_TODO	throw Soy::AssertException( std::string("todo: ") + __PRETTY_FUNCTION__ )

namespace Chakra
{
	const char*	GetErrorString(JsErrorCode Error);
}

const char* Chakra::GetErrorString(JsErrorCode Error)
{
	switch ( Error )
	{
#define CASE_ERROR(e)	case e:	return # e
		CASE_ERROR( JsNoError );
		CASE_ERROR( JsErrorCategoryUsage );
		CASE_ERROR( JsErrorInvalidArgument );
		CASE_ERROR( JsErrorNullArgument );
		CASE_ERROR( JsErrorNoCurrentContext );
		CASE_ERROR( JsErrorInExceptionState );
		CASE_ERROR( JsErrorNotImplemented );
		CASE_ERROR( JsErrorWrongThread );
		CASE_ERROR( JsErrorRuntimeInUse );
		CASE_ERROR( JsErrorBadSerializedScript );
		CASE_ERROR( JsErrorInDisabledState );
		CASE_ERROR( JsErrorCannotDisableExecution );
		CASE_ERROR( JsErrorHeapEnumInProgress );
		CASE_ERROR( JsErrorArgumentNotObject );
		CASE_ERROR( JsErrorInProfileCallback );
		CASE_ERROR( JsErrorInThreadServiceCallback );
		CASE_ERROR( JsErrorCannotSerializeDebugScript );
		CASE_ERROR( JsErrorAlreadyDebuggingContext );
		CASE_ERROR( JsErrorAlreadyProfilingContext );
		CASE_ERROR( JsErrorIdleNotEnabled );
		CASE_ERROR( JsCannotSetProjectionEnqueueCallback );
		CASE_ERROR( JsErrorCannotStartProjection );
		CASE_ERROR( JsErrorInObjectBeforeCollectCallback );
		CASE_ERROR( JsErrorObjectNotInspectable );
		CASE_ERROR( JsErrorPropertyNotSymbol );
		CASE_ERROR( JsErrorPropertyNotString );
		CASE_ERROR( JsErrorInvalidContext );
		CASE_ERROR( JsInvalidModuleHostInfoKind );
		CASE_ERROR( JsErrorModuleParsed );
		CASE_ERROR( JsNoWeakRefRequired );
		CASE_ERROR( JsErrorPromisePending );
		CASE_ERROR( JsErrorModuleNotEvaluated );
		//CASE_ERROR( JsErrorCategoryEngine );
		CASE_ERROR( JsErrorOutOfMemory );
		CASE_ERROR( JsErrorBadFPUState );
		//CASE_ERROR( JsErrorCategoryScript );
		CASE_ERROR( JsErrorScriptException );
		CASE_ERROR( JsErrorScriptCompile );
		CASE_ERROR( JsErrorScriptTerminated );
		CASE_ERROR( JsErrorScriptEvalDisabled );
		//CASE_ERROR( JsErrorCategoryFatal );
		CASE_ERROR( JsErrorFatal );
		CASE_ERROR( JsErrorWrongRuntime );
		//CASE_ERROR( JsErrorCategoryDiagError );
		CASE_ERROR( JsErrorDiagAlreadyInDebugMode );
		CASE_ERROR( JsErrorDiagNotInDebugMode );
		CASE_ERROR( JsErrorDiagNotAtBreak );
		CASE_ERROR( JsErrorDiagInvalidHandle );
		CASE_ERROR( JsErrorDiagObjectNotFound );
		CASE_ERROR( JsErrorDiagUnableToPerformAction );
#undef CASE_ERROR
	
		default:	return "Unhandled Chakra Error";
	}
}



void Chakra::IsOkay(JsErrorCode Error,const std::string& Context)
{
	if ( Error == JsNoError )
		return;
	
	std::stringstream ErrorStr;
	ErrorStr << "Chakra Error " << GetErrorString(Error) << " in " << Context;
	throw Soy::AssertException( ErrorStr );
}


const JSClassDefinition kJSClassDefinitionEmpty = {};



Chakra::TVirtualMachine::TVirtualMachine(const std::string& RuntimePath)
{
	JsRuntimeAttributes Attributes = JsRuntimeAttributeNone;
	JsThreadServiceCallback ThreadCallback = nullptr;
	auto Error = JsCreateRuntime( Attributes, ThreadCallback, &mRuntime );
	IsOkay( Error, "JsCreateRuntime" );
}

Chakra::TVirtualMachine::~TVirtualMachine()
{
	auto Error = JsDisposeRuntime( mRuntime );
	IsOkay( Error, "JsDisposeRuntime" );
}

JSContextGroupRef::JSContextGroupRef(std::nullptr_t)
{
	//	gr: don't throw. Just let this be in an invalid state for initialisation of variables
}

JSContextGroupRef::JSContextGroupRef(const std::string& RuntimePath)
{
	mVirtualMachine.reset( new Chakra::TVirtualMachine(RuntimePath));
}

JSContextGroupRef::operator bool() const
{
	return mVirtualMachine!=nullptr;
}




void JSObjectRef::operator=(std::nullptr_t Null)
{
	THROW_TODO;
}

void JSObjectRef::operator=(JSObjectRef That)
{
	THROW_TODO;
}

bool JSObjectRef::operator!=(std::nullptr_t Null) const
{
	THROW_TODO;
}

bool JSObjectRef::operator!=(const JSObjectRef& That) const
{
	THROW_TODO;
}

JSObjectRef::operator bool() const
{
	THROW_TODO;
}
	

void JSObjectSetPrivate(JSObjectRef Object,void* Data)
{
	THROW_TODO;
}

void* JSObjectGetPrivate(JSObjectRef Object)
{
	THROW_TODO;
}

JSObjectRef	JSObjectMake(JSContextRef Context,JSClassRef Class,void* Data)
{
	THROW_TODO;
}

JSValueRef	JSObjectGetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef* Exception)
{
	THROW_TODO;
}

void JSObjectSetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception )
{
	THROW_TODO;
}

void		JSObjectSetPropertyAtIndex(JSContextRef Context,JSObjectRef This,size_t Index,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSType JSValueGetType(JSValueRef Value)
{
	THROW_TODO;
}


JSType JSValueGetType(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}


bool JSValueIsObject(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

bool JSValueIsObject(JSContextRef Context,JSObjectRef Value)
{
	THROW_TODO;
}

JSObjectRef JSValueToObject(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

void		JSValueProtect(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

void		JSValueUnprotect(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}


JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef Context,JSObjectRef This)
{
	THROW_TODO;
}

size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef Keys)
{
	THROW_TODO;
}

JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef Keys,size_t Index)
{
	THROW_TODO;
}


bool JSValueIsNumber(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

double JSValueToNumber(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSValueRef JSValueMakeNumber(JSContextRef Context,int Value)
{
	THROW_TODO;
}


bool JSObjectIsFunction(JSContextRef Context,JSObjectRef Value)
{
	THROW_TODO;
}

JSValueRef JSObjectCallAsFunction(JSContextRef Context,JSObjectRef Object,JSObjectRef This,size_t ArgumentCount,JSValueRef* Arguments,JSValueRef* Exception)
{
	THROW_TODO;
}

JSValueRef JSObjectMakeFunctionWithCallback(JSContextRef Context,JSStringRef Name,JSObjectCallAsFunctionCallback FunctionPtr)
{
	THROW_TODO;
}


bool JSValueToBoolean(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

JSValueRef JSValueMakeBoolean(JSContextRef Context,bool Value)
{
	THROW_TODO;
}


JSValueRef	JSValueMakeUndefined(JSContextRef Context)
{
	THROW_TODO;
}

bool		JSValueIsUndefined(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}


JSValueRef	JSValueMakeNull(JSContextRef Context)
{
	THROW_TODO;
}

bool		JSValueIsNull(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}


JSObjectRef	JSObjectMakeArray(JSContextRef Context,size_t ElementCount,const JSValueRef* Elements,JSValueRef* Exception)
{
	THROW_TODO;
}

bool JSValueIsArray(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

JSTypedArrayType JSValueGetTypedArrayType(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSTypedArrayType JSValueGetTypedArrayType(JSContextRef Context,JSObjectRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSObjectRef	JSObjectMakeTypedArrayWithBytesWithCopy(JSContextRef Context,JSTypedArrayType ArrayType,const uint8_t* ExternalBuffer,size_t ExternalBufferSize,JSValueRef* Exception)
{
	THROW_TODO;
}

JSObjectRef	JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef Context, JSTypedArrayType ArrayType, void* ExternalBuffer, size_t ExternalBufferSize, JSTypedArrayBytesDeallocator Dealloc, void* DeallocContext, JSValueRef* Exception)
{
	throw Soy::AssertException("v8 cannot use JSObjectMakeTypedArrayWithBytesNoCopy as we dont do the dealloc");
}

void* JSObjectGetTypedArrayBytesPtr(JSContextRef Context,JSObjectRef ArrayObject,JSValueRef* Exception)
{
	THROW_TODO;
}

size_t JSObjectGetTypedArrayByteOffset(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	THROW_TODO;
}

size_t JSObjectGetTypedArrayLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	THROW_TODO;
}

size_t JSObjectGetTypedArrayByteLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	THROW_TODO;
}


JSValueRef JSEvaluateScript(JSContextRef Context,JSStringRef Source,JSObjectRef This,JSStringRef Filename,int LineNumber,JSValueRef* Exception)
{
	THROW_TODO;
}

JSGlobalContextRef JSContextGetGlobalContext(JSContextRef Context)
{
	THROW_TODO;
}

JSObjectRef JSContextGetGlobalObject(JSContextRef Context)
{
	THROW_TODO;
}

JSContextGroupRef JSContextGroupCreate()
{
	THROW_TODO;
}

JSContextGroupRef JSContextGroupCreateWithRuntime(const std::string& RuntimeDirectory)
{
	JSContextGroupRef NewVirtualMachine( RuntimeDirectory );
	return NewVirtualMachine;
}

void JSContextGroupRelease(JSContextGroupRef ContextGroup)
{
	//	try and release all members here and maybe check for dangling refcounts
}

JSGlobalContextRef JSGlobalContextCreateInGroup(JSContextGroupRef ContextGroup,JSClassRef GlobalClass)
{
	THROW_TODO;
}

void				JSGlobalContextSetName(JSGlobalContextRef Context,JSStringRef Name)
{
	THROW_TODO;
}

void				JSGlobalContextRelease(JSGlobalContextRef Context)
{
	THROW_TODO;
}

void JSGarbageCollect(JSContextRef Context)
{
	THROW_TODO;
}


JSStringRef	JSStringCreateWithUTF8CString(JSContextRef Context,const char* Buffer)
{
	THROW_TODO;
}

size_t JSStringGetUTF8CString(JSContextRef Context,JSStringRef String,char* Buffer,size_t BufferSize)
{
	THROW_TODO;
}

size_t JSStringGetLength(JSStringRef String)
{
	THROW_TODO;
}

JSStringRef	JSValueToStringCopy(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSValueRef JSValueMakeString(JSContextRef Context,JSStringRef String)
{
	THROW_TODO;
}

void JSStringRelease(JSStringRef String)
{
	THROW_TODO;
}

JSValueRef JSObjectToValue(JSObjectRef Object)
{
	THROW_TODO;
}


JSClassRef JSClassCreate(JSContextRef Context,JSClassDefinition* Definition)
{
	THROW_TODO;
}

void		JSClassRetain(JSClassRef Class)
{
	THROW_TODO;
}



//	major abstraction from V8 to JSCore
//	JSCore has no global->local (maybe it should execute a run-next-in-queue func)
void JSLockAndRun(JSGlobalContextRef GlobalContext,std::function<void(JSContextRef&)> Functor)
{
	THROW_TODO;
}




JSValueRef JSValueMakeFromJSONString(JSContextRef Context, JSStringRef String)
{
	THROW_TODO;
}

