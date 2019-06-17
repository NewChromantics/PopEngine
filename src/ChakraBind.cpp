#include "ChakraBind.h"
#include "SoyDebug.h"
#include "SoyFileSystem.h"

#include "TBind.h"

#define THROW_TODO	throw Soy::AssertException( std::string("todo: ") + __PRETTY_FUNCTION__ )

namespace Chakra
{
	const char*	GetErrorString(JsErrorCode Error);
	JsSourceContext		GetNewScriptContext();
	
	std::atomic<ChakraCookie>	gScriptContextCounter(1000);
}

JsSourceContext Chakra::GetNewScriptContext()
{
	return gScriptContextCounter++;
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



void JSObjectSetPrivate(JSObjectRef Object,void* Data)
{
	auto Error = JsSetExternalData( Object.mValue, Data );
	Chakra::IsOkay( Error, "JsSetExternalData" );
}

void* JSObjectGetPrivate(JSObjectRef Object)
{
	void* Data = nullptr;
	auto Error = JsGetExternalData( Object.mValue, &Data );
	Chakra::IsOkay( Error, "JsGetExternalData" );
	return Data;
}

JSObjectRef	JSObjectMake(JSContextRef Context,JSClassRef Class,void* Data)
{
	//	dumb object
	if ( !Class )
	{
		if ( Data )
			throw Soy::AssertException("JSObjectMake without class, with data, excepting null data if no class");
		
		JSValueRef NewObject = nullptr;
		auto Error = JsCreateObject( &NewObject );
		Chakra::IsOkay( Error, "JsCreateObject" );
		if ( !NewObject )
			throw Soy::AssertException("JsCreateObject created null object");
		return NewObject;
	}

	//auto FreeFunc = Class.Finalise;
	JSValueRef NewObject = nullptr;
	JsFinalizeCallback FreeFunc = nullptr;
	auto Error = JsCreateExternalObject( Data, FreeFunc, &NewObject );
	Chakra::IsOkay( Error, "JsCreateExternalObject" );
	if ( !NewObject )
		throw Soy::AssertException("JsCreateExternalObject created null object");

	return NewObject;
}


JsPropertyIdRef GetProperty(JSStringRef Name)
{
	Array<char> NameString;
	Bind::GetString( nullptr, Name, GetArrayBridge(NameString) );
	
	//	property id's are context specific
	JsPropertyIdRef Property = nullptr;
	auto Error = JsCreatePropertyId( NameString.GetArray(), NameString.GetSize(), &Property );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return Property;
}

JSValueRef	JSObjectGetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef* Exception)
{
	JSValueRef Value = nullptr;
	auto Property = GetProperty(Name);
	auto Error = JsGetProperty( This.mValue, Property, &Value );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	if ( !Value )
		throw Soy::AssertException("JsGetProperty got null value");
	
	return Value;
}


void JSObjectSetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception)
{
	bool StrictRules = true;
	auto Property = GetProperty(Name);
	auto Error = JsSetProperty( This.mValue, Property, Value, StrictRules );
	Chakra::IsOkay( Error, "JsSetProperty" );
	
	//	test result
	{
		auto SameValue = JSObjectGetProperty( Context, This, Name, nullptr );
		auto SameType = JSValueGetType( SameValue );
		std::Debug << "Set type is " << SameType << std::endl;
	}
}

void		JSObjectSetPropertyAtIndex(JSContextRef Context,JSObjectRef This,size_t Index,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSType JSValueGetType(JSValueRef Value)
{
	if ( Value == nullptr )
		return kJSTypeUndefined;
	
	JsValueType Type = JsUndefined;
	auto Error = JsGetValueType( Value, &Type );
	Chakra::IsOkay( Error, "JsGetValueType" );
	
	switch ( Type )
	{
		//	not sure what to do with symbol... object or string?
		case JsSymbol:
			throw Soy::AssertException("todo: handle chakra JS type symbol. Is it a string or an object?");
		
		case JsUndefined:	return kJSTypeUndefined;
		case JsNull:		return kJSTypeNull;
		case JsNumber:		return kJSTypeNumber;
		case JsString:		return kJSTypeString;
		case JsBoolean:		return kJSTypeBoolean;
		
		case JsObject:
		//	gr: we treat arrays, functions as objects, then delve deeper, to match jscore
		case JsFunction:
		case JsError:
		case JsArray:
		case JsArrayBuffer:
		case JsTypedArray:
		case JsDataView:
			return kJSTypeObject;
		
		default:break;
	}
	
	std::stringstream ErrorStr;
	ErrorStr << "Unhandled Chakra JS type " << Type;
	throw Soy::AssertException(ErrorStr);
}


JSType JSValueGetType(JSContextRef Context,JSValueRef Value)
{
	return JSValueGetType( Value );
}


bool JSValueIsObject(JSContextRef Context,JSValueRef Value)
{
	auto Type = JSValueGetType( Value );
	return Type == kJSTypeObject;
}

bool JSValueIsObject(JSContextRef Context,JSObjectRef Value)
{
	auto Type = JSValueGetType( Value.mValue );
	return Type == kJSTypeObject;
}

JSObjectRef JSValueToObject(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	auto Type = JSValueGetType( Value );
	if ( Type != kJSTypeObject )
	{
		std::stringstream Error;
		Error << "JSValueToObject() value is not an object (is " << Type << ")";
		throw Soy::AssertException( Error );
	}
	return Value;
}

void		JSValueProtect(JSContextRef Context,JSValueRef Value)
{
	unsigned int NewCount = 0;
	auto Error = JsAddRef( Value, &NewCount );
	Chakra::IsOkay( Error, "JSValueProtect");
}

void		JSValueUnprotect(JSContextRef Context,JSValueRef Value)
{
	unsigned int NewCount = 0;
	auto Error = JsRelease( Value, &NewCount );
	Chakra::IsOkay( Error, "JSValueUnprotect");
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
	auto Type = JSValueGetType( Value );
	return Type == kJSTypeNumber;
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
	JsValueType Type = JsUndefined;
	auto Error = JsGetValueType( Value.mValue, &Type );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return (Type == JsFunction);
}

JSValueRef JSObjectCallAsFunction(JSContextRef Context,JSObjectRef Object,JSObjectRef This,size_t ArgumentCount,JSValueRef* Arguments,JSValueRef* Exception)
{
	THROW_TODO;
}

JSValueRef JSObjectMakeFunctionWithCallback(JSContextRef Context,JSStringRef Name,JSObjectCallAsFunctionCallback FunctionPtr)
{
	JSValueRef Function = nullptr;
	void* UserData = nullptr;
	auto Result = JsCreateFunction( FunctionPtr, UserData, &Function );
	Chakra::IsOkay( Result, __PRETTY_FUNCTION__ );
	return Function;
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
	JsValueType Type = JsUndefined;
	auto Error = JsGetValueType( Value, &Type );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return (Type == JsUndefined);
}


JSValueRef	JSValueMakeNull(JSContextRef Context)
{
	THROW_TODO;
}

bool		JSValueIsNull(JSContextRef Context,JSValueRef Value)
{
	JsValueType Type = JsUndefined;
	auto Error = JsGetValueType( Value, &Type );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return (Type == JsUndefined);
}


JSObjectRef	JSObjectMakeArray(JSContextRef Context,size_t ElementCount,const JSValueRef* Elements,JSValueRef* Exception)
{
	THROW_TODO;
}

bool JSValueIsArray(JSContextRef Context,JSValueRef Value)
{
	JsValueType Type = JsUndefined;
	auto Error = JsGetValueType( Value, &Type );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return (Type == JsArray);
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
	auto ParseAttributes = JsParseScriptAttributeNone;
	JsSourceContext ScriptCookie = Chakra::GetNewScriptContext();
	JsValueRef Result = nullptr;
	auto Error = JsRun( Source.mValue, ScriptCookie, Filename.mValue, ParseAttributes, &Result );
	Chakra::IsOkay( Error, "JSEvaluateScript/JsRun");
	return Result;
}

JSGlobalContextRef JSContextGetGlobalContext(JSContextRef Context)
{
	return Context;
}

JSObjectRef JSContextGetGlobalObject(JSContextRef Context)
{
	JSValueRef Object = nullptr;
	auto Error = JsGetGlobalObject( &Object );
	Chakra::IsOkay( Error, "JsCreateObject" );
	return Object;
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
	if ( GlobalClass )
		throw Soy::AssertException("Not currently supporting creating context with a global class");
	
	JsContextRef NewContext = nullptr;
	auto Error = JsCreateContext( ContextGroup.mVirtualMachine->mRuntime, &NewContext );
	Chakra::IsOkay( Error, "JsCreateContext" );
	return NewContext;
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
	JsValueRef String = nullptr;
	auto Length = strlen(Buffer);
	auto Error = JsCreateString( Buffer, Length, &String );
	Chakra::IsOkay( Error, std::string("JSStringCreateWithUTF8CString") + std::string(" with ") + Buffer );
	return String;
}

size_t JSStringGetUTF8CString(JSContextRef Context,JSStringRef String,char* Buffer,size_t BufferSize)
{
	size_t CopyLength = 0;
	auto Result = JsCopyString( String.mValue, Buffer, BufferSize, &CopyLength );
	Chakra::IsOkay( Result, "JsCopyString");
	return CopyLength;
}

size_t JSStringGetLength(JSStringRef String)
{
	int Length = 0;
	auto Error = JsGetStringLength( String.mValue, &Length );
	Chakra::IsOkay( Error, "JsGetStringLength" );

	if ( Length < 0 )
	{
		std::stringstream ErrorStr;
		ErrorStr << "JsGetStringLength gave negative length " << Length;
		throw Soy::AssertException( ErrorStr );
	}
	return Length;
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
	//	doesn't seem to be a function for this
}

JSValueRef JSObjectToValue(JSObjectRef Object)
{
	return Object.mValue;
}


JSClassRef JSClassCreate(JSContextRef Context,JSClassDefinition* Definition)
{
	JSClassRef Class(nullptr);
	
	//	JsCreateFunction
	
	return Class;
}

void		JSClassRetain(JSClassRef Class)
{
}



void JSLockAndRun(JSGlobalContextRef GlobalContext,std::function<void(JSContextRef&)> Functor)
{
	//	todo: lock
	//	todo: set exception capture
	try
	{
		auto Result = JsSetCurrentContext( GlobalContext );
		Chakra::IsOkay( Result, "JsSetCurrentContext" );
		Functor( GlobalContext );
		Result = JsSetCurrentContext(JS_INVALID_REFERENCE);
		Chakra::IsOkay( Result, "JsSetCurrentContext(Invalid)" );
	}
	catch(std::exception& e)
	{
		JsSetCurrentContext(JS_INVALID_REFERENCE);
		throw;
	}
}




JSValueRef JSValueMakeFromJSONString(JSContextRef Context, JSStringRef String)
{
	THROW_TODO;
}

void JSValueWrapper::Set(JSValueRef Value)
{
	Release();
	if ( !Value )
		return;
	
	mValue = Value;
	JSValueProtect(nullptr, mValue);
}

void JSValueWrapper::Release()
{
	if ( !mValue )
		return;
	JSValueUnprotect(nullptr, mValue);
	mValue = nullptr;
}
