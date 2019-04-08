#include "JsCoreDll.h"
#include <functional>
#include "SoyLib\src\BufferArray.hpp"
#include "SoyLib\src\SoyRuntimeLibrary.h"

//	load the DLL ourselves and pair up all the symbols

namespace JsCore
{
	void	LoadDll();
	//std::shared_ptr<Soy::TRuntimeLibrary>	mDll;
}

	/*

typedef decltype(JSEvaluateScript) FUNCTIONTYPE;

template<typename FUNCTION>
class TAutoLinkedFunction
{
public:
	typedef FUNCTION TFUNCTION;
	static TFUNCTION* Static;
};
TAutoLinkedFunction<decltype(JSEvaluateScript)>::TFUNCTION JSEvaluateScript;
decltype(JSEvaluateScript) JSEvaluateScript = (decltype(JSEvaluateScript))&Blah;
//extern "C" decltype(JSEvaluateScript) JSEvaluateScript;
*/

BufferArray<std::function<void()>, 100> LoadFuncs;
void AddToLoadList(std::function<void()> Load)
{
	LoadFuncs.PushBack(Load);
}
/*

template <const size_t _UniqueId, typename _Res, typename... _ArgTypes>
struct fun_ptr_helper
{
public:
	typedef std::function<_Res(_ArgTypes...)> function_type;

	static void bind(function_type&& f)
	{ instance().fn_.swap(f); }

	static void bind(const function_type& f)
	{ instance().fn_=f; }

	static _Res invoke(_ArgTypes... args)
	{ return instance().fn_(args...); }

	typedef decltype(&fun_ptr_helper::invoke) pointer_type;
	static pointer_type ptr()
	{ return &invoke; }

private:
	static fun_ptr_helper& instance()
	{
		static fun_ptr_helper inst_;
		return inst_;
	}

	fun_ptr_helper() {}

	function_type fn_;
};

template <const size_t _UniqueId, typename _Res, typename... _ArgTypes>
typename fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::pointer_type
get_fn_ptr(const std::function<_Res(_ArgTypes...)>& f)
{
	fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::bind(f);
	return fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::ptr();
}

template<typename T>
std::function<typename std::enable_if<std::is_function<T>::value, T>::type>
make_function(T *t)
{
	return {t};
}



template <const size_t _UniqueId, typename _Res, typename... _ArgTypes>
typename fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::pointer_type
get_fn_ptr(const std::function<_Res(_ArgTypes...)>& f)
{
	fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::bind(f);
	return fun_ptr_helper<_UniqueId, _Res, _ArgTypes...>::ptr();
}
*/


template<const char* SymbolName,typename RESULTTYPE, typename... ARGTYPES>
class Wrapper
{
public:
	static std::function<RESULTTYPE(ARGTYPES...)>& GetWrapperFunc()
	{
		//	this implements the static
		static std::function<RESULTTYPE(ARGTYPES...)> WrapperFunc;
		return WrapperFunc;
	}

	static RESULTTYPE Invoke(ARGTYPES... args)
	{
		auto& WrapperFunc = GetWrapperFunc();
		return WrapperFunc(args...);
	}

	static void* GetInvokePtr()
	{
		return Invoke;
	}
};

template<const char* SymbolName,typename RESULTTYPE, typename... ARGTYPES>
void* GetStaticFunctionB(std::function<RESULTTYPE(ARGTYPES...)> f)
{
	return Wrapper<SymbolName,RESULTTYPE, ARGTYPES...>::GetInvokePtr();
}

template<const char* SymbolName,typename FUNCTYPE>
FUNCTYPE* GetStaticFunctionA()
{
	std::function<FUNCTYPE> f;
	auto* StaticFunc = GetStaticFunctionB<SymbolName>(f);
	return reinterpret_cast<FUNCTYPE*>(StaticFunc);
}



/*
template<typename FUNCTION>
FUNCTION* GetFunction(const char* Name)
{
	static std::function<FUNCTION> Cache;
	auto Loader = [&]()
	{
		//Cache = []()
		{
			std::Debug << "Hello " << Name << std::endl;
		}
	};
	AddToLoadList(Loader);
	return Cache;
}
auto* x = get_fn_ptr<0>(make_function(JSEvaluateScript));
*/

extern "C" const char JSEvaluateScript_FuncName[] = "JSWxxx";
extern "C" decltype(JSEvaluateScript)* __imp_JSEvaluateScript = GetStaticFunctionA<JSEvaluateScript_FuncName,decltype(JSEvaluateScript)>();
//extern "C" decltype(JSEvaluateScript)* __imp_JSEvaluateScript = reinterpret_cast<decltype(JSEvaluateScript)*>( JSEvaluateScriptX );
//extern "C" decltype(JSEvaluateScript)* __imp_JSEvaluateScript = GetFunction<decltype(JSEvaluateScript)>("Hello");


#define DEFINE_JS_BRIDGE(FUNCTIONNAME)	\
extern "C" const char FUNCTIONNAME ## _FuncName[] = #FUNCTIONNAME;	\
extern "C" decltype(FUNCTIONNAME)* __imp_ ## FUNCTIONNAME = GetStaticFunctionA<FUNCTIONNAME ## _FuncName,decltype(FUNCTIONNAME)>();	\

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
DEFINE_JS_BRIDGE(JSValueToBoolean);
DEFINE_JS_BRIDGE(JSValueToNumber);
DEFINE_JS_BRIDGE(JSValueToStringCopy);
DEFINE_JS_BRIDGE(JSValueToObject);
DEFINE_JS_BRIDGE(JSValueProtect);
DEFINE_JS_BRIDGE(JSValueUnprotect);
DEFINE_JS_BRIDGE(JSContextGroupRelease);
DEFINE_JS_BRIDGE(JSGlobalContextCreateInGroup);
DEFINE_JS_BRIDGE(JSGlobalContextRelease);
DEFINE_JS_BRIDGE(JSContextGetGlobalObject);
DEFINE_JS_BRIDGE(JSContextGetGlobalContext);
DEFINE_JS_BRIDGE(JSGlobalContextSetName);
DEFINE_JS_BRIDGE(JSStringCreateWithUTF8CString);
DEFINE_JS_BRIDGE(JSStringGetLength);
DEFINE_JS_BRIDGE(JSStringGetUTF8CString);
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









//auto JSEvaluateScript_FuncName = "JSEvaluateScript";

//extern "C" auto* __imp_JSEvaluateScript = &JSEvaluateScript;
/*
extern "C" FUNCTIONTYPE JSEvaluateScript(...)
{
	//= GetFunction<decltype(JSEvaluateScript)>();
}
*/

void JsCore::LoadDll()
{
	/*
	Soy::TRuntimeLibrary Library("JavascriptCore.dll", "JavaScriptCore.dll");
	Library.SetFunction(WrapperFunction<JSEvaluateScript_FuncName>::WrapperFunc, JSEvaluateScript_FuncName);
	*/
}


