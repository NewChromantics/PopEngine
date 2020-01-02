//	gr: can't exclude file per-configuration in xcode grr
#if defined(JSAPI_CHAKRA)
#include "ChakraBind.h"
#include "SoyDebug.h"
#include "SoyFileSystem.h"
#include "TBind.h"
#include <atomic>
#include <string_view>
using namespace std::literals;


#define THROW_TODO	throw Soy::AssertException( std::string("todo: ") + __PRETTY_FUNCTION__ )

namespace Chakra
{
	const char*	GetErrorString(JsErrorCode Error);

	JsSourceContext		GetNewScriptContext();
	std::atomic<JsSourceContext>	gScriptContextCounter(1000);

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
#if defined(TARGET_OSX)
		CASE_ERROR( JsErrorInvalidContext );
		CASE_ERROR( JsInvalidModuleHostInfoKind );
		CASE_ERROR( JsErrorModuleParsed );
		CASE_ERROR( JsNoWeakRefRequired );
		CASE_ERROR( JsErrorPromisePending );
		CASE_ERROR( JsErrorModuleNotEvaluated );
		//CASE_ERROR( JsErrorCategoryEngine );
#endif
		CASE_ERROR( JsErrorOutOfMemory );
#if defined(TARGET_OSX)
		CASE_ERROR( JsErrorBadFPUState );
#endif
		//CASE_ERROR( JsErrorCategoryScript );
		CASE_ERROR( JsErrorScriptException );
		CASE_ERROR( JsErrorScriptCompile );
		CASE_ERROR( JsErrorScriptTerminated );
		CASE_ERROR( JsErrorScriptEvalDisabled );
		//CASE_ERROR( JsErrorCategoryFatal );
		CASE_ERROR( JsErrorFatal );
		CASE_ERROR( JsErrorWrongRuntime );
		//CASE_ERROR( JsErrorCategoryDiagError );

#if defined(TARGET_OSX)
		CASE_ERROR( JsErrorDiagAlreadyInDebugMode );
		CASE_ERROR( JsErrorDiagNotInDebugMode );
		CASE_ERROR( JsErrorDiagNotAtBreak );
		CASE_ERROR( JsErrorDiagInvalidHandle );
		CASE_ERROR( JsErrorDiagObjectNotFound );
		CASE_ERROR( JsErrorDiagUnableToPerformAction );
#endif
#undef CASE_ERROR
	
		default:	return "Unhandled Chakra Error";
	}
}


#if defined(TARGET_WINDOWS)
//	missing API wrapper
JsErrorCode JsCreateString(const char* Buffer, size_t Length, JsValueRef* Result)
{
	auto StringW = Soy::StringToWString(Buffer);

	return JsPointerToString(StringW.c_str(), Length, Result);
}
#endif

#if defined(TARGET_WINDOWS)
//	missing API wrapper
JsErrorCode JsCopyString(JsValueRef String, char* Buffer, size_t BufferSize, size_t* Length)
{
	*Length = 0;
	const wchar_t* BufferW = nullptr;
	auto Error = JsStringToPointer(String, &BufferW, Length);
	std::wstring StringW(BufferW, *Length);
	auto StringC = Soy::WStringToString(StringW);
	Soy::StringToBuffer(StringC, Buffer, BufferSize);
	return Error;
}
#endif


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

	size_t StringLength = 0;

	char Buffer[1000];
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


#if defined(TARGET_WINDOWS)
//	API difference
void JsCreatePropertyId(JSContextRef Context, const std::string& Name, JsPropertyIdRef* Property)
{
	if (Context)
	{
		auto& vm = Chakra::GetVirtualMachine(Context);
		*Property = vm.GetCachedProperty(Name);
		return;
	}

	auto NameW = Soy::StringToWString(Name);
	JsGetPropertyIdFromName(NameW.c_str(), Property);
}
#endif

/*
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
*/

JsPropertyIdRef GetProperty(JSContextRef Context, const std::string& Name)
{
	//	property id's are context specific
	JsPropertyIdRef Property = nullptr;
	JsCreatePropertyId(Context,Name, &Property );
	return Property;
}

JsPropertyIdRef GetProperty(JSContextRef Context,JSStringRef Name)
{
	//	well, this is annoying, gotta go back to string and back again

	//	get a pointer, this exists for the lifetime of the JsValue, 
	//	so just use it quickly here
	const wchar_t* NameString = nullptr;
	size_t NameStringLength = 0;
	auto Error = JsStringToPointer(Name.mValue, &NameString, &NameStringLength);
	Chakra::IsOkay(Error, "JsStringToPointer");

	JsPropertyIdRef Property = nullptr;
	Error = JsGetPropertyIdFromName(NameString, &Property);
	Chakra::IsOkay(Error, "JsGetPropertyIdFromName");

	return Property;
}

std::string GetPropertyString(JSContextRef Context,JSObjectRef Object,const std::string& PropertyName)
{
	auto Property = GetProperty(Context,PropertyName);
	auto PropertyValue = JSObjectGetProperty( Context, Object, PropertyName, nullptr );
	bool IsError = false;
	auto String = JSGetStringNoThrow( PropertyValue, IsError );
	return String;
}


std::string GetPropertyString(JSObjectRef Object, const std::string& PropertyName)
{
	JSContextRef Context = nullptr;
	return GetPropertyString(Context, Object, PropertyName);
}


std::string GetPropertyString(JSObjectRef Object,JSStringRef PropertyName)
{
	THROW_TODO;
}

void DebugPropertyName(JsValueRef ExceptionValue)
{
	JsValueRef PropertyNamesArray = nullptr;
	auto Error = JsGetOwnPropertyNames(ExceptionValue, &PropertyNamesArray);
	Chakra::IsOkay(Error, "JsGetOwnPropertyNames");

	//	argh: can't see how to get array length

	int Index = 0;
	for (int Index = 0; Index < 100; Index++)
	{
		JSValueRef IndexValue = nullptr;
		auto Error = JsIntToNumber(Index, &IndexValue);
		JSValueRef NameValue = nullptr;
		Error = JsGetIndexedProperty(PropertyNamesArray, IndexValue, &NameValue);

		bool HasError = true;
		auto NameString = JSGetStringNoThrow(NameValue, HasError);
		if (HasError)
			break;
		std::Debug << Index << "=" << NameString << std::endl;
	}

}

std::string ExceptionToString(JsValueRef ExceptionValue)
{
	JSContextRef Context = nullptr;
	
	if (JSValueGetType(ExceptionValue) == kJSTypeString)
	{
		auto ExceptionString = Bind::GetString(Context, ExceptionValue);
		return ExceptionString;
	}

	auto ExceptionObject = JSValueToObject( Context, ExceptionValue, nullptr );
	

	DebugPropertyName(ExceptionValue);
	
	//	our only reference!
	//	https://chromium.googlesource.com/external/github.com/Microsoft/ChakraCore/+/refs/heads/master/bin/NativeTests/MemoryPolicyTest.cpp#126
	//	gr: no property named message!
	 //	gr: searching propertys shows
	///0=exception	1=source	2=line	3=column	4=length	5=url	6=undefined
	//	gr: ^^^ but on windows[sdk], they're different :)
#if defined(TARGET_WINDOWS)
	auto MessageKey = "message";
	//auto LineKey = "number";
	auto LineKey = "line";
	auto FilenameKey = "stack";
	//auto SourceKey = "description";
	auto SourceKey = "source";
#else
	auto MessageKey = "exception";
	auto LineKey = "line";
	auto FilenameKey = "url";
	auto SourceKey = "source";
#endif

	//auto Message = GetPropertyString( ExceptionObject, "message" );
	auto Message = GetPropertyString( ExceptionObject, MessageKey);
	auto Url = GetPropertyString( ExceptionObject, FilenameKey );
	auto Line = GetPropertyString( ExceptionObject, LineKey);

	//	code that failed
	auto Source = GetPropertyString( ExceptionObject, SourceKey);

	//	gr: is array length?
	//	length = 0....
	//	gr: no length in winsdk
	auto Length = GetPropertyString( ExceptionObject, "length"s );
	
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


#if defined(TARGET_WINDOWS)
JsErrorCode JsGetAndClearExceptionWithMetadata(JsValueRef *exception)
{
	return JsGetAndClearException(exception);
}
#endif


__thread bool IsThrowing = false;

void Chakra::IsOkay(JsErrorCode Error, const char* Context)
{
	if (Error == JsNoError)
		return;

	IsOkay(Error, std::string(Context));
}

void Chakra::IsOkay(JsErrorCode Error, const std::string& Context)
{
	if (Error == JsNoError)
		return;

	IsOkay(Error, std::string_view(Context));
}

void Chakra::IsOkay(JsErrorCode Error,const std::string_view& Context)
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
	//	if windows 10 is lower than a certain version, the edge chakra doesn't support async/ES8
	//	we should abort? or warn?
#if defined(TARGET_WINDOWS)
	auto WinVersion = Platform::GetOsVersion();
	//if (WinVersion < Soy::TVersion(10, 0, 17134))	//	this version definitely okay
	if (WinVersion <= Soy::TVersion(10, 0, 14393))	//	this version definitely too old
	{
		throw Soy::AssertException("Windows version too old for ES8 features");
	}
#endif

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


JsPropertyIdRef Chakra::TVirtualMachine::GetCachedProperty(const std::string& Name)
{
	auto Entry = mCachedPropertys.find(Name);
	if (Entry != mCachedPropertys.end())
	{
		return Entry->second;
	}

	JsPropertyIdRef Property = nullptr;
	JsCreatePropertyId(nullptr, Name, &Property);
	
	//	store string, and protect it to stop it getting garbage collected
	mCachedPropertys[Name] = Property;
	JSValueProtect(nullptr, Property);
	return Property;
}

JSValueRef Chakra::TVirtualMachine::GetCachedString(const std::string& Buffer)
{
	auto Entry = mCachedStrings.find(Buffer);
	if (Entry != mCachedStrings.end())
	{
		return Entry->second;
	}

	JsValueRef String = nullptr;
	auto Error = JsCreateString(Buffer.c_str(), Buffer.length(), &String);
	Chakra::IsOkay(Error, std::string("JSStringCreateWithUTF8CString") + std::string(" with ") + Buffer);

	//	store string, and protect it to stop it getting garbage collected
	mCachedStrings[Buffer] = String;
	JSValueProtect(nullptr,String);
	return String;
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



void JSObjectSetPrivate(JSContextRef Context,JSObjectRef Object,void* Data)
{
	auto Error = JsSetExternalData( Object.mValue, Data );
	Chakra::IsOkay( Error, "JsSetExternalData" );
}

void* JSObjectGetPrivate(JSContextRef Context,JSObjectRef Object)
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

	Error = JsSetPrototype( NewObject, Class.mPrototype );
	Chakra::IsOkay( Error, "JsSetPrototype" );

	return NewObject;
}



JSValueRef JSObjectGetProperty(JSContextRef Context, JSObjectRef This,JSStringRef Name, JSValueRef* Exception)
{
	JSValueRef Value = nullptr;
	auto Property = GetProperty(Context, Name);
	auto Error = JsGetProperty(This.mValue, Property, &Value);
	Chakra::IsOkay(Error, "JsGetProperty");
	if (!Value)
		throw Soy::AssertException("JsGetProperty got null value");

	return Value;
}

JSValueRef JSObjectGetProperty(JSContextRef Context,JSObjectRef This,const std::string& Name,JSValueRef* Exception)
{
	JSValueRef Value = nullptr;
	auto Property = GetProperty( Context, Name );
	auto Error = JsGetProperty( This.mValue, Property, &Value );
	Chakra::IsOkay( Error, __PRETTY_FUNCTION__ );
	if ( !Value )
		throw Soy::AssertException("JsGetProperty got null value");
	
	return Value;
}


void JSObjectSetProperty(JSContextRef Context,JSObjectRef This,const std::string& Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception)
{
	bool StrictRules = true;
	auto Property = GetProperty(Context,Name);
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

void JSValueProtect(JSContextRef Context, JSObjectRef Value)
{
	JSValueProtect(Context, Value.mValue);
}

void JSValueUnprotect(JSContextRef Context, JSObjectRef Value)
{
	JSValueUnprotect(Context, Value.mValue);
}

void JSValueProtect(JSContextRef Context,JSValueRef Value)
{
	unsigned int NewCount = 0;
	auto Error = JsAddRef( Value, &NewCount );
	Chakra::IsOkay( Error, "JSValueProtect");
}

void JSValueUnprotect(JSContextRef Context,JSValueRef Value)
{
	unsigned int NewCount = 0;
	auto Error = JsRelease( Value, &NewCount );
	Chakra::IsOkay( Error, "JSValueUnprotect");
}


JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef Context,JSObjectRef This)
{
	JsValueRef PropertyNamesArray = nullptr;
	auto Error = JsGetOwnPropertyNames(This.mValue, &PropertyNamesArray);
	Chakra::IsOkay(Error, "JsGetOwnPropertyNames");
	return PropertyNamesArray;
}

size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef Keys)
{
	//	gr: iirc "length" doesn't appear in OSX build of chakra
	//		this gives 13 results for a x12 array. 0...12 and then length
	//	in chakra, all the indexes are propertyid's, we can probbaly cache
	//	integer propertyids
	{
		JsPropertyIdRef Property = nullptr;
		auto Error = JsGetPropertyIdFromName(L"length", &Property);
		Chakra::IsOkay(Error, "JsGetPropertyIdFromName(length)");

		JSContextRef Context = nullptr;
		JsValueRef LengthValue = nullptr;
		Error = JsGetProperty(Keys.mValue, Property, &LengthValue);
		Chakra::IsOkay(Error, "JsGetProperty (length)");

		int Length = -1;
		Error = JsNumberToInt(LengthValue, &Length);
		Chakra::IsOkay(Error, "JsNumberToInt (length)");
		if (Length < 1)
			throw Soy::AssertException("Array length negative (or less than 1, should include length)");

		//	number of elements includes length, argh
		Length--;
		return Length;
	}

	const int SafeLoop = 9999;
	//	count indexes 
	for (auto i = 0; i < SafeLoop; i++)
	{
		JsValueRef iValue = nullptr;
		auto Error = JsIntToNumber(i, &iValue);
		Chakra::IsOkay(Error, "JsIntToNumber");
		
		bool HasIndex = false;
		Error = JsHasIndexedProperty(Keys.mValue, iValue, &HasIndex);
		Chakra::IsOkay(Error, "JsHasIndexedProperty");
		
		if (!HasIndex)
			return i;	//	count
	}

	throw Soy::AssertException("Iterating over properties went over safe loop");
}

JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef Keys,size_t Index)
{
	//	this is returning a string, but really its a key
	//	there's a slight mis-match here, JSCore everything is a string, but in others
	//	keys are seperate properties
	JsValueRef IndexValue = nullptr;
	auto Error = JsIntToNumber(Index, &IndexValue);
	Chakra::IsOkay(Error, "JsIntToNumber");
	
	JSValueRef Element = nullptr;
	Error = JsGetIndexedProperty(Keys.mValue, IndexValue, &Element);
	Chakra::IsOkay(Error, "JsGetIndexedProperty");

	return Element;
}


bool JSValueIsNumber(JSContextRef Context,JSValueRef Value)
{
	auto Type = JSValueGetType( Value );
	return Type == kJSTypeNumber;
}

double JSValueToNumber(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	JsValueRef NumberValue = nullptr;
	auto Error = JsConvertValueToNumber( Value, &NumberValue );
	Chakra::IsOkay( Error, "JsConvertValueToNumber" );
	double Double = 0;
	Error = JsNumberToDouble( NumberValue, &Double );
	Chakra::IsOkay( Error, "JsNumberToDouble" );
	return Double;
}

JSValueRef JSValueMakeNumber(JSContextRef Context, double IntValue)
{
	JsValueRef Value = nullptr;
	auto Error = JsDoubleToNumber(IntValue, &Value);
	Chakra::IsOkay(Error, "JsDoubleToNumber");
	return Value;
}


JSValueRef JSValueMakeNumber(JSContextRef Context, int IntValue)
{
	JsValueRef Value = nullptr;
	auto Error = JsIntToNumber(IntValue, &Value);
	Chakra::IsOkay(Error, "JsIntToNumber");
	return Value;
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


JSValueRef JSObjectCallAsFunction(JSContextRef Context, JSValueRef FunctorValue)
{
	auto FunctorObject = JSValueToObject(Context, FunctorValue, nullptr);
	
	JSObjectRef This = nullptr;
	size_t ArgumentCount = 0;
	JSValueRef* Arguments = nullptr;
	JSValueRef* Exception = nullptr;
	return JSObjectCallAsFunction(Context, FunctorObject, This, ArgumentCount, Arguments, Exception);
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
	//	convert to bool, then read that bool
	//	gr: is it any more effecient to do a type check first?
	JSValueRef ValueAsBoolean = nullptr;
	auto Error = JsConvertValueToBoolean(ThatValue, &ValueAsBoolean);
	Chakra::IsOkay(Error, "JsConvertValueToBoolean");

	bool ValueBool = false;
	Error = JsBooleanToBool(ValueAsBoolean, &ValueBool);
	Chakra::IsOkay(Error, "JsBooleanToBool");
	
	return ValueBool;
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
	JsValueRef Array = nullptr;
	auto Error = JsCreateArray(ElementCount, &Array);
	Chakra::IsOkay(Error, __PRETTY_FUNCTION__);

	for (auto i = 0; i < ElementCount; i++)
	{
		auto Index = JSValueMakeNumber(Context, i);
		auto Element = Elements ? Elements[i] : nullptr;
		Error = JsSetIndexedProperty(Array, Index, Element);
		Chakra::IsOkay(Error, "JSObjectMakeArray set index");
	}
	return Array;
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


class TTypedArrayBufferMeta
{
public:
	uint8_t*			Bytes = nullptr;
	int					ElementSize = 0;
	unsigned int		ByteLength = 0;
	JsTypedArrayType	ChakraType = JsArrayTypeFloat64;	//	obscure case for initialisation
	JSTypedArrayType	BindType = kJSTypedArrayTypeNone;
};


TTypedArrayBufferMeta GetTypedArrayBufferMeta(JSValueRef Array)
{
	TTypedArrayBufferMeta Meta;

	//	bytes are valid for as long as ArrayObject, so this is unsafe atm
	auto Error = JsGetTypedArrayStorage(Array, &Meta.Bytes, &Meta.ByteLength, &Meta.ChakraType, &Meta.ElementSize );
	Chakra::IsOkay(Error, "JsGetTypedArrayStorage");

	Meta.BindType = static_cast<JSTypedArrayType>(Meta.ChakraType);

	return Meta;
}

TTypedArrayMeta GetTypedArrayMeta(JSValueRef Array)
{
	TTypedArrayMeta Meta;
	auto Error = JsGetTypedArrayInfo( Array, &Meta.ChakraType, &Meta.ArrayBuffer, &Meta.ByteOffset, &Meta.ByteLength );
	Chakra::IsOkay( Error, "JsGetTypedArrayInfo" );

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


size_t GetElementSize(JSTypedArrayType ArrayType)
{
	switch (ArrayType)
	{
	case kJSTypedArrayTypeInt8Array:	return sizeof(int8_t);
	case kJSTypedArrayTypeInt16Array:	return sizeof(int16_t);
	case kJSTypedArrayTypeInt32Array:	return sizeof(int32_t);
	case kJSTypedArrayTypeUint8Array:	return sizeof(uint8_t);
	case kJSTypedArrayTypeUint8ClampedArray:	return sizeof(uint8_t);
	case kJSTypedArrayTypeUint16Array:	return sizeof(uint16_t);
	case kJSTypedArrayTypeUint32Array:	return sizeof(uint32_t);
	case kJSTypedArrayTypeFloat32Array:	return sizeof(float);
	case kJSTypedArrayTypeFloat64Array:	return sizeof(double);
	default:break;
	}

	throw Soy::AssertException("GetElementSize Unhandled type");
}

JSObjectRef	JSObjectMakeTypedArrayWithBytesWithCopy(JSContextRef Context,JSTypedArrayType ArrayType,const uint8_t* ExternalBuffer,size_t ExternalBufferSize,JSValueRef* Exception)
{
	auto ExternalArray = GetRemoteArray(ExternalBuffer, ExternalBufferSize);
	JSValueRef ArrayBuffer = nullptr;
	auto Error = JsCreateArrayBuffer(ExternalBufferSize, &ArrayBuffer);
	Chakra::IsOkay(Error, "JsCreateArrayBuffer");
		
	auto ArrayTypeChakra = static_cast<JsTypedArrayType>(ArrayType);
	JsValueRef BaseArray = ArrayBuffer;
	auto ByteOffset = 0;
	auto ElementSize = GetElementSize(ArrayType);
	auto ElementCount = ExternalBufferSize / ElementSize;
	auto Alignment = ExternalBufferSize % ElementSize;
	if (Alignment != 0)
		throw Soy::AssertException("Typed array data not aligned to type size");
	JsValueRef TypedArrayObject = nullptr;
	Error = JsCreateTypedArray(ArrayTypeChakra, BaseArray, ByteOffset, ElementCount, &TypedArrayObject);
	Chakra::IsOkay(Error, "JsCreateTypedArray");
	
	//	copy data
	auto Meta = GetTypedArrayBufferMeta(TypedArrayObject);
	auto TypedArrayArray = GetRemoteArray(Meta.Bytes, Meta.ByteLength);
	TypedArrayArray.Copy(ExternalArray);
	return TypedArrayObject;
}

JSObjectRef	JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef Context, JSTypedArrayType ArrayType, void* ExternalBuffer, size_t ExternalBufferSize, JSTypedArrayBytesDeallocator Dealloc, void* DeallocContext, JSValueRef* Exception)
{
	//JsCreateExternalArrayBuffer
	throw Soy::AssertException("v8 cannot use JSObjectMakeTypedArrayWithBytesNoCopy as we dont do the dealloc");
}

void* JSObjectGetTypedArrayBytesPtr(JSContextRef Context,JSObjectRef ArrayObject,JSValueRef* Exception)
{
	auto Meta = GetTypedArrayBufferMeta( ArrayObject.mValue );
	return Meta.Bytes;
}

size_t JSObjectGetTypedArrayByteOffset(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	auto Meta = GetTypedArrayMeta( Array.mValue );
	return Meta.ByteOffset;
}

size_t JSObjectGetTypedArrayLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	auto Meta = GetTypedArrayBufferMeta(Array.mValue);
	auto ElementCount = Meta.ByteLength / Meta.ElementSize;
	return ElementCount;
}

size_t JSObjectGetTypedArrayByteLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	auto Meta = GetTypedArrayMeta( Array.mValue );
	return Meta.ByteLength;
}


#if defined(TARGET_WINDOWS)
enum JsParseScriptAttributes
{
	JsParseScriptAttributeNone
};
#endif

#if defined(TARGET_WINDOWS)
JsErrorCode JsRun(JSValueRef Source, JsSourceContext ScriptCookie, JSValueRef Filename,JsParseScriptAttributes ParseAttributes,JSValueRef* Result)
{
	//	gr: this is a bit redundant, we make a string, then go back again.
	//		lets make a higher level JS func
	JSContextRef Context = nullptr;
	auto SourceString = Bind::GetString(Context, Source);
	auto SourceStringW = Soy::StringToWString(SourceString);
	auto FilenameString = Bind::GetString(Context, Filename);
	auto FilenameStringW = Soy::StringToWString(FilenameString);

	return JsRunScript(SourceStringW.c_str(), ScriptCookie, FilenameStringW.c_str(), Result);
}
#endif


JSValueRef JSEvaluateScript(JSContextRef Context,JSStringRef Source,JSObjectRef This,JSStringRef Filename,int LineNumber,JSValueRef* Exception)
{
	if ( !This )
		This = JSContextGetGlobalObject(Context);
	
	if ( !Filename )
		Filename = Bind::GetString( Context, std::string("<null filename>") );
	
	auto ParseAttributes = JsParseScriptAttributeNone;
	JsSourceContext ScriptCookie = Chakra::GetNewScriptContext();
	JsValueRef Result = nullptr;

	//	winsdk doesn't have JSrun
	//	JsRunScript
	/*
	//	parse it and turn into a script
	JsParseScript
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

void JSGlobalContextSetQueueJobFunc(JSContextGroupRef ContextGroup, JSGlobalContextRef Context, std::function<void(std::function<void(JSContextRef)>)> QueueJobFunc)
{
	auto* Vm = ContextGroup.mVirtualMachine.get();
	Vm->SetQueueJobFunc(Context, QueueJobFunc);
}


bool JSContextGroupRunVirtualMachineTasks(JSContextGroupRef ContextGroup, std::function<void(std::chrono::milliseconds)> &Sleep)
{
	return true;
}

void Chakra::TVirtualMachine::SetQueueJobFunc(JSGlobalContextRef Context, std::function<void(std::function<void(JSContextRef)>)> QueueJobFunc)
{
	mQueueJobFuncs[Context] = QueueJobFunc;
}

void Chakra::TVirtualMachine::QueueTask(JsValueRef Task,JSGlobalContextRef Context)
{
	JsAddRef(Task, nullptr);
	auto Run = [=](JsContextRef Context)
	{
		auto Result = JSObjectCallAsFunction(Context, Task);
		//	nothing to do with result
		JsRelease(Task,nullptr);
	};
	auto& QueueFunc = mQueueJobFuncs[Context];
	QueueFunc(Run);
}


JSGlobalContextRef JSGlobalContextCreateInGroup(JSContextGroupRef ContextGroup,JSClassRef GlobalClass)
{
	if ( GlobalClass )
		throw Soy::AssertException("Not currently supporting creating context with a global class");
	
	JsContextRef NewContext = nullptr;
	auto Error = JsCreateContext( ContextGroup.mVirtualMachine->mRuntime, &NewContext );
	Chakra::IsOkay( Error, "JsCreateContext" );
	
	Chakra::SetVirtualMachine( NewContext, ContextGroup );
	auto* Vm = ContextGroup.mVirtualMachine.get();

	auto SetupPromiseCallback = [&](JSContextRef Context)
	{
		//	promise callback handling
		JsPromiseContinuationCallback PromiseCallback = [](JsValueRef Task,void* UserData)
		{
			auto ContextRef = reinterpret_cast<JSGlobalContextRef>(UserData);
			auto& Vm = Chakra::GetVirtualMachine(ContextRef);
			//std::Debug << "JsPromiseContinuationCallback" << std::endl;
			Vm.QueueTask(Task, ContextRef);
		};
		auto GlobalContext = JSContextGetGlobalContext(Context);
		Error = JsSetPromiseContinuationCallback( PromiseCallback, GlobalContext);
		Chakra::IsOkay( Error, "JsSetPromiseContinuationCallback" );
	};
	
	auto SetupDebugging = [&](JSContextRef Context)
	{
		//	gr: calling this makes debugging work, but the camera code calls a different mode which fails
		//	The host should make sure that CoInitializeEx is called with COINIT_MULTITHREADED or COINIT_APARTMENTTHREADED at least once before using this API
		auto InitResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		//auto InitResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);		
		Platform::IsOkay(InitResult, "CoInitializeEx(COINIT_MULTITHREADED)");

		auto Error = JsStartDebugging();
		Chakra::IsOkay(Error, "JsStartDebugging");
	};

	//	needs to be setup in-context
	JSLockAndRun(NewContext, SetupPromiseCallback);

	try
	{
		//JSLockAndRun(NewContext, SetupDebugging);
	}
	catch (std::exception& e)
	{
		std::Debug << "Failed to enable JS debugging; " << e.what() << std::endl;
	}

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
	auto GlobalContext = JSContextGetGlobalContext(Context);
	auto& vm = Chakra::GetVirtualMachine(GlobalContext);

	auto Error = JsCollectGarbage( vm.mRuntime );
	Chakra::IsOkay(Error, "JsCollectGarbage");
}

JSStringRef	JSStringCreateWithUTF8CString(JSContextRef Context, const std::string& Buffer)
{
	auto& vm = Chakra::GetVirtualMachine(Context);
	JsValueRef String = vm.GetCachedString(Buffer);
	return String;
}

JSStringRef	JSStringCreateWithUTF8CString(JSContextRef Context,const char* Buffer)
{
	//	gr: we can probably do this with each system as strings are global to VM (except in v8)
	auto& vm = Chakra::GetVirtualMachine(Context);
	JsValueRef String = vm.GetCachedString(Buffer);
	return String;
	/*
	JsValueRef String = nullptr;
	auto Length = strlen(Buffer);
	auto Error = JsCreateString( Buffer, Length, &String );
	Chakra::IsOkay( Error, std::string("JSStringCreateWithUTF8CString") + std::string(" with ") + Buffer );
	return String;
	*/
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
	
	//	bind functions to a prototype, then set that ON the constructor
	//	https://github.com/microsoft/Chakra-Samples/blob/master/ChakraCore%20Samples/OpenGL%20Engine/OpenGLEngine/ChakraCoreHost.cpp#L480
	auto Error = JsCreateObject( &Class.mPrototype );
	Chakra::IsOkay( Error, "JsCreateObject(Prototype)" );

	{
		int i=0;
		while ( true )
		{
			auto& FunctionDefinition = Definition.staticFunctions[i];
			i++;
			if ( FunctionDefinition.name == nullptr )
				break;
	
			auto NameValue = Bind::GetString( Context, FunctionDefinition.name );
			auto Function = JSObjectMakeFunctionWithCallback( Context, NameValue, FunctionDefinition.callAsFunction );
			
			auto Attributes = kJSPropertyAttributeNone;
			JSObjectSetProperty( Context, Class.mPrototype, FunctionDefinition.name, Function, Attributes, nullptr );
		}
	}
	
	//	example code does this, but there's a specific function...
	//	gr: set prototype after construction
	//		we/example code cheats but setting it up by default
	{
		//	gr: this does nothing
		//auto Attributes = kJSPropertyAttributeNone;
		//auto PrototypeString = Bind::GetString( Context, "prototype" );
		//JSObjectSetProperty( Context, Class.mConstructor, PrototypeString, Class.mPrototype, Attributes, nullptr );
		
		//	this does nothing, needs to be called post construct
		//Error = JsSetPrototype( Class.mConstructor, Prototype );
		//Chakra::IsOkay( Error, "JsSetPrototype");
	}
	
	return Class;
}

void		JSClassRetain(JSClassRef Class)
{
	//	protect values here?
	JSContextRef Context = nullptr;
	JSValueProtect(Context, Class.mConstructor);
	JSValueProtect(Context, Class.mPrototype);
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
	//JSValueProtect(nullptr, mValue);
}

void JSValueWrapper::Release()
{
	if ( !mValue )
		return;
	//JSValueUnprotect(nullptr, mValue);
	mValue = nullptr;
}

void JSObjectTypedArrayDirty(JSContextRef Context, JSObjectRef Object)
{

}


#endif//JSAPI_CHAKRA

