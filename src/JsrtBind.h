#pragma once

#if !defined(JSAPI_JSRT)
#error This file should not be being built
#endif

//	gr: most comprehensive sample
//	https://github.com/microsoft/Chakra-Samples/blob/master/Chakra%20Samples/JSRT%20Win32%20Hosting%20Samples/Edge%20JSRT%20Samples/C%2B%2B/ChakraHost.cpp#L411

#define USE_EDGEMODE_JSRT
#include <jsrt.h>
#undef min
#undef max


#include "MemHeap.hpp"
#include "HeapArray.hpp"

namespace JsCore
{
	class TContext;
}

namespace Jsrt
{
	const int InternalFieldDataIndex = 0;

	class TAllocator;
	class TVirtualMachine;
	

}


enum JSType
{
	kJSTypeString,
	kJSTypeBoolean,
	kJSTypeUndefined,
	kJSTypeNull,
	kJSTypeObject,
	kJSTypeNumber,
};

enum JSClassAttributes
{
	kJSClassAttributeNone
};

//	_JsTypedArrayType in chakra
enum JSTypedArrayType
{
	kJSTypedArrayTypeNone,
	kJSTypedArrayTypeInt8Array = JsArrayTypeInt8,
	kJSTypedArrayTypeInt16Array = JsArrayTypeInt16,
	kJSTypedArrayTypeInt32Array = JsArrayTypeInt32,
	kJSTypedArrayTypeUint8Array = JsArrayTypeUint8,
	kJSTypedArrayTypeUint8ClampedArray = JsArrayTypeUint8Clamped,
	kJSTypedArrayTypeUint16Array = JsArrayTypeUint16,
	kJSTypedArrayTypeUint32Array = JsArrayTypeUint32,
	kJSTypedArrayTypeFloat32Array = JsArrayTypeFloat32,
	kJSTypedArrayTypeFloat64Array = JsArrayTypeFloat64,

	kJSTypedArrayTypeArrayBuffer,
};


enum JSPropertyAttributes
{
	kJSPropertyAttributeNone
};


typedef struct {
	const char* name;
	JSObjectCallAsFunctionCallback callAsFunction;
	JSPropertyAttributes attributes;
} JSStaticFunction;




class JSContextGroupRef;

typedef JsContextRef JSContextRef;
typedef JsValueRef JSValueRef;

/*
class JSContextRef
{
public:
	JSContextRef(std::nullptr_t)	{}
	JSContextRef(v8::Local<v8::Context>& Local);
	JSContextRef(v8::Local<v8::Context>&& Local);

};
*/
class JSGlobalContextRef;

//	this is the virtual machine
//	if we use shared ptr's i think we're okay just passing it around
class JSContextGroupRef
{
public:
	JSContextGroupRef(std::nullptr_t);
	JSContextGroupRef(const std::string& RuntimePath);
	/*
	
	operator bool() const	{	return mVirtualMachine!=nullptr;	}
	
	void					CreateContext(JSGlobalContextRef& NewContext);
	V8::TVirtualMachine&	GetVirtualMachine()	{	return *mVirtualMachine;	}
	
	std::shared_ptr<V8::TVirtualMachine>	mVirtualMachine;
	*/
};


//	actual persistent context
class JSGlobalContextRef
{
public:
	JSGlobalContextRef(std::nullptr_t)	{}
	
	operator bool() const			{	return mContext!=nullptr;	}

};




class JSObjectRef
{
public:
	JSObjectRef(std::nullptr_t)	{}
	JSObjectRef(v8::Local<v8::Object>& Local);
	JSObjectRef(v8::Local<v8::Object>&& Local);

	//void			operator=(std::nullptr_t Null);
	//void			operator=(JSObjectRef That);
	//bool			operator!=(std::nullptr_t Null) const;
	//bool			operator!=(const JSObjectRef& That) const;
	operator 		bool() const						{	return !mThis.IsEmpty();	}
	v8::Isolate&	GetIsolate()	{	return *mThis->GetIsolate();	}
};

/*
class JSValueRef
{
public:
	JSValueRef()	{}
	JSValueRef(std::nullptr_t)	{}
	JSValueRef(JSObjectRef Object);
	JSValueRef(v8::Local<v8::Value>& Local);
	JSValueRef(v8::Local<v8::Value>&& Local);

	void	operator=(JSObjectRef That);
	void	operator=(std::nullptr_t Null);
	//bool	operator!=(std::nullptr_t Null) const;
	operator bool() const						{	return !mThis.IsEmpty();	}

};

*/

class JSStringRef 
{
public:

};


class JSClassRef
{
public:
	JSClassRef(std::nullptr_t)	{}

};


typedef void(*JSTypedArrayBytesDeallocator)(void* bytes, void* deallocatorContext);
//typedef JSObjectRef(*JSObjectCallAsConstructorCallback) (JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
typedef v8::FunctionCallback JSObjectCallAsConstructorCallback;
//typedef JSValueRef(*JSObjectCallAsFunctionCallback) (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
typedef v8::FunctionCallback JSObjectCallAsFunctionCallback;
typedef void(*JSObjectFinalizeCallback)(const v8::WeakCallbackInfo<void>& Meta);



class JSClassDefinition
{
public:
	const char*							className = nullptr;	//	emulate JScore instability with raw pointers
	JSClassAttributes					attributes;
	JSObjectCallAsConstructorCallback	callAsConstructor = nullptr;
	JSObjectFinalizeCallback			finalize = nullptr;
	JSStaticFunction*					staticFunctions = nullptr;
};
extern const JSClassDefinition kJSClassDefinitionEmpty;



class JSPropertyNameArrayRef
{
public:

};





void		JSObjectSetPrivate(JSObjectRef Object,void* Data);
void*		JSObjectGetPrivate(JSObjectRef Object);
JSObjectRef	JSObjectMake(JSContextRef Context,JSClassRef Class,void*);
JSValueRef	JSObjectGetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef* Exception);
void		JSObjectSetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception );
void		JSObjectSetPropertyAtIndex(JSContextRef Context,JSObjectRef This,size_t Index,JSValueRef Value,JSValueRef* Exception);

JSType		JSValueGetType(JSContextRef Context,JSValueRef Value);
bool		JSValueIsObject(JSContextRef Context,JSValueRef Value);
bool		JSValueIsObject(JSContextRef Context,JSObjectRef Value);
JSObjectRef JSValueToObject(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);

//	gr: these actually take a normal context, but that's causing issues in JScore and we use globals instead
void		JSValueProtect(JSGlobalContextRef Context,JSValueRef Value);
void		JSValueUnprotect(JSGlobalContextRef Context,JSValueRef Value);

JSPropertyNameArrayRef	JSObjectCopyPropertyNames(JSContextRef Context,JSObjectRef This);
size_t		JSPropertyNameArrayGetCount(JSPropertyNameArrayRef Keys);
JSStringRef	JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef Keys,size_t Index);

JSValueRef	JSValueMakeFromJSONString(JSContextRef Context, JSStringRef String);

bool		JSValueIsNumber(JSContextRef Context,JSValueRef Value);
double		JSValueToNumber(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
JSValueRef	JSValueMakeNumber(JSContextRef Context,int Value);

bool		JSObjectIsFunction(JSContextRef Context,JSObjectRef Value);
JSValueRef	JSObjectCallAsFunction(JSContextRef Context,JSObjectRef Object,JSObjectRef This,size_t ArgumentCount,JSValueRef* Arguments,JSValueRef* Exception);
JSValueRef	JSObjectMakeFunctionWithCallback(JSContextRef Context,JSStringRef Name,JSObjectCallAsFunctionCallback FunctionPtr);

bool		JSValueToBoolean(JSContextRef Context,JSValueRef Value);
JSValueRef	JSValueMakeBoolean(JSContextRef Context,bool Value);

JSValueRef	JSValueMakeUndefined(JSContextRef Context);
bool		JSValueIsUndefined(JSContextRef Context,JSValueRef Value);

JSValueRef	JSValueMakeNull(JSContextRef Context);
bool		JSValueIsNull(JSContextRef Context,JSValueRef Value);

JSObjectRef	JSObjectMakeArray(JSContextRef Context,size_t ElementCount,const JSValueRef* Elements,JSValueRef* Exception=nullptr);
bool		JSValueIsArray(JSContextRef Context,JSValueRef Value);
JSTypedArrayType	JSValueGetTypedArrayType(JSContextRef Context,JSValueRef Value,JSValueRef* Exception=nullptr);
JSObjectRef	JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef Context,JSTypedArrayType ArrayType,void* Buffer,size_t BufferSize,JSTypedArrayBytesDeallocator Dealloc,void* DeallocContext,JSValueRef* Exception=nullptr);
void*		JSObjectGetTypedArrayBytesPtr(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception=nullptr);
size_t		JSObjectGetTypedArrayByteOffset(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception=nullptr);
size_t		JSObjectGetTypedArrayLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception=nullptr);
size_t		JSObjectGetTypedArrayByteLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception=nullptr);

JSValueRef			JSEvaluateScript(JSContextRef Context,JSStringRef Source,JSObjectRef This,JSStringRef Filename,int LineNumber,JSValueRef* Exception);
JSGlobalContextRef	JSContextGetGlobalContext(JSContextRef Context);
JSObjectRef			JSContextGetGlobalObject(JSContextRef Context);
JSContextGroupRef	JSContextGroupCreateWithRuntime(const std::string& RuntimeDirectory);
JSContextGroupRef	JSContextGroupCreate();
void				JSContextGroupRelease(JSContextGroupRef ContextGroup);
JSGlobalContextRef	JSGlobalContextCreateInGroup(JSContextGroupRef ContextGroup,JSClassRef GlobalClass);
void				JSGlobalContextSetName(JSGlobalContextRef Context,JSStringRef Name);
void				JSGlobalContextRelease(JSGlobalContextRef Context);
void				JSGarbageCollect(JSContextRef Context);

//JSStringRef	JSStringCreateWithUTF8CString(const char* Buffer);
JSStringRef	JSStringCreateWithUTF8CString(JSContextRef Context,const char* Buffer);
size_t		JSStringGetUTF8CString(JSContextRef Context,JSStringRef String,char* Buffer,size_t BufferSize);
size_t		JSStringGetLength(JSStringRef String);
JSStringRef	JSValueToStringCopy(JSContextRef Context,JSValueRef Value,JSValueRef* Exception=nullptr);
JSValueRef	JSValueMakeString(JSContextRef Context,JSStringRef String);
void		JSStringRelease(JSStringRef String);

JSClassRef	JSClassCreate(JSContextRef Context,JSClassDefinition* Definition);
void		JSClassRetain(JSClassRef Class);


