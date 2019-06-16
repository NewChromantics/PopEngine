#include "JsCoreDll.h"
#include <functional>
#include "SoyLib\src\BufferArray.hpp"
#include "SoyLib\src\SoyRuntimeLibrary.h"

//	load the DLL ourselves and pair up all the symbols

namespace JsCore
{
	std::shared_ptr<Soy::TRuntimeLibrary> JavascriptCoreDll;
}


//	should match this symbol from the library, but lets just get this done for now
const JSClassDefinition __JSClassDefinitionEmpty = {};
extern "C" const JSClassDefinition* __imp_kJSClassDefinitionEmpty = &__JSClassDefinitionEmpty;


template<const char* SymbolName,typename RESULTTYPE, typename... ARGTYPES>
class Wrapper
{
public:
	static std::function<RESULTTYPE(ARGTYPES...)>& GetWrapperFunction()
	{
		//	this implements the static
		static std::function<RESULTTYPE(ARGTYPES...)> WrapperFunction;
		return WrapperFunction;
	}

	//	static C-function to execute the c++ function
	static RESULTTYPE Invoke(ARGTYPES... args)
	{
		//	this should throw bad_function if not initialised
		auto& WrapperFunction = GetWrapperFunction();
		return WrapperFunction(args...);
	}

	//	get a C-style function pointer to our invoker
	static void* GetInvokeFunctionPointer()
	{
		return Invoke;
	}
};

template<const char* FUNCTIONNAME,typename RESULTTYPE, typename... ARGTYPES>
void* GetStaticFunction2(std::function<RESULTTYPE(ARGTYPES...)> f)
{
	return Wrapper<FUNCTIONNAME,RESULTTYPE, ARGTYPES...>::GetInvokeFunctionPointer();
}

template<const char* FUNCTIONNAME,typename FUNCTIONTYPE>
FUNCTIONTYPE* GetStaticFunction()
{
	std::function<FUNCTIONTYPE> DummyToGetType;
	auto* StaticFunc = GetStaticFunction2<FUNCTIONNAME>(DummyToGetType);
	return reinterpret_cast<FUNCTIONTYPE*>(StaticFunc);
}


template<const char* FUNCTIONNAME,typename RESULTTYPE, typename... ARGTYPES>
std::function<RESULTTYPE(ARGTYPES...)>& GetFunctionPointer2(std::function<RESULTTYPE(ARGTYPES...)> f)
{
	return Wrapper<FUNCTIONNAME,RESULTTYPE, ARGTYPES...>::GetWrapperFunction();
}

template<const char* FUNCTIONNAME,typename FUNCTIONTYPE>
std::function<FUNCTIONTYPE>& GetFunctionPointer()
{
	std::function<FUNCTIONTYPE> DummyToGetType;
	return GetFunctionPointer2<FUNCTIONNAME>(DummyToGetType);
}


template<const char* FUNCTIONNAME,typename FUNCTIONTYPE>
void LoadFunction(Soy::TRuntimeLibrary& Library)
{
	auto& Function = GetFunctionPointer<FUNCTIONNAME, FUNCTIONTYPE>();
	Library.SetFunction(Function, FUNCTIONNAME);
}


template<void* FUNCTIONPTR>
void LoadFunction(Soy::TRuntimeLibrary& Library)
{
	//	if we use static_assert(true), it asserts at definition,
	//	we need to assert at instantiation (maybe it's because of the use of TYPE?)
	//	https://stackoverflow.com/a/17679382/355753
	static_assert( sizeof(TYPE) == -1, "This function needs to be specialised with DEFINE_JS_BRIDGE" );
}



#define DEFINE_JS_BRIDGE(FUNCTIONNAME)	\
extern "C" const char FUNCTIONNAME ## _FuncName[] = #FUNCTIONNAME;	\
extern "C" decltype(FUNCTIONNAME)* __imp_ ## FUNCTIONNAME = GetStaticFunction<FUNCTIONNAME ## _FuncName,decltype(FUNCTIONNAME)>();	\
\

//template<> inline void LoadFunction<&FUNCTIONNAME>(Soy::TRuntimeLibrary& Library)	{	LoadFunction<FUNCTIONNAME ## _FuncName,decltype(FUNCTIONNAME)>( Library );	}	\


//template<> inline void LoadFunction<decltype(FUNCTIONNAME)>(Soy::TRuntimeLibrary& Library)	{	LoadFunction<FUNCTIONNAME ## _FuncName,decltype(FUNCTIONNAME)>( Library );	}	\


DEFINE_JS_BRIDGE(JSEvaluateScript);
DEFINE_JS_BRIDGE(JSGarbageCollect);
DEFINE_JS_BRIDGE(JSValueGetType);
DEFINE_JS_BRIDGE(JSValueIsUndefined);
DEFINE_JS_BRIDGE(JSValueIsNumber);
DEFINE_JS_BRIDGE(JSValueIsObject);
DEFINE_JS_BRIDGE(JSValueIsArray);
DEFINE_JS_BRIDGE(JSValueGetTypedArrayType);
DEFINE_JS_BRIDGE(JSClassCreate);
DEFINE_JS_BRIDGE(JSClassRetain);
DEFINE_JS_BRIDGE(JSClassRelease);
DEFINE_JS_BRIDGE(JSObjectMake);
DEFINE_JS_BRIDGE(JSObjectMakeFunctionWithCallback);
DEFINE_JS_BRIDGE(JSObjectMakeConstructor);
DEFINE_JS_BRIDGE(JSObjectMakeArray);
DEFINE_JS_BRIDGE(JSObjectMakeDate);
DEFINE_JS_BRIDGE(JSObjectMakeError);
DEFINE_JS_BRIDGE(JSObjectMakeRegExp);
DEFINE_JS_BRIDGE(JSObjectMakeFunction);
DEFINE_JS_BRIDGE(JSValueMakeUndefined);
DEFINE_JS_BRIDGE(JSValueMakeNull);
DEFINE_JS_BRIDGE(JSValueMakeBoolean);
DEFINE_JS_BRIDGE(JSValueMakeNumber);
DEFINE_JS_BRIDGE(JSValueMakeString);
DEFINE_JS_BRIDGE(JSValueMakeFromJSONString);
DEFINE_JS_BRIDGE(JSValueToBoolean);
DEFINE_JS_BRIDGE(JSValueToNumber);
DEFINE_JS_BRIDGE(JSValueToStringCopy);
DEFINE_JS_BRIDGE(JSValueToObject);
DEFINE_JS_BRIDGE(JSValueProtect);
DEFINE_JS_BRIDGE(JSValueUnprotect);
DEFINE_JS_BRIDGE(JSContextGroupCreate);
DEFINE_JS_BRIDGE(JSContextGroupRelease);
DEFINE_JS_BRIDGE(JSGlobalContextCreateInGroup);
DEFINE_JS_BRIDGE(JSGlobalContextRelease);
DEFINE_JS_BRIDGE(JSContextGetGlobalObject);
DEFINE_JS_BRIDGE(JSContextGetGlobalContext);
DEFINE_JS_BRIDGE(JSGlobalContextSetName);
DEFINE_JS_BRIDGE(JSStringCreateWithUTF8CString);
DEFINE_JS_BRIDGE(JSStringGetLength);
DEFINE_JS_BRIDGE(JSStringGetUTF8CString);
DEFINE_JS_BRIDGE(JSStringRelease);
DEFINE_JS_BRIDGE(JSObjectMakeTypedArrayWithBytesNoCopy);
DEFINE_JS_BRIDGE(JSObjectGetTypedArrayBytesPtr);
DEFINE_JS_BRIDGE(JSObjectGetTypedArrayLength);
DEFINE_JS_BRIDGE(JSObjectGetTypedArrayByteLength);
DEFINE_JS_BRIDGE(JSObjectGetTypedArrayByteOffset);
DEFINE_JS_BRIDGE(JSObjectGetPrototype);
DEFINE_JS_BRIDGE(JSObjectSetPrototype);
DEFINE_JS_BRIDGE(JSObjectHasProperty);
DEFINE_JS_BRIDGE(JSObjectGetProperty);
DEFINE_JS_BRIDGE(JSObjectSetProperty);
DEFINE_JS_BRIDGE(JSObjectDeleteProperty);
DEFINE_JS_BRIDGE(JSObjectGetPropertyAtIndex);
DEFINE_JS_BRIDGE(JSObjectSetPropertyAtIndex);
DEFINE_JS_BRIDGE(JSObjectGetPrivate);
DEFINE_JS_BRIDGE(JSObjectSetPrivate);
DEFINE_JS_BRIDGE(JSObjectIsFunction);
DEFINE_JS_BRIDGE(JSObjectCallAsFunction);
DEFINE_JS_BRIDGE(JSObjectIsConstructor);
DEFINE_JS_BRIDGE(JSObjectCallAsConstructor);
DEFINE_JS_BRIDGE(JSObjectCopyPropertyNames);
DEFINE_JS_BRIDGE(JSPropertyNameArrayRetain);
DEFINE_JS_BRIDGE(JSPropertyNameArrayRelease);
DEFINE_JS_BRIDGE(JSPropertyNameArrayGetCount);
DEFINE_JS_BRIDGE(JSPropertyNameArrayGetNameAtIndex);
DEFINE_JS_BRIDGE(JSPropertyNameAccumulatorAddName);


#define LOAD_FUNCTION(FUNCTIONNAME)		LoadFunction<FUNCTIONNAME ## _FuncName,decltype(FUNCTIONNAME)>(Library)



void JsCore::LoadDll()
{
	if ( JavascriptCoreDll )
		return;

	const char* Filename = "JavascriptCore.dll";
	JavascriptCoreDll.reset(new Soy::TRuntimeLibrary(Filename));
	auto& Library = *JavascriptCoreDll;


#undef DEFINE_JS_BRIDGE
#define DEFINE_JS_BRIDGE	LOAD_FUNCTION

	DEFINE_JS_BRIDGE(JSEvaluateScript);
	DEFINE_JS_BRIDGE(JSGarbageCollect);
	DEFINE_JS_BRIDGE(JSValueGetType);
	DEFINE_JS_BRIDGE(JSValueIsUndefined);
	DEFINE_JS_BRIDGE(JSValueIsNumber);
	DEFINE_JS_BRIDGE(JSValueIsObject);
	DEFINE_JS_BRIDGE(JSValueIsArray);
	DEFINE_JS_BRIDGE(JSValueGetTypedArrayType);
	DEFINE_JS_BRIDGE(JSClassCreate);
	DEFINE_JS_BRIDGE(JSClassRetain);
	DEFINE_JS_BRIDGE(JSClassRelease);
	DEFINE_JS_BRIDGE(JSObjectMake);
	DEFINE_JS_BRIDGE(JSObjectMakeFunctionWithCallback);
	DEFINE_JS_BRIDGE(JSObjectMakeConstructor);
	DEFINE_JS_BRIDGE(JSObjectMakeArray);
	DEFINE_JS_BRIDGE(JSObjectMakeDate);
	DEFINE_JS_BRIDGE(JSObjectMakeError);
	DEFINE_JS_BRIDGE(JSObjectMakeRegExp);
	DEFINE_JS_BRIDGE(JSObjectMakeFunction);
	DEFINE_JS_BRIDGE(JSValueMakeUndefined);
	DEFINE_JS_BRIDGE(JSValueMakeNull);
	DEFINE_JS_BRIDGE(JSValueMakeBoolean);
	DEFINE_JS_BRIDGE(JSValueMakeNumber);
	DEFINE_JS_BRIDGE(JSValueMakeString);
	DEFINE_JS_BRIDGE(JSValueMakeFromJSONString);	
	DEFINE_JS_BRIDGE(JSValueToBoolean);
	DEFINE_JS_BRIDGE(JSValueToNumber);
	DEFINE_JS_BRIDGE(JSValueToStringCopy);
	DEFINE_JS_BRIDGE(JSValueToObject);
	DEFINE_JS_BRIDGE(JSValueProtect);
	DEFINE_JS_BRIDGE(JSValueUnprotect);
	DEFINE_JS_BRIDGE(JSContextGroupCreate);
	DEFINE_JS_BRIDGE(JSContextGroupRelease);
	DEFINE_JS_BRIDGE(JSGlobalContextCreateInGroup);
	DEFINE_JS_BRIDGE(JSGlobalContextRelease);
	DEFINE_JS_BRIDGE(JSContextGetGlobalObject);
	DEFINE_JS_BRIDGE(JSContextGetGlobalContext);
	DEFINE_JS_BRIDGE(JSGlobalContextSetName);
	DEFINE_JS_BRIDGE(JSStringCreateWithUTF8CString);
	DEFINE_JS_BRIDGE(JSStringGetLength);
	DEFINE_JS_BRIDGE(JSStringGetUTF8CString);
	DEFINE_JS_BRIDGE(JSStringRelease);
	DEFINE_JS_BRIDGE(JSObjectMakeTypedArrayWithBytesNoCopy);
	DEFINE_JS_BRIDGE(JSObjectGetTypedArrayBytesPtr);
	DEFINE_JS_BRIDGE(JSObjectGetTypedArrayLength);
	DEFINE_JS_BRIDGE(JSObjectGetTypedArrayByteLength);
	DEFINE_JS_BRIDGE(JSObjectGetTypedArrayByteOffset);
	DEFINE_JS_BRIDGE(JSObjectGetPrototype);
	DEFINE_JS_BRIDGE(JSObjectSetPrototype);
	DEFINE_JS_BRIDGE(JSObjectHasProperty);
	DEFINE_JS_BRIDGE(JSObjectGetProperty);
	DEFINE_JS_BRIDGE(JSObjectSetProperty);
	DEFINE_JS_BRIDGE(JSObjectDeleteProperty);
	DEFINE_JS_BRIDGE(JSObjectGetPropertyAtIndex);
	DEFINE_JS_BRIDGE(JSObjectSetPropertyAtIndex);
	DEFINE_JS_BRIDGE(JSObjectGetPrivate);
	DEFINE_JS_BRIDGE(JSObjectSetPrivate);
	DEFINE_JS_BRIDGE(JSObjectIsFunction);
	DEFINE_JS_BRIDGE(JSObjectCallAsFunction);
	DEFINE_JS_BRIDGE(JSObjectIsConstructor);
	DEFINE_JS_BRIDGE(JSObjectCallAsConstructor);
	DEFINE_JS_BRIDGE(JSObjectCopyPropertyNames);
	DEFINE_JS_BRIDGE(JSPropertyNameArrayRetain);
	DEFINE_JS_BRIDGE(JSPropertyNameArrayRelease);
	DEFINE_JS_BRIDGE(JSPropertyNameArrayGetCount);
	DEFINE_JS_BRIDGE(JSPropertyNameArrayGetNameAtIndex);
	DEFINE_JS_BRIDGE(JSPropertyNameAccumulatorAddName);


}


