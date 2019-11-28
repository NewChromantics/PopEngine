#pragma once

#include <SoyTypes.h>

#if !defined(JSAPI_CHAKRA)
#error This file should not be being built
#endif

//	gr: most comprehensive sample
//	https://github.com/microsoft/Chakra-Samples/blob/master/Chakra%20Samples/JSRT%20Win32%20Hosting%20Samples/Edge%20JSRT%20Samples/C%2B%2B/ChakraHost.cpp#L411

#if defined(TARGET_WINDOWS)
	#define USE_EDGEMODE_JSRT
	#include <jsrt.h>
	#undef min
	#undef max
#else
	#include <ChakraCore.h>
#endif

#include "MemHeap.hpp"
#include "HeapArray.hpp"

namespace JsCore
{
	class TContext;
}

namespace Chakra
{
	const int InternalFieldDataIndex = 0;

	class TAllocator;
	class TVirtualMachine;
	
	void	IsOkay(JsErrorCode Error,const std::string& Context);
}


//	todo:
class JSContextGroupRef;
typedef JsContextRef JSContextRef;	//	void* pointer
typedef JsValueRef JSValueRef;


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
	kJSTypedArrayTypeInt8Array = JsArrayTypeInt8,
	kJSTypedArrayTypeInt16Array = JsArrayTypeInt16,
	kJSTypedArrayTypeInt32Array = JsArrayTypeInt32,
	kJSTypedArrayTypeUint8Array = JsArrayTypeUint8,
	kJSTypedArrayTypeUint8ClampedArray = JsArrayTypeUint8Clamped,
	kJSTypedArrayTypeUint16Array = JsArrayTypeUint16,
	kJSTypedArrayTypeUint32Array = JsArrayTypeUint32,
	kJSTypedArrayTypeFloat32Array = JsArrayTypeFloat32,
	kJSTypedArrayTypeFloat64Array = JsArrayTypeFloat64,

	//	not in chakra
	kJSTypedArrayTypeNone,
	kJSTypedArrayTypeArrayBuffer,
};


enum JSPropertyAttributes
{
	kJSPropertyAttributeNone
};

typedef JSContextRef JSGlobalContextRef;




class Chakra::TVirtualMachine
{
public:
	TVirtualMachine(const std::string& RuntimePath);
	~TVirtualMachine();
	
	//	lock & run & unlock
	void			Execute(JSGlobalContextRef Context,std::function<void(JSContextRef&)>& Execute);
	
	void			QueueTask(JsValueRef Task, JSGlobalContextRef Context);
	void			FlushTasks(JSContextGroupRef Context);

	void			SetQueueJobFunc(JSGlobalContextRef Context, std::function<void(std::function<void(JSContextRef)>)> QueueJobFunc);
	void			SetWakeJobQueueFunc(JSGlobalContextRef Context, std::function<void()> WakeJobQueueFunc);

public:
	//	the runtime can only execute one context at once, AND (on osx at least) the runtime
	//	falls over if you try and set the same context twice
	std::mutex		mCurrentContextLock;
	JSContextRef	mCurrentContext = nullptr;
	JsRuntimeHandle	mRuntime;

	std::function<void()>	mWakeJobQueue;
	//	microtasks queued from promises
	std::mutex			mTasksLock;
	Array<std::pair<JsValueRef, JSGlobalContextRef>>	mTasks;

	std::map<JSGlobalContextRef, std::function<void()>>	mWakeJobQueueFuncs;
	std::map<JSGlobalContextRef, std::function<void(std::function<void(JSContextRef)>)>>	mQueueJobFuncs;
};

//	this is the virtual machine
//	if we use shared ptr's i think we're okay just passing it around
class JSContextGroupRef
{
public:
	JSContextGroupRef(std::nullptr_t);
	JSContextGroupRef(const std::string& RuntimePath);
	
	
	operator bool() const;
	/*
	void					CreateContext(JSGlobalContextRef& NewContext);
	V8::TVirtualMachine&	GetVirtualMachine()	{	return *mVirtualMachine;	}
	*/
	std::shared_ptr<Chakra::TVirtualMachine>	mVirtualMachine;
};

/*
//	actual persistent context
class JSGlobalContextRef
{
public:
	JSGlobalContextRef(std::nullptr_t)	{}
	
	//operator bool() const			{	return mContext!=nullptr;	}

};
*/

//	to appease generic binding, Object's are typed to be different from dumb values
//	gr: any JsValueRef's off the stack could be cleaned up, so we need to increment inside this class (erk!)
class JSValueWrapper
{
public:
	JSValueWrapper(std::nullptr_t)	{}
	JSValueWrapper(const JSValueWrapper& That)
	{
		Set( That.mValue );
	}
	JSValueWrapper(JSValueRef Value)
	{
		Set( Value );
	}
	~JSValueWrapper()
	{
		Release();
	}
	
	void			operator=(std::nullptr_t Null)				{	Release();	}
	void			operator=(JSValueWrapper That)				{	Set( That.mValue );	}
	bool			operator!=(std::nullptr_t Null) const		{	return mValue != Null;	}
	bool			operator!=(const JSValueWrapper& That) const	{	return mValue != That.mValue;	}
	operator 		bool() const								{	return mValue != nullptr;	}
	
	//	gr: for some reason this was getting nulled after being used to construct another object...
	//operator		JSValueRef() const							{	return mValue;	}
	
private:
	void			Set(JSValueRef Value);
	void			Release();

public:
	JSValueRef		mValue = nullptr;
};

typedef JSValueWrapper JSObjectRef;
typedef JSValueWrapper JSStringRef;



typedef void(*JSTypedArrayBytesDeallocator)(void* bytes, void* deallocatorContext);
//typedef JSObjectRef(*JSObjectCallAsConstructorCallback) (JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
//typedef v8::FunctionCallback JSObjectCallAsConstructorCallback;
//typedef JSValueRef(*JSObjectCallAsFunctionCallback) (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
//typedef v8::FunctionCallback JSObjectCallAsFunctionCallback;
//typedef void(*JSObjectFinalizeCallback)(const v8::WeakCallbackInfo<void>& Meta);
typedef JsNativeFunction JSObjectCallAsFunctionCallback;
typedef JsNativeFunction JSObjectCallAsConstructorCallback;
typedef JsFinalizeCallback JSObjectFinalizeCallback;


typedef struct {
	const char* name;
	JSObjectCallAsFunctionCallback callAsFunction;
	JSPropertyAttributes attributes;
} JSStaticFunction;



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


class JSClassRef
{
public:
	JSClassRef(std::nullptr_t)	{}
	
	operator bool	() const	{	return mConstructor;	}
	
	JSObjectFinalizeCallback	mDestructor = nullptr;
	
	//	these need to be persistent!
	JSValueRef			mConstructor = nullptr;
	JsValueRef			mPrototype = nullptr;
};


class JSPropertyNameArrayRef
{
public:

};





void		JSObjectSetPrivate(JSContextRef Context,JSObjectRef Object,void* Data);
void*		JSObjectGetPrivate(JSContextRef Context,JSObjectRef Object);
JSObjectRef	JSObjectMake(JSContextRef Context,JSClassRef Class,void*);
JSValueRef	JSObjectGetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef* Exception);
void		JSObjectSetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception );
void		JSObjectSetPropertyAtIndex(JSContextRef Context,JSObjectRef This,size_t Index,JSValueRef Value,JSValueRef* Exception);
JSValueRef	JSObjectToValue(JSObjectRef Object);

JSType		JSValueGetType(JSValueRef Value);
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
JSValueRef	JSValueMakeNumber(JSContextRef Context,double Value);

bool		JSObjectIsFunction(JSContextRef Context,JSObjectRef Value);
JSValueRef	JSObjectCallAsFunction(JSContextRef Context, JSValueRef Functor);
JSValueRef	JSObjectCallAsFunction(JSContextRef Context, JSObjectRef Object, JSObjectRef This, size_t ArgumentCount, JSValueRef* Arguments, JSValueRef* Exception);
JSValueRef	JSObjectMakeFunctionWithCallback(JSContextRef Context,JSStringRef Name,JSObjectCallAsFunctionCallback FunctionPtr);

bool		JSValueToBoolean(JSContextRef Context,JSValueRef Value);
JSValueRef	JSValueMakeBoolean(JSContextRef Context,bool Value);

JSValueRef	JSValueMakeUndefined(JSContextRef Context);
bool		JSValueIsUndefined(JSContextRef Context,JSValueRef Value);

JSValueRef	JSValueMakeNull(JSContextRef Context);
bool		JSValueIsNull(JSContextRef Context,JSValueRef Value);

JSObjectRef	JSObjectMakeArray(JSContextRef Context,size_t ElementCount,const JSValueRef* Elements,JSValueRef* Exception=nullptr);
bool		JSValueIsArray(JSContextRef Context,JSValueRef Value);
JSTypedArrayType	JSValueGetTypedArrayType(JSContextRef Context,JSObjectRef Value,JSValueRef* Exception=nullptr);
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

JSStringRef	JSStringCreateWithUTF8CString(JSContextRef Context,const char* Buffer);
size_t		JSStringGetUTF8CString(JSContextRef Context,JSStringRef String,char* Buffer,size_t BufferSize);
size_t		JSStringGetLength(JSStringRef String);
JSStringRef	JSValueToStringCopy(JSContextRef Context,JSValueRef Value,JSValueRef* Exception=nullptr);
JSValueRef	JSValueMakeString(JSContextRef Context,JSStringRef String);
void		JSStringRelease(JSStringRef String);

JSClassRef	JSClassCreate(JSContextRef Context,JSClassDefinition& Definition);
void		JSClassRetain(JSClassRef Class);


