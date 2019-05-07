#pragma once

#if !defined(JSAPI_V8)
#error This file should not be being built
#endif

//	gr: see if we can avoid including this everywhere
//		would need moving the JS* calls from JSBind.h
//		but JSBind classes still need to be exposed.
//		Might be good as a lib

#include <cstddef>
#include "v8/include/v8.h"
#include "v8/include/v8-platform.h"


//	gr: the diffs are external vs internal as well as API changes
//#define V8_VERSION	5
#define V8_VERSION	V8_MAJOR_VERSION

#if !defined(V8_VERSION)
#error need V8_VERSION 5 or 6
#endif


#include "MemHeap.hpp"

namespace JsCore
{
	class TContext;
}

namespace V8
{
	const int InternalFieldDataIndex = 0;

	class TAllocator;
	class TVirtualMachine;
	
	template<typename V8TYPE>
	class TPersistent;		//	this should turn into the JS persistent after some refactoring

	template<typename V8TYPE>
	class TLocalRef;

	template<typename V8TYPE>
	std::shared_ptr<TPersistent<V8TYPE>>	GetPersistent(v8::Isolate& Isolate,v8::Local<V8TYPE> Local);
	
	class TException;
	template<typename V8TYPE>
	void	IsOkay(v8::MaybeLocal<V8TYPE> Result,v8::Isolate& Isolate,v8::TryCatch& TryCatch,const std::string& Context);
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

enum JSTypedArrayType
{
	kJSTypedArrayTypeNone,
	kJSTypedArrayTypeInt8Array,
	kJSTypedArrayTypeInt16Array,
	kJSTypedArrayTypeInt32Array,
	kJSTypedArrayTypeUint8Array,
	kJSTypedArrayTypeUint8ClampedArray,
	kJSTypedArrayTypeUint16Array,
	kJSTypedArrayTypeUint32Array,
	kJSTypedArrayTypeFloat32Array,
};

typedef void(*JSTypedArrayBytesDeallocator)(void* bytes, void* deallocatorContext);
//typedef JSObjectRef(*JSObjectCallAsConstructorCallback) (JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
typedef v8::FunctionCallback JSObjectCallAsConstructorCallback;
//typedef JSValueRef(*JSObjectCallAsFunctionCallback) (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
typedef v8::FunctionCallback JSObjectCallAsFunctionCallback;
typedef void(*JSObjectFinalizeCallback)(const v8::WeakCallbackInfo<void>& Meta);


enum JSPropertyAttributes
{
	kJSPropertyAttributeNone
};


typedef struct {
	const char* name;
	JSObjectCallAsFunctionCallback callAsFunction;
	JSPropertyAttributes attributes;
} JSStaticFunction;



class V8::TException : public std::exception
{
public:
	TException(v8::Isolate& Isolate,v8::TryCatch& TryCatch,const std::string& Context);
	
	virtual const char* what() const __noexcept
	{
		return mError.c_str();
	}
	
public:
	std::string		mError;
};


//	temp class to see that if we manually control life time of persistent if it doesnt get deallocated on garbage cleanup
//	gr: I think in the use case (a lambda) it becomes const so won't get freed anyway?
template<typename TYPE>
class V8::TPersistent
{
public:
	TPersistent(v8::Isolate& Isolate,v8::Local<TYPE>& Local)
	{
		/*
		 Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
		 PersistentHandle.Reset( &Isolate, LocalHandle );
		 return PersistentHandle;
		 */
		mPersistent.Reset( &Isolate, Local );
	}
	~TPersistent()
	{
		//	gr: seems like we need this... the persistent policy should mean we don't...
		//	gotta release persistents, or we end up running out of handles
		mPersistent.Reset();
		//std::Debug << "V8Storage<" << Soy::GetTypeName<TYPE>() << " released" << std::endl;
	}
	
	v8::Local<TYPE>		GetLocal(v8::Isolate& Isolate)
	{
		return v8::Local<TYPE>::New( &Isolate, mPersistent );
	}
	v8::Persistent<TYPE>	mPersistent;
};

template<typename TYPE>
inline std::shared_ptr<V8::TPersistent<TYPE>> V8::GetPersistent(v8::Isolate& Isolate,v8::Local<TYPE> LocalHandle)
{
	auto ResolverPersistent = std::make_shared<V8::TPersistent<TYPE>>( Isolate, LocalHandle );
	return ResolverPersistent;
}



class V8::TAllocator: public v8::ArrayBuffer::Allocator
{
public:
	TAllocator(const char* Name="V8::TAllocator") :
		mHeap	( true, true, Name )
	{
	}
	
	virtual void*	Allocate(size_t length) override;
	virtual void*	AllocateUninitialized(size_t length) override;
	virtual void	Free(void* data, size_t length) override;
	
public:
	prmem::Heap		mHeap;
};


class V8::TVirtualMachine
{
public:
	TVirtualMachine(std::nullptr_t Null)	{};
	TVirtualMachine(const std::string& RuntimePath);
	~TVirtualMachine()
	{
	}
	
	void	ExecuteInIsolate(std::function<void(v8::Isolate&)> Functor);
	bool	ProcessJobQueue(std::function<void(std::chrono::milliseconds)>& Sleep);

public:
	std::unique_ptr<v8::Platform>	mPlatform;
	//std::shared_ptr<TV8Inspector>	mInspector;
	v8::Isolate*					mIsolate = nullptr;		//	there is no delete for an isolate, so it's a naked pointer
	std::shared_ptr<V8::TAllocator>	mAllocator;
};

//	common wrapper for Local<>
template<typename V8TYPE>
class V8::TLocalRef
{
public:
	TLocalRef()	{}
	TLocalRef(const v8::Local<V8TYPE>& Local) :
		mThis	( Local )
	{
	}
	TLocalRef(const v8::Local<V8TYPE>&& Local) :
		mThis	( Local )
	{
	}
	TLocalRef(const TLocalRef& That) :
		mThis	( That.mThis )
	{
	}

	bool	operator!=(const V8::TLocalRef<V8TYPE>& That) const	{	return mThis != That.mThis;	}
	bool	operator!=(std::nullptr_t Null) const			{	return !mThis.IsEmpty();	}
	operator bool() const									{	return !mThis.IsEmpty();	}

	v8::Local<v8::Value>	GetValue()		{	return mThis.template As<v8::Value>();	}

public:
	v8::Local<V8TYPE>	mThis;
};

class JSContextGroupRef;


class JSContextRef : public V8::TLocalRef<v8::Context>
{
public:
	JSContextRef(std::nullptr_t)	{}
	JSContextRef(v8::Local<v8::Context>& Local);
	JSContextRef(v8::Local<v8::Context>&& Local);

	//void	operator=(std::nullptr_t Null);
	v8::Isolate&		GetIsolate();
	JsCore::TContext&	GetContext();
	void				SetContext(JsCore::TContext& Context);
	v8::TryCatch&		GetTryCatch();
	
	v8::TryCatch*		mTryCatch = nullptr;
};

class JSGlobalContextRef;

//	this is the virtual machine
//	if we use shared ptr's i think we're okay just passing it around
class JSContextGroupRef
{
public:
	JSContextGroupRef(std::nullptr_t);
	JSContextGroupRef(const std::string& RuntimePath);
	
	operator bool() const	{	return mVirtualMachine!=nullptr;	}
	
	void					CreateContext(JSGlobalContextRef& NewContext);
	V8::TVirtualMachine&	GetVirtualMachine()	{	return *mVirtualMachine;	}
	
	std::shared_ptr<V8::TVirtualMachine>	mVirtualMachine;
};


//	actual persistent context
class JSGlobalContextRef
{
public:
	JSGlobalContextRef(std::nullptr_t)	{}
	
	operator bool() const			{	return mContext!=nullptr;	}

	std::shared_ptr<V8::TPersistent<v8::Context>>	mContext;
	V8::TVirtualMachine&	GetVirtualMachine();
	void					ExecuteInContext(std::function<void(JSContextRef&)> Functor);
	v8::Isolate&			GetIsolate()	{	return *GetVirtualMachine().mIsolate;	}
public:
	JSContextGroupRef	mParent = nullptr;
	std::string			mName;
};




class JSObjectRef : public V8::TLocalRef<v8::Object>
{
public:
	JSObjectRef(std::nullptr_t)	{}
	JSObjectRef(v8::Local<v8::Object>& Local);
	JSObjectRef(v8::Local<v8::Object>&& Local);
	/*
	JSObjectRef(const JSObjectRef& That) :
		LocalRef	( That.mThis )
	{
	}
*/
	//void			operator=(std::nullptr_t Null);
	//void			operator=(JSObjectRef That);
	//bool			operator!=(std::nullptr_t Null) const;
	//bool			operator!=(const JSObjectRef& That) const;
	operator 		bool() const						{	return !mThis.IsEmpty();	}
	v8::Isolate&	GetIsolate()	{	return *mThis->GetIsolate();	}
};



class JSValueRef : public V8::TLocalRef<v8::Value>
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



class JSStringRef : public V8::TLocalRef<v8::String>
{
public:
	JSStringRef(std::nullptr_t)	{}
	JSStringRef(v8::Local<v8::String>& Local);
	JSStringRef(v8::Local<v8::String>&& Local) : JSStringRef	( Local )	{}
	JSStringRef(JSContextRef Context,const std::string& String);
	JSStringRef(v8::Isolate& Isolate,const std::string& String);

	void			operator=(std::nullptr_t Null);
	//bool	operator!=(std::nullptr_t Null) const;
	operator		bool() const					{	return !mThis.IsEmpty();	}
	
	std::string		GetString(JSContextRef Context);
};


class JSClassRef
{
public:
	JSClassRef(std::nullptr_t)	{}
	
	operator bool	() const	{	return mTemplate != nullptr;	}
	
	JSObjectFinalizeCallback								mDestructor = nullptr;
	std::shared_ptr<V8::TPersistent<v8::FunctionTemplate>>	mConstructor;
	std::shared_ptr<V8::TPersistent<v8::ObjectTemplate>>	mTemplate;
};



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

JSObjectRef	JSObjectMakeArray(JSContextRef Context,size_t ElementCount,const JSValueRef* Elements,JSValueRef* Exception);
bool		JSValueIsArray(JSContextRef Context,JSValueRef Value);
JSTypedArrayType	JSValueGetTypedArrayType(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
JSObjectRef	JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef Context,JSTypedArrayType ArrayType,void* Buffer,size_t BufferSize,JSTypedArrayBytesDeallocator Dealloc,void* DeallocContext,JSValueRef* Exception);
void*		JSObjectGetTypedArrayBytesPtr(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception);
size_t		JSObjectGetTypedArrayByteOffset(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception);
size_t		JSObjectGetTypedArrayLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception);
size_t		JSObjectGetTypedArrayByteLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception);

JSValueRef			JSEvaluateScript(JSContextRef Context,JSStringRef Source,JSObjectRef This,JSStringRef Filename,int LineNumber,JSValueRef* Exception);
JSGlobalContextRef	JSContextGetGlobalContext(JSContextRef Context);
JSObjectRef			JSContextGetGlobalObject(JSContextRef Context);
JSContextGroupRef	JSContextGroupCreate(const std::string& RuntimeDirectory);
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
JSStringRef	JSValueToStringCopy(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
JSValueRef	JSValueMakeString(JSContextRef Context,JSStringRef String);
void		JSStringRelease(JSStringRef String);

JSClassRef	JSClassCreate(JSContextRef Context,JSClassDefinition* Definition);
void		JSClassRetain(JSClassRef Class);




template<typename V8TYPE>
inline void V8::IsOkay(v8::MaybeLocal<V8TYPE> Result,v8::Isolate& Isolate,v8::TryCatch& TryCatch,const std::string& Context)
{
	//	valid result
	if ( !Result.IsEmpty() )
		return;

	throw V8::TException( Isolate, TryCatch, Context );
}
