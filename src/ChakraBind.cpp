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

	void				SetVirtualMachine(JSGlobalContextRef Context,JSContextGroupRef ContextGroup);
	TVirtualMachine&	GetVirtualMachine(JSGlobalContextRef Context);
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

std::string JSGetStringNoThrow(JsValueRef Value,bool& IsError)
{
	JsValueType ValueType;
	auto Error = JsGetValueType( Value, &ValueType );
	if ( Error != JsNoError )
	{
		IsError = true;
		std::stringstream ErrorString;
		ErrorString << "<JsGetValueType " << Chakra::GetErrorString( Error ) << ">";
		return ErrorString.str();
	}
	
	//	gr: cannot convert object to string? (during exception)
	JsValueRef String = nullptr;
	Error = JsConvertValueToString( Value, &String );
	if ( Error != JsNoError )
	{
		IsError = true;
		std::stringstream ErrorString;
		ErrorString << "<JsConvertValueToString " << Chakra::GetErrorString( Error ) << ">";
		return ErrorString.str();
	}

	char Buffer[1000];
	size_t StringLength = 0;
	Error = JsCopyString( String, Buffer, sizeof(Buffer), &StringLength );
	if ( Error != JsNoError )
	{
		IsError = true;
		std::stringstream ErrorString;
		ErrorString << "<JsCopyString " << Chakra::GetErrorString( Error ) << ">";
		return ErrorString.str();
	}
	
	IsError = false;
	std::string StringString( Buffer, StringLength );
	return StringString;
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

std::string GetPropertyString(JSObjectRef Object,const char* PropertyName)
{
	JsContextRef Context = nullptr;
	auto PropertyNameString = Bind::GetString(Context,PropertyName);
	auto Property = GetProperty( PropertyNameString );
	auto PropertyValue = JSObjectGetProperty( Context, Object, PropertyNameString, nullptr );
	bool IsError = false;
	auto String = JSGetStringNoThrow( PropertyValue, IsError );
	return String;
}

std::string ExceptionToString(JsValueRef ExceptionValue)
{
	JSContextRef Context = nullptr;
	
	auto ExceptionObject = JSValueToObject( Context, ExceptionValue, nullptr );
	
	
	//	our only reference!
	//	https://chromium.googlesource.com/external/github.com/Microsoft/ChakraCore/+/refs/heads/master/bin/NativeTests/MemoryPolicyTest.cpp#126
	//	gr: no property named message!
	 //	gr: searching propertys shows
	///0=exception	1=source	2=line	3=column	4=length	5=url	6=undefined
	//auto Message = GetPropertyString( ExceptionObject, "message" );
	auto Message = GetPropertyString( ExceptionObject, "exception" );
	auto Url = GetPropertyString( ExceptionObject, "url" );
	auto Line = GetPropertyString( ExceptionObject, "line" );

	//	code that failed
	auto Source = GetPropertyString( ExceptionObject, "source" );

	//	gr: is array length?
	//	length = 0....
	auto Length = GetPropertyString( ExceptionObject, "length" );
	
	std::stringstream ExceptionString;
	ExceptionString << "> " << Source << std::endl;
	ExceptionString << Url << ":" << Line << ": " << Message;
	return ExceptionString.str();
	
		/*
	JsValueRef PropertyNamesArray = nullptr;
	auto Error = JsGetOwnPropertyNames( ExceptionValue, &PropertyNamesArray );
	Chakra::IsOkay( Error, "JsGetOwnPropertyNames" );
	
	//	argh: can't see how to get array length

	int Index = 0;
	for ( int Index=0;	Index<99999;	Index++ )
	{
		JSValueRef IndexValue = nullptr;
		auto Error = JsIntToNumber( Index, &IndexValue );
		JSValueRef NameValue = nullptr;
		Error = JsGetIndexedProperty( PropertyNamesArray, IndexValue, &NameValue );
		
		bool HasError = true;
		auto NameString = JSGetStringNoThrow( NameValue, HasError );
		if ( HasError )
			break;
		std::Debug << Index << "=" << NameString << std::endl;
	}
	
	return "hello";
*/
}

__thread bool IsThrowing = false;

void Chakra::IsOkay(JsErrorCode Error,const std::string& Context)
{
	if ( Error == JsNoError )
		return;
	
	if ( IsThrowing )
		return;
	
	IsThrowing = true;
	
	//	grab exception
	JsValueRef ExceptionMeta = nullptr;
	std::stringstream ExceptionString;
	bool HasException = false;
	JsHasException(&HasException);
	
	if ( HasException )
	{
		auto GetExceptionError = JsGetAndClearExceptionWithMetadata( &ExceptionMeta );
		try
		{
			std::stringstream ExceptionException;
			
			if ( GetExceptionError != JsNoError )
			{
				ExceptionException << "JsGetAndClearExceptionWithMetadata error " << GetExceptionError;
				throw Soy::AssertException(ExceptionException);
			}
			
			auto ExceptionAsString = ExceptionToString( ExceptionMeta );
			ExceptionString << ExceptionAsString;
			
			/*	gr: this wasn't working as object didn't want to convert to a string
			bool IsError = false;
			ExceptionException << JSGetStringNoThrow( ExceptionMeta, IsError );
			if ( IsError )
				throw Soy::AssertException(ExceptionException);

			ExceptionString << ExceptionException.str();
			*/
		}
		catch(std::exception& e)
		{
			ExceptionString << "<Error getting exception: " << e.what() << ">";
		}
	}
	
	std::stringstream ErrorStr;
	ErrorStr << "Chakra Error " << GetErrorString(Error) << " in " << Context;
	if ( HasException )
		ErrorStr << "; Exception: " << ExceptionString.str();
	else
		ErrorStr << "(No exception)";
	
	IsThrowing = false;
	
	throw Soy::AssertException( ErrorStr );
}


const JSClassDefinition kJSClassDefinitionEmpty = {};



void Chakra::SetVirtualMachine(JSGlobalContextRef Context,JSContextGroupRef ContextGroup)
{
	auto* Vm = ContextGroup.mVirtualMachine.get();
	auto Error = JsSetContextData( Context, Vm );
	IsOkay( Error, "SetVirtualMachine/JsSetContextData");
}

Chakra::TVirtualMachine& Chakra::GetVirtualMachine(JSGlobalContextRef Context)
{
	void* Vm = nullptr;
	auto Error = JsGetContextData( Context, &Vm );
	IsOkay( Error, "GetVirtualMachine/JsGetContextData");

	if ( !Vm )
		throw Soy::AssertException("User data on context is null");
	auto* RealVm = static_cast<Chakra::TVirtualMachine*>(Vm);
	return *RealVm;
}


Chakra::TVirtualMachine::TVirtualMachine(const std::string& RuntimePath)
{
	JsRuntimeAttributes Attributes = JsRuntimeAttributeNone;
	JsThreadServiceCallback ThreadCallback = nullptr;
	auto Error = JsCreateRuntime( Attributes, ThreadCallback, &mRuntime );
	IsOkay( Error, "JsCreateRuntime" );
	
	bool IsRuntimeExecutionDisabled = false;
	JsIsRuntimeExecutionDisabled( mRuntime, &IsRuntimeExecutionDisabled );
	if ( IsRuntimeExecutionDisabled )
		throw Soy::AssertException("Expecting runtime enabled");
}

Chakra::TVirtualMachine::~TVirtualMachine()
{
	auto Error = JsDisposeRuntime( mRuntime );
	IsOkay( Error, "JsDisposeRuntime" );
}

//	lock & run & unlock
void Chakra::TVirtualMachine::Execute(JSGlobalContextRef Context,std::function<void(JSContextRef&)>& Execute)
{
	if ( !Context )
		throw Soy::AssertException("Trying to execte on null context");
	
	//	default sets new context and unlocks the lock
	std::function<void()> Lock = [&]
	{
		mCurrentContext = Context;
		auto Result = JsSetCurrentContext( mCurrentContext );
		Chakra::IsOkay( Result, "JsSetCurrentContext" );
	};
	
	std::function<void()> Unlock = [&]
	{
		auto Result = JsSetCurrentContext( nullptr );
		Chakra::IsOkay( Result, "JsSetCurrentContext (unset)" );
		
		mCurrentContextLock.unlock();
		mCurrentContext = nullptr;
	};

	//	get lock
	if ( !mCurrentContextLock.try_lock() )
	{
		//	failed, but if we're trying to re-lock same context, don't do anything
		if ( mCurrentContext == Context )
		{
			Lock = []{};
			Unlock = []{};
		}
		else
		{
			//	wait to lock to new context
			mCurrentContextLock.lock();
		}
	}
	
	//	lock, run, unlock
	try
	{
		Lock();
		Execute( mCurrentContext );
		Unlock();
	}
	catch(std::exception& e)
	{
		Unlock();
		throw;
	}
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

	//	gr: for chakra, when we want the prototype object (class but no data)
	//		it's excepting the constructor
	if ( !Data )
	{
		return Class.mConstructor;
	}
	
	//auto FreeFunc = Class.Finalise;
	JSValueRef NewObject = nullptr;
	JsFinalizeCallback FreeFunc = Class.mDestructor;
	auto Error = JsCreateExternalObject( Data, FreeFunc, &NewObject );
	Chakra::IsOkay( Error, "JsCreateExternalObject" );
	if ( !NewObject )
		throw Soy::AssertException("JsCreateExternalObject created null object");

	return NewObject;
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
}

void JSObjectSetPropertyAtIndex(JSContextRef Context,JSObjectRef This,size_t Index,JSValueRef Value,JSValueRef* Exception)
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
	if ( !JSObjectIsFunction( Context, Object ) )
		throw Soy::AssertException("Trying to call non-function");
	
	//	cannot provide null this
	if ( !This )
		This = JSContextGetGlobalObject(Context);
	
	//	there must ALWAYS be arguments, [0] is this
	BufferArray<JSValueRef,20> ArgumentsArray;
	ArgumentsArray.PushBack( This.mValue );
	for ( auto a=0;	a<ArgumentCount;	a++ )
		ArgumentsArray.PushBack( Arguments[a] );
	
	JsValueRef Result = nullptr;
	auto Error = JsCallFunction( Object.mValue, ArgumentsArray.GetArray(), ArgumentsArray.GetSize(), &Result );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return Result;
}

JSValueRef JSObjectMakeFunctionWithCallback(JSContextRef Context,JSStringRef Name,JSObjectCallAsFunctionCallback FunctionPtr)
{
	JSValueRef Function = nullptr;
	//	this is user data, we dont get a context in the callback, so we send it ourselves
	void* CallbackState = Context;

	//	named just makes it easier to debug
	//auto Result = JsCreateFunction( FunctionPtr, CallbackState, &Function );
	auto Result = JsCreateNamedFunction( Name.mValue, FunctionPtr, CallbackState, &Function );
	Chakra::IsOkay( Result, __PRETTY_FUNCTION__ );
	return Function;
}


bool JSValueToBoolean(JSContextRef Context,JSValueRef ThatValue)
{
	JSValueRef Value = nullptr;
	auto Error = JsConvertValueToBoolean(ThatValue,&Value);
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return Value;
}

JSValueRef JSValueMakeBoolean(JSContextRef Context,bool Boolean)
{
	JSValueRef Value = nullptr;
	auto Error = JsBoolToBoolean(Boolean,&Value);
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return Value;
}


JSValueRef	JSValueMakeUndefined(JSContextRef Context)
{
	JSValueRef Value = nullptr;
	auto Error = JsGetUndefinedValue(&Value);
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return Value;
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
	JSValueRef Value = nullptr;
	auto Error = JsGetNullValue(&Value);
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return Value;
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

class TTypedArrayMeta
{
public:
	uint32_t			ByteOffset = 0;
	uint32_t			ByteLength = 0;
	JsValueRef			ArrayBuffer = nullptr;
	JsTypedArrayType	ChakraType = JsArrayTypeFloat64;	//	obscure case for initialisation
	JSTypedArrayType	BindType = kJSTypedArrayTypeNone;
};

TTypedArrayMeta GetTypedArrayMeta(JSValueRef Array)
{
	TTypedArrayMeta Meta;
	auto Error = JsGetTypedArrayInfo( Array, &Meta.ChakraType, &Meta.ArrayBuffer, &Meta.ByteOffset, &Meta.ByteLength );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );

	//	chakra has no unhandled types
	Meta.BindType = static_cast<JSTypedArrayType>( Meta.ChakraType );
	
	//	do other verification
	
	return Meta;
}

JSTypedArrayType JSValueGetTypedArrayType(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	auto Meta = GetTypedArrayMeta( Value );
	return Meta.BindType;
}

JSTypedArrayType JSValueGetTypedArrayType(JSContextRef Context,JSObjectRef Value,JSValueRef* Exception)
{
	//	caller doesn't call any "is typed array" first, so we check here, but throw in the meta code
	JsValueType ValueType = JsUndefined;
	auto Error = JsGetValueType( Value.mValue, &ValueType );
	Chakra::IsOkay( Error, "JsGetValueType" );
	if ( ValueType != JsTypedArray )
		return kJSTypedArrayTypeNone;

	auto Meta = GetTypedArrayMeta( Value.mValue );
	return Meta.BindType;
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
	auto Meta = GetTypedArrayMeta( Array.mValue );
	return Meta.ByteOffset;
}

size_t JSObjectGetTypedArrayLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	//	todo: convert byte length to typed length!
	THROW_TODO;
}

size_t JSObjectGetTypedArrayByteLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	auto Meta = GetTypedArrayMeta( Array.mValue );
	return Meta.ByteLength;
}


JSValueRef JSEvaluateScript(JSContextRef Context,JSStringRef Source,JSObjectRef This,JSStringRef Filename,int LineNumber,JSValueRef* Exception)
{
	if ( !This )
		This = JSContextGetGlobalObject(Context);
	
	auto ParseAttributes = JsParseScriptAttributeNone;
	JsSourceContext ScriptCookie = Chakra::GetNewScriptContext();
	JsValueRef Result = nullptr;
/*
	//	parse it and turn into a script
	auto Error = JsParse( Source.mValue, ScriptCookie, Filename.mValue, ParseAttributes, &Result );
	Chakra::IsOkay( Error, "JsParse");
	JsValueType ResultType;
	JsGetValueType( Result, &ResultType );
	
	//	call the result
	//	gr: fatal when using global
	JsValueRef Arguments[1] = {This.mValue};
	Error = JsCallFunction( Result, Arguments, 1, &Result );
	Chakra::IsOkay( Error, "Calling parsed script");
	*/
	//	fatal
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
	
	Chakra::SetVirtualMachine( NewContext, ContextGroup );
	
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
	//	this function gets a value in a string representation
	JsValueRef String = nullptr;
	auto Error = JsConvertValueToString( Value, &String );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	return String;
}

JSValueRef JSValueMakeString(JSContextRef Context,JSStringRef String)
{
	//	should this be a copy?
	return String.mValue;
}

void JSStringRelease(JSStringRef String)
{
	//	doesn't seem to be a function for this
}

JSValueRef JSObjectToValue(JSObjectRef Object)
{
	return Object.mValue;
}


JSClassRef JSClassCreate(JSContextRef Context,JSClassDefinition& Definition)
{
	JSClassRef Class(nullptr);
	
	//	gr: doesn't seem to be a specific constructor function creator
	std::string FunctionName = std::string( Definition.className )+" constructor";
	auto FunctionNameValue = Bind::GetString( Context, FunctionName );
	Class.mConstructor = JSObjectMakeFunctionWithCallback( Context, FunctionNameValue, Definition.callAsConstructor );
	Class.mDestructor = Definition.finalize;
	
	return Class;
}

void		JSClassRetain(JSClassRef Class)
{
}



void JSLockAndRun(JSGlobalContextRef GlobalContext,std::function<void(JSContextRef&)> Functor)
{
	//	run via vm which handles locking
	auto& vm = Chakra::GetVirtualMachine(GlobalContext);
	vm.Execute( GlobalContext, Functor );
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
