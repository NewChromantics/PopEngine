#pragma once

#include <JavaScriptCore/JavaScriptCore.h>
#include <memory>
#include "HeapArray.hpp"
#include "SoyLib/src/SoyThread.h"


namespace Bind
{
	class TInstance;
}

//	https://karhm.com/JavaScriptCore_C_API/
namespace JsCore
{
	typedef Bind::TInstance TInstance;
	//class TInstance;	//	vm
	class TContext;
	class TJobQueue;	//	thread of js-executions
	class TCallback;	//	function parameters
	
	class TObject;
	class TFunction;
	class TPromise;
	class TArray;
	class TPersistent;	//	basically a refcounter, currently explicitly for objects&functions

	class TTemplate;
	template<const char* TYPENAME,class TYPE>
	class TObjectWrapper;
	class TObjectWrapperBase;

	TContext&	GetContext(JSContextRef ContextRef);
	
	//	value conversion - maybe should be type orientated instead of named
	template<typename TYPE>
	TYPE		FromValue(JSContextRef Context,JSValueRef Handle);
	std::string	GetString(JSContextRef Context,JSValueRef Handle);
	std::string	GetString(JSContextRef Context,JSStringRef Handle);
	float		GetFloat(JSContextRef Context,JSValueRef Handle);
	bool		GetBool(JSContextRef Context,JSValueRef Handle);
	template<typename INTTYPE>
	INTTYPE		GetInt(JSContextRef Context,JSValueRef Handle);

	//	create JS types
	template<typename TYPE>
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<TYPE>& Array);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<JSValueRef>& Values);

	//	typed arrays
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<uint8_t>& Values);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<float>& Values);

	JSStringRef	GetString(JSContextRef Context,const std::string& Value);
	JSObjectRef	GetObject(JSContextRef Context,JSValueRef Value);

	//	gr: consider templating this so that we can static_assert on non-specified implementation to avoid the auto-resolution to bool
	JSValueRef	GetValue(JSContextRef Context,const std::string& Value);
	JSValueRef	GetValue(JSContextRef Context,float Value);
	JSValueRef	GetValue(JSContextRef Context,size_t Value);
	JSValueRef	GetValue(JSContextRef Context,uint32_t Value);
	JSValueRef	GetValue(JSContextRef Context,int32_t Value);
	JSValueRef	GetValue(JSContextRef Context,bool Value);
	JSValueRef	GetValue(JSContextRef Context,uint8_t Value);
	JSValueRef	GetValue(JSContextRef Context,JSObjectRef Value);
	inline JSValueRef	GetValue(JSContextRef Context,JSValueRef Value)	{	return Value;	}
	JSValueRef	GetValue(JSContextRef Context,const TPersistent& Value);
	JSValueRef	GetValue(JSContextRef Context,const TPromise& Value);
	JSValueRef	GetValue(JSContextRef Context,const TObject& Value);
	JSValueRef	GetValue(JSContextRef Context,const TArray& Value);
	template<typename TYPE>
	JSValueRef	GetValue(JSContextRef Context,const ArrayBridge<TYPE>& Array);

	
	//	is something we support as a TArray
	bool		IsArray(JSContextRef Context,JSValueRef Handle);
	bool		IsFunction(JSContextRef Context,JSValueRef Handle);

	//	throw c++ exception if the exception object is an exception
	void		ThrowException(JSContextRef Context,JSValueRef ExceptionHandle,const std::string& ThrowContext=std::string());

	//	enum array supports single objects as well as arrays, so we can enumerate a single float into an array of one, as well as an array
	template<typename TYPE>
	void		EnumArray(JSContextRef Context,JSValueRef Value,ArrayBridge<TYPE>& Array);
	
	prmem::Heap&	GetGlobalObjectHeap();
}


//	preparing for virtuals, anything with this, we expect to overide at some point
#define bind_override

//#define RETAIN_FUNCTION
#define RETAIN_WRAPPER_HANDLE


#define DEFINE_FROM_VALUE(TYPE,FUNCNAME)	\
	template<> inline TYPE JsCore::FromValue<TYPE>(JSContextRef Context,JSValueRef Handle)	{	return FUNCNAME( Context, Handle );	}
DEFINE_FROM_VALUE( bool, GetBool );
DEFINE_FROM_VALUE( int32_t, GetInt<int32_t> );
DEFINE_FROM_VALUE( uint32_t, GetInt<uint32_t> );
DEFINE_FROM_VALUE( uint8_t, GetInt<uint8_t> );
DEFINE_FROM_VALUE( std::string, GetString );
DEFINE_FROM_VALUE( float, GetFloat );

template<typename TYPE>
inline TYPE JsCore::FromValue(JSContextRef Context,JSValueRef Handle)
{
	//	if we use static_assert(true), it asserts at definition,
	//	we need to assert at instantiation (maybe it's because of the use of TYPE?)
	//	https://stackoverflow.com/a/17679382/355753
	static_assert( sizeof(TYPE) == -1, "This type needs to be specialised with DEFINE_FROM_VALUE" );
}




class JsCore::TArray
{
public:
	TArray(JSContextRef Context,JSValueRef Value) : TArray( Context, GetObject( Context, Value ) )	{}
	TArray(JSContextRef Context,JSObjectRef Object);

	void		Set(size_t Index,JsCore::TObject& Object);
	template<typename TYPE>
	void		CopyTo(ArrayBridge<TYPE>&& Values)		{	CopyTo( Values );	}
	void		CopyTo(ArrayBridge<uint32_t>& Values);
	void		CopyTo(ArrayBridge<int32_t>& Values);
	void		CopyTo(ArrayBridge<uint8_t>& Values);
	void		CopyTo(ArrayBridge<float>& Values);
	void		CopyTo(ArrayBridge<bool>& Values);

public:
	JSContextRef	mContext = nullptr;
	JSObjectRef		mThis = nullptr;
};

class JsCore::TFunction
{
	friend class TPersistent;
public:
	TFunction()		{}
	TFunction(JSContextRef Context,JSValueRef Value);
	~TFunction();
#if defined(RETAIN_FUNCTION)
	TFunction(const TFunction& That);

	TFunction&		operator=(const TFunction& That);
#endif
	
	//operator		bool() const	{	return mThis != nullptr;	}
	
	//	would be nice to capture return, but it's contained inside Params for now. Maybe template & error for type mismatch
	void			Call(JsCore::TCallback& Params) const;
	void			Call(JsCore::TObject& This) const;
	JSValueRef		Call(JSObjectRef This=nullptr,JSValueRef Value=nullptr) const;
	
public:
	JSContextRef	mContext = nullptr;
	JSObjectRef		mThis = nullptr;
};


//	VM to contain multiple contexts/containers
class Bind::TInstance
{
public:
	TInstance(const std::string& RootDirectory,const std::string& ScriptFilename);
	~TInstance();
	
	std::shared_ptr<JsCore::TContext>	CreateContext(const std::string& Name);
	
private:
	//	when the group is created it does async jobs on that thread's run loop
	//	for deadlock reasons we don't want that to be the main thread (opengl calls get stuck)
	//	so create it on some other thread and it'll use that runloop
	SoyWorkerJobThread	mContextGroupThread;
	
	JSContextGroupRef	mContextGroup = nullptr;
	std::string			mRootDirectory;
	
	std::shared_ptr<JsCore::TContext>	mContext;
};

class JsCore::TJobQueue : public SoyWorkerJobThread
{
public:
	TJobQueue(JsCore::TContext& Context) :
		SoyWorkerJobThread	( "JsCore::TJobQueue" ),
		mContext			( Context )
	{
	}
	
	JsCore::TContext&	mContext;
};


class JsCore::TCallback //: public JsCore::TCallback
{
public:
	TCallback(TContext& Context) :
		//JsCore::TCallback	( Context ),
		mContext		( Context )
	{
	}
	
	virtual size_t			GetArgumentCount() bind_override	{	return mArguments.GetSize();	}
	virtual std::string		GetArgumentString(size_t Index) bind_override;
	std::string				GetArgumentFilename(size_t Index);
	virtual bool			GetArgumentBool(size_t Index) bind_override;
	virtual int32_t			GetArgumentInt(size_t Index) bind_override	{	return JsCore::GetInt<int32_t>( GetContextRef(), mArguments[Index] );	}
	virtual float			GetArgumentFloat(size_t Index) bind_override;
	virtual JsCore::TFunction	GetArgumentFunction(size_t Index) bind_override;
	virtual JsCore::TArray	GetArgumentArray(size_t Index) bind_override;
	virtual TObject			GetArgumentObject(size_t Index) bind_override;
	template<typename TYPE>
	TYPE&					GetArgumentPointer(size_t Index);
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<bool>&& Array) bind_override		{	EnumArray( GetContextRef(), GetArgumentValue(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<uint32_t>&& Array) bind_override	{	EnumArray( GetContextRef(), GetArgumentValue(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<int32_t>&& Array) bind_override	{	EnumArray( GetContextRef(), GetArgumentValue(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Array) bind_override	{	EnumArray( GetContextRef(), GetArgumentValue(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<float>&& Array) bind_override		{	EnumArray( GetContextRef(), GetArgumentValue(Index), Array );	}
	
	
	template<typename TYPE>
	TYPE&					This();
	virtual TObject			ThisObject() bind_override;

	virtual bool			IsArgumentString(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeString;	}
	virtual bool			IsArgumentBool(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeBoolean;	}
	virtual bool			IsArgumentUndefined(size_t Index)bind_override	{	return GetArgumentType(Index) == kJSTypeUndefined;	}
	virtual bool			IsArgumentArray(size_t Index)bind_override		{	return IsArray( GetContextRef(), GetArgumentValue(Index) );	}
	virtual bool			IsArgumentFunction(size_t Index)bind_override	{	return IsFunction( GetContextRef(), GetArgumentValue(Index) );	}
	virtual bool			IsArgumentObject(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeObject;	}

	virtual void			Return() bind_override							{	return ReturnUndefined();	}
	void					ReturnUndefined() bind_override;
	virtual void			ReturnNull() bind_override;
	virtual void			Return(const std::string& Value) bind_override	{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(bool Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(size_t Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(uint32_t Value) bind_override			{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(JsCore::TObject& Value) bind_override		{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(JSValueRef Value) bind_override			{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(JSObjectRef Value) bind_override			{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(JsCore::TArray& Value) bind_override		{	mReturn = GetValue( GetContextRef(), Value.mThis );	}
	virtual void			Return(JsCore::TPromise& Value) bind_override;
	virtual void			Return(JsCore::TPersistent& Value) bind_override;
	template<typename TYPE>
	inline void				Return(ArrayBridge<TYPE>&& Values) bind_override	{	mReturn = GetArray( GetContextRef(), Values );	}

	//	functions for c++ calling JS
	virtual void			SetThis(JsCore::TObject& This) bind_override;
	virtual void			SetArgumentString(size_t Index,const std::string& Value) bind_override;
	virtual void			SetArgumentInt(size_t Index,uint32_t Value) bind_override;
	virtual void			SetArgumentObject(size_t Index,JsCore::TObject& Value) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<std::string>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<float>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,JsCore::TArray& Value) bind_override;

	virtual bool			GetReturnBool() bind_override			{	return GetBool( GetContextRef(), mReturn );	}
	
private:
	JSContextRef			GetContextRef();
	JSType					GetArgumentType(size_t Index);
	JSValueRef				GetArgumentValue(size_t Index);
	
public:
	TContext&			mContext;
	JSValueRef			mThis = nullptr;
	JSValueRef			mReturn = nullptr;
	Array<JSValueRef>	mArguments;
};


class JsCore::TTemplate //: public JsCore::TTemplate
{
	friend JsCore::TContext;
public:
	TTemplate()	{}
	TTemplate(TContext& Context,const std::string& Name) :
		mName		( Name ),
		mContext	( &Context )
	{
	}
	
	bool			operator==(const std::string& Name) const	{	return mName == Name;	}

	template<const char* FUNCTIONNAME>
	void			BindFunction(std::function<void(JsCore::TCallback&)> Function);
	void			RegisterClassWithContext(TContext& Context,const std::string& ParentObjectName);

	JsCore::TObjectWrapperBase&	AllocInstance()		{	return mAllocator();	}
	
public:
	JSClassDefinition	mDefinition = kJSClassDefinitionEmpty;

private:
	std::string			mName;
	JSClassRef			mClass = nullptr;
	TContext*			mContext = nullptr;
	Array<JSStaticFunction>	mFunctions;
	std::function<JsCore::TObjectWrapperBase&()>	mAllocator;
};

//	make this generic for v8 & jscore
//	it should also be a Soy::TUniform type
class JsCore::TObject //: public JsCore::TObject
{
	friend class TPersistent;
	friend class TPromise;
public:
	TObject()	{}	//	for arrays
	TObject(JSContextRef Context,JSObjectRef This);	//	if This==null then it's the global
	
	template<typename TYPE>
	inline TYPE&			This()	{	return This<TYPE>(mThis);	}
	template<typename TYPE>
	static TYPE&			This(JSObjectRef Object);

	virtual bool			HasMember(const std::string& MemberName) bind_override;
	
	virtual JsCore::TObject	GetObject(const std::string& MemberName) bind_override;
	virtual std::string		GetString(const std::string& MemberName) bind_override;
	virtual uint32_t		GetInt(const std::string& MemberName) bind_override;
	virtual float			GetFloat(const std::string& MemberName) bind_override;
	virtual JsCore::TFunction	GetFunction(const std::string& MemberName) bind_override;
	virtual bool			GetBool(const std::string& MemberName) bind_override;

	virtual void			SetObject(const std::string& Name,const JsCore::TObject& Object) bind_override;
	virtual void			SetFunction(const std::string& Name,JsCore::TFunction& Function) bind_override;
	virtual void			SetFloat(const std::string& Name,float Value) bind_override;
	virtual void			SetString(const std::string& Name,const std::string& Value) bind_override;
	virtual void			SetBool(const std::string& Name,bool Value) bind_override;
	virtual void			SetInt(const std::string& Name,uint32_t Value) bind_override;
	virtual void			SetArray(const std::string& Name,JsCore::TArray& Array) bind_override;
	template<typename TYPE>
	inline void				SetArray(const std::string& Name,ArrayBridge<TYPE>&& Values) bind_override
	{
		auto Array = JsCore::GetArray( mContext, Values );
		SetMember( Name, Array );
	}

	//	Jscore specific
private:
	JSValueRef		GetMember(const std::string& MemberName);
	void			SetMember(const std::string& Name,JSValueRef Value);
	JSContextRef	mContext = nullptr;

public:
	JSObjectRef		mThis = nullptr;
};



class JsCore::TPersistent
{
public:
	TPersistent()	{}
	TPersistent(const TPersistent& That)	{	Retain( That );	}
	TPersistent(const TPersistent&& That)	{	Retain( That );	}
	TPersistent(const TObject& Object)		{	Retain( Object );	}
	TPersistent(const TFunction& Object)	{	Retain( Object );	}
	~TPersistent();							//	dec refound
	
	operator		bool() const		{	return IsFunction() || IsObject();	}
	bool			IsFunction() const	{	return mFunction.mThis != nullptr;	}
	bool			IsObject() const	{	return mObject.mThis != nullptr;	}
	
	//	const for lambda[=] capture
	TObject			GetObject() const		{	return mObject;	}
	TFunction		GetFunction() const		{	return mFunction;	}
	JSContextRef	GetContext() const;
	
	TPersistent&	operator=(const TPersistent& That)	{	Retain(That);	return *this;	}
	
private:
	void		Retain(const TObject& Object);
	void		Retain(const TFunction& Object);
	void		Retain(const TPersistent& That);
	
public:
	TObject		mObject;
	TFunction	mFunction;
};


//	functions marked virtual need to become generic
class JsCore::TContext //: public JsCore::TContext
{
public:
	TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory);
	~TContext();
	
	virtual void		LoadScript(const std::string& Source,const std::string& Filename) bind_override;
	virtual void		Execute(std::function<void(TContext&)> Function) bind_override;
	virtual void		Queue(std::function<void(TContext&)> Function,size_t DeferMs=0) bind_override;
	virtual void		GarbageCollect();
	
	template<const char* FunctionName>
	void				BindGlobalFunction(std::function<void(JsCore::TCallback&)> Function,const std::string& ParentName=std::string());
	
	JsCore::TObject			GetGlobalObject(const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object
	virtual void			CreateGlobalObjectInstance(const std::string&  ObjectType,const std::string& Name) bind_override;
	virtual JsCore::TObject	CreateObjectInstance(const std::string& ObjectTypeName=std::string());
	JsCore::TObject			CreateObjectInstance(const std::string& ObjectTypeName,ArrayBridge<JSValueRef>&& ConstructorArguments);
	
	virtual JsCore::TPersistent	CreatePersistent(JsCore::TObject& Object) bind_override;
	virtual std::shared_ptr<JsCore::TPersistent>	CreatePersistentPtr(JsCore::TObject& Object) bind_override;
	virtual JsCore::TPersistent	CreatePersistent(JsCore::TFunction& Object) bind_override;
	virtual JsCore::TPromise		CreatePromise(const std::string& DebugName) bind_override;
	virtual JsCore::TArray	CreateArray(size_t ElementCount,std::function<std::string(size_t)> GetElement) bind_override;
	virtual JsCore::TArray	CreateArray(size_t ElementCount,std::function<TObject(size_t)> GetElement) bind_override;
	virtual JsCore::TArray	CreateArray(size_t ElementCount,std::function<TArray(size_t)> GetElement) bind_override;
	virtual JsCore::TArray	CreateArray(size_t ElementCount,std::function<int32_t(size_t)> GetElement) bind_override;
	virtual JsCore::TArray	CreateArray(size_t ElementCount);
	template<typename TYPE>
	JsCore::TArray			CreateArray(ArrayBridge<TYPE>&& Values);
	JsCore::TArray			CreateArray(ArrayBridge<uint8_t>&& Values);
	JsCore::TArray			CreateArray(ArrayBridge<float>&& Values);
	
	
	template<typename OBJECTWRAPPERTYPE>
	void				BindObjectType(const std::string& ParentName=std::string());
	
	//	api calls with context provided
	//template<typename IN,typename OUT>
	//OUT					GetString(IN Handle)			{	return JsCore::GetString(mContext,Handle);	}
	void				ThrowException(JSValueRef ExceptionHandle,const std::string& ErrorContext="JsCore exception")	{	JsCore::ThrowException( mContext, ExceptionHandle, ErrorContext );	}
	
	
	
	prmem::Heap&		GetObjectHeap()		{	return GetGeneralHeap();	}
	prmem::Heap&		GetImageHeap()		{	return mImageHeap;	}
	prmem::Heap&		GetGeneralHeap()	{	return JsCore::GetGlobalObjectHeap();	}
	std::string			GetResolvedFilename(const std::string& Filename);
	
	//	this can almost be static, but TCallback needs a few functions of TContext
	JSValueRef			CallFunc(std::function<void(JsCore::TCallback&)> Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception,const std::string& FunctionContext);
	
	
private:
	void				BindRawFunction(const std::string& FunctionName,const std::string& ParentObjectName,JSObjectCallAsFunctionCallback Function);
	
	
	//JSObjectRef			GetGlobalObject(const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object
	
	
public:
	TInstance&			mInstance;
	JSGlobalContextRef	mContext = nullptr;
	
	prmem::Heap			mImageHeap = prmem::Heap(true,true,"Context Images");
	std::string			mRootDirectory;
	
	//	"templates" in v8, "classes" in jscore
	Array<TTemplate>	mObjectTemplates;
	
	//	no promise type, so this is our promise instantiator
	TPersistent			mMakePromiseFunction;
	
	//	queue for jobs to try and keep non-js threads free and some kinda organisation
	//	although jscore IS threadsafe, so we can execute on other threads, it's not
	//	the same on other systems
	TJobQueue			mJobQueue;
	std::recursive_mutex	mExecuteLock;
};


class JsCore::TPromise
{
public:
	TPromise()	{}
	TPromise(TObject& Promise,TFunction& Resolve,TFunction& Reject,const std::string& DebugName);
	~TPromise();
	
	//	const for lambda[=] copy capture
	void			Resolve(const std::string& Value) const		{	Resolve( GetValue( GetContext(), Value ) );	}
	void			Resolve(JsCore::TObject& Value) const			{	Resolve( GetValue( GetContext(), Value ) );	}
	template<typename TYPE>
	void			Resolve(ArrayBridge<TYPE>&& Values) const	{	Resolve( GetValue( GetContext(), Values ) );	}
	void			Resolve(JsCore::TArray& Value) const			{	Resolve( GetValue( GetContext(), Value ) );	}
	void			Resolve(JSValueRef Value) const;//			{	mResolve.Call(nullptr,Value);	}
	void			ResolveUndefined() const;

	void			Reject(const std::string& Value) const		{	Reject( GetValue( GetContext(), Value ) );	}
	void			Reject(JSValueRef Value) const;//			{	mReject.Call(nullptr,Value);	}
	
private:
	JSContextRef	GetContext() const	{	return mPromise.GetContext();	}
	
public:
	std::string		mDebugName;
	TPersistent		mPromise;
	TPersistent		mResolve;
	TPersistent		mReject;
};




class JsCore::TObjectWrapperBase
{
public:
	TObjectWrapperBase(TContext& Context,TObject& This) :
		mHandle		( This ),
		mContext	( Context )
	{
	}
	virtual ~TObjectWrapperBase()	{}

	virtual TObject	GetHandle();
	virtual void	SetHandle(TObject& NewHandle);
	
	//	construct and allocate
	virtual void 	Construct(TCallback& Arguments)=0;
	
	template<typename TYPE>
	//static TObjectWrapperBase*	Allocate(JsCore::TContext& Context,JsCore::TObject& This)
	static TYPE*	Allocate(TContext& Context,TObject& This)
	{
		return new TYPE( Context, This );
	}
	
	TContext&		GetContext()	{	return mContext;	}	//	owner

protected:
#if defined(RETAIN_WRAPPER_HANDLE)
	TPersistent		mHandle;
#else
	//	gr: this is a weak reference so the object gets free'd
	TObject			mHandle;
#endif
	TContext&		mContext;
};


//	template name? that's right, need unique references.
//	still working on getting rid of that, but still allow dynamic->static function binding
template<const char* TYPENAME,class TYPE>
class JsCore::TObjectWrapper : public JsCore::TObjectWrapperBase
{
public:
	//typedef std::function<TObjectWrapper<TYPENAME,TYPE>*(TV8Container&,v8::Local<v8::Object>)> ALLOCATORFUNC;
	
public:
	TObjectWrapper(TContext& Context,TObject& This) :
		TObjectWrapperBase	( Context, This )
	{
	}
	
	static std::string		GetTypeName()	{	return TYPENAME;	}
	
	static TTemplate 		AllocTemplate(JsCore::TContext& Context,std::function<TObjectWrapperBase*(JSObjectRef)> AllocWrapper);
	
protected:
	static void				Free(JSObjectRef ObjectRef)
	{
		//	free the void
		//	cast to TObject and use This to do proper type checks
		//std::Debug << "Free object of type " << TYPENAME << std::endl;
		auto& Object = TObject::This<TYPE>( ObjectRef );
		auto* pObject = &Object;
		
		auto& Heap = JsCore::GetGlobalObjectHeap();
		Heap.Free(pObject);
		
		//	reset the void for safety?
		JSObjectSetPrivate( ObjectRef, nullptr );
	}
	
protected:
	std::shared_ptr<TYPE>			mObject;
};


template<const char* TYPENAME,class TYPE>
inline JsCore::TTemplate JsCore::TObjectWrapper<TYPENAME,TYPE>::AllocTemplate(JsCore::TContext& Context,std::function<TObjectWrapperBase*(JSObjectRef)> AllocWrapper)
{
	static std::function<TObjectWrapperBase*(JSObjectRef)> AllocWrapperCache;
	if ( AllocWrapperCache != nullptr )
		throw Soy::AssertException("This allocator is already bound. Duplicate string?");
	AllocWrapperCache = AllocWrapper;
	
	//	setup constructor CFunc here
	static JSObjectCallAsConstructorCallback CConstructorFunc = [](JSContextRef ContextRef,JSObjectRef constructor,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef* Exception)
	{
		try
		{
			//	gr: constructor here, is this function.
			//		we need to create a new object and return it
			auto& Context = JsCore::GetContext( ContextRef );
			auto ArgumentsArray = GetRemoteArray( Arguments, ArgumentCount );
			auto ThisObject = Context.CreateObjectInstance( TYPENAME, GetArrayBridge(ArgumentsArray) );
			auto This = ThisObject.mThis;
			return This;
		}
		catch(std::exception& e)
		{
			std::stringstream Error;
			Error << TYPENAME << "() constructor exception: " << e.what();
			*Exception = GetValue( ContextRef, Error.str() );
			//	we HAVE to return an object, but NULL is a value, not an object :/
			auto NullObject = JSObjectMake( ContextRef, nullptr, nullptr );
			return NullObject;
		}
	};
	
	//	https://stackoverflow.com/questions/46943350/how-to-use-jsexport-and-javascriptcore-in-c
	TTemplate Template( Context, TYPENAME );
	auto& Definition = Template.mDefinition;
	Definition = kJSClassDefinitionEmpty;

	Definition.className = TYPENAME;
	Definition.attributes = kJSClassAttributeNone;
	Definition.callAsConstructor = CConstructorFunc;
	Definition.finalize = Free;
	
	return Template;
}




template<const char* FunctionName>
inline void JsCore::TContext::BindGlobalFunction(std::function<void(JsCore::TCallback&)> Function,const std::string& ParentName)
{
	//	try and remove context cache
	static std::function<void(JsCore::TCallback&)> FunctionCache;
	if ( FunctionCache != nullptr )
		throw Soy::AssertException("This function is already bound. Duplicate string?");
	FunctionCache = Function;

	
	JSObjectCallAsFunctionCallback CFunc = [](JSContextRef Context,JSObjectRef Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef* Exception)
	{
		auto& ContextPtr = JsCore::GetContext( Context );
		return ContextPtr.CallFunc( FunctionCache, This, ArgumentCount, Arguments, *Exception, FunctionName );
	};
	
	BindRawFunction( FunctionName, ParentName, CFunc );
}



template<typename TYPE>
inline JsCore::TArray JsCore::TContext::CreateArray(ArrayBridge<TYPE>&& Values)
{
	auto GetElement = [&](size_t Index)
	{
		return Values[Index];
	};
	auto Array = CreateArray( Values.GetSize(), GetElement );
	return Array;
}


template<typename TYPE>
inline TYPE& JsCore::TCallback::GetArgumentPointer(size_t Index)
{
	auto Object = GetArgumentObject(Index);
	return Object.This<TYPE>();
}

template<typename TYPE>
inline TYPE& JsCore::TCallback::This()
{
	auto Object = ThisObject();
	return Object.This<TYPE>();
}

template<typename TYPE>
inline TYPE& JsCore::TObject::This(JSObjectRef Object)
{
	auto* This = JSObjectGetPrivate(Object);
	if ( This == nullptr )
		throw Soy::AssertException("Object::This is null");
	auto* Wrapper = reinterpret_cast<TObjectWrapperBase*>( This );
	auto* TypeWrapper = dynamic_cast<TYPE*>( Wrapper );
	return *TypeWrapper;
}



template<typename OBJECTWRAPPERTYPE>
inline void JsCore::TContext::BindObjectType(const std::string& ParentName)
{
	auto AllocWrapper = [this](JSObjectRef This)
	{
		TObject ThisObject( mContext, This );
		auto& Heap = this->GetObjectHeap();
		auto* NewObject = Heap.Alloc<OBJECTWRAPPERTYPE>( *this, ThisObject );
		return NewObject;
	};

	//	create a template that can be overloaded by the type
	auto Template = OBJECTWRAPPERTYPE::AllocTemplate( *this, AllocWrapper );

	Template.mAllocator = [this]() -> TObjectWrapperBase&
	{
		JsCore::TObject Null;
		auto* New = new OBJECTWRAPPERTYPE( *this, Null );
		return *New;
	};
	
	//	init template with overloaded stuff
	OBJECTWRAPPERTYPE::CreateTemplate( Template );
	
	//	finish off
	Template.RegisterClassWithContext( *this, ParentName );
	mObjectTemplates.PushBack( Template );
}

template<typename TYPE>
inline JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<TYPE>& TypeArray)
{
	Array<JSValueRef> Values;
	for ( auto i = 0;	i <TypeArray.GetSize();	i++ )
	{
		auto Value = GetValue(Context, TypeArray[i]);
		Values.PushBack(Value);
	}
	
	//	call GetArrayBridge() in place so it calls the specialised
	auto ArrayObject = GetArray( Context, GetArrayBridge( Values ) );
	return ArrayObject;
}

template<>
inline JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<uint8_t>& Array)
{
	throw Soy::AssertException("Make typed array");
	//	make typed array
}


template<typename INTTYPE>
inline INTTYPE JsCore::GetInt(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	JSValueRef Exception = nullptr;
	auto DoubleJs = JSValueToNumber( Context, Handle, &Exception );
	
	auto Int = static_cast<INTTYPE>( DoubleJs );
	return Int;
}


template<const char* FUNCTIONNAME>
inline void JsCore::TTemplate::BindFunction(std::function<void(JsCore::TCallback&)> Function)
{
	//	try and remove context cache
	static std::function<void(JsCore::TCallback&)> FunctionCache;
	if ( FunctionCache != nullptr )
		throw Soy::AssertException("This function name is already bound. Duplicate string?");
	FunctionCache = Function;

	JSObjectCallAsFunctionCallback CFunc = [](JSContextRef Context,JSObjectRef Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef* Exception)
	{
		auto& ContextPtr = JsCore::GetContext( Context );
		return ContextPtr.CallFunc( FunctionCache, This, ArgumentCount, Arguments, *Exception, FUNCTIONNAME );
	};
	
	JSStaticFunction NewFunction;
	NewFunction.name = FUNCTIONNAME;
	NewFunction.callAsFunction = CFunc;
	NewFunction.attributes = kJSPropertyAttributeNone;
	
	mFunctions.PushBack(NewFunction);
}


template<typename TYPE>
inline JSValueRef JsCore::GetValue(JSContextRef Context,const ArrayBridge<TYPE>& Values)
{
	auto Array = GetArray( Context, Values );
	return GetValue( Context, Array );
}

//	enum array supports single objects as well as arrays, so we can enumerate a single float into an array of one, as well as an array
template<typename TYPE>
inline void JsCore::EnumArray(JSContextRef Context,JSValueRef Value,ArrayBridge<TYPE>& Array)
{
	if ( IsArray( Context, Value ) )
	{
		JsCore::TArray ArrayHandle( Context, Value );
		ArrayHandle.CopyTo(Array);
		return;
	}
	
	//	this needs to support arrays of objects really
	auto SingleValue = GetInt<TYPE>( Context, Value );
	Array.PushBack( SingleValue );
}
