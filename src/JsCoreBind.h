#pragma once

#if defined(JSAPI_V8)


#else
	//	gr: we're binding them ourselves
	#if defined(PLATFORM_WINDOWS)
	#include "JsCoreDll.h"
	#else
	#include <JavaScriptCore/JavaScriptCore.h>
	#endif
#endif

#include <memory>
#include "HeapArray.hpp"
#include "SoyLib/src/SoyThread.h"


namespace Bind
{
	class TInstanceBase;
	class TInstance;
}

//	https://karhm.com/JavaScriptCore_C_API/
namespace JsCore
{
	typedef Bind::TInstance TInstance;
	//class TInstance;		//	vm
	class TLocalContext;	//	limited lifetime/temp context
	class TContext;			//	global context
	class TJobQueue;		//	thread of js-executions
	class TCallback;		//	function parameters
	class TContextDebug;	//	debug meta for a context
	
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
	std::string	GetString(JSStringRef Handle);
	float		GetFloat(JSContextRef Context,JSValueRef Handle);
	bool		GetBool(JSContextRef Context,JSValueRef Handle);
	template<typename INTTYPE>
	INTTYPE		GetInt(JSContextRef Context,JSValueRef Handle);

	//	create JS types
	template<typename TYPE>
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<TYPE>& Array);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<JSValueRef>& Values);
	JSObjectRef	GetArray(JSContextRef Context,size_t Size);	//	create array of undefineds
	
	//	typed arrays
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<uint8_t>& Values);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<uint32_t>& Values);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<float>& Values);

	//	gr: note: this JSStringRef needs explicit releasing (JSStringRelease) if not sent off to JS land
	//		todo: auto releasing string!
	JSStringRef	GetString(JSContextRef Context,const std::string& Value);
	JSStringRef	GetString(const std::string& Value);
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
	JSValueRef	GetValue(JSContextRef Context,const TFunction& Value);
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
//#define RETAIN_WRAPPER_HANDLE	//	a lot less crashing, but still happens


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



class JsCore::TLocalContext
{
public:
	TLocalContext(JSContextRef Local,TContext& Global) :
		mLocalContext	( Local ),
		mGlobalContext	( Global )
	{
	}
	
	JSContextRef	mLocalContext;
	TContext&		mGlobalContext;
};

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
	
public:
	JSObjectRef		mThis = nullptr;
};


//	VM to contain multiple contexts/containers
class Bind::TInstance : public Bind::TInstanceBase
{
public:
	TInstance(const std::string& RootDirectory,const std::string& ScriptFilename,std::function<void(int32_t)> OnShutdown);
	~TInstance();
	
	std::shared_ptr<JsCore::TContext>	CreateContext(const std::string& Name);
	void								DestroyContext(JsCore::TContext& Context);
	void								Shutdown(int32_t ExitCode);

private:
	//	when the group is created it does async jobs on that thread's run loop
	//	for deadlock reasons we don't want that to be the main thread (opengl calls get stuck)
	//	so create it on some other thread and it'll use that runloop
	SoyWorkerJobThread	mContextGroupThread;
	
	JSContextGroupRef	mContextGroup = nullptr;
	std::string			mRootDirectory;
	
	Array<std::shared_ptr<JsCore::TContext>>	mContexts;

	std::function<void(int32_t)>	mOnShutdown;	//	callback when we want to die
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
	TCallback(TLocalContext& Context) :
		//JsCore::TCallback	( Context ),
		mLocalContext	( Context )
	{
	}
	
	virtual size_t			GetArgumentCount() bind_override	{	return mArguments.GetSize();	}
	virtual std::string		GetArgumentString(size_t Index) bind_override;
	std::string				GetArgumentFilename(size_t Index);
	virtual bool			GetArgumentBool(size_t Index) bind_override;
	virtual int32_t			GetArgumentInt(size_t Index) bind_override	{	return JsCore::GetInt<int32_t>( GetContextRef(), GetArgumentValue(Index) );	}
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
	virtual bool			IsArgumentNull(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeNull;	}
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
	inline void				Return(ArrayBridge<TYPE>&& Values) bind_override
	{
		auto Array = GetArray( GetContextRef(), Values );
		auto ArrayValue = GetValue( GetContextRef(), Array );
		mReturn = ArrayValue;
	}

	//	functions for c++ calling JS
	virtual void			SetThis(JsCore::TObject& This) bind_override;
	virtual void			SetArgument(size_t Index,JSValueRef Value) bind_override;
	virtual void			SetArgumentString(size_t Index,const std::string& Value) bind_override;
	virtual void			SetArgumentInt(size_t Index,uint32_t Value) bind_override;
	virtual void			SetArgumentObject(size_t Index,JsCore::TObject& Value) bind_override;
	virtual void			SetArgumentFunction(size_t Index,JsCore::TFunction& Value) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<std::string>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<float>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,JsCore::TArray& Value) bind_override;

	virtual bool			GetReturnBool() bind_override			{	return GetBool( GetContextRef(), mReturn );	}
	virtual TObject			GetReturnObject() bind_override;
	virtual TFunction		GetReturnFunction() bind_override;

	JSContextRef			GetContextRef();

private:
	JSType					GetArgumentType(size_t Index);
	JSValueRef				GetArgumentValue(size_t Index);
	
public:
	TLocalContext&		mLocalContext;
	TContext&			mContext = mLocalContext.mGlobalContext;
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
	void			RegisterClassWithContext(TLocalContext& Context,const std::string& ParentObjectName,const std::string& OverrideLeafName);

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
	friend class TObjectWrapperBase;
public:
	TObject()	{}	//	for arrays
	TObject(JSContextRef Context,JSObjectRef This);	//	if This==null then it's the global
	//	should probbaly block = operator so any copy of an object always has a new Context
	
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
		auto ArrayValue = JsCore::GetValue( mContext, Array );
		SetMember( Name, ArrayValue );
	}

	//	Jscore specific
private:
	JSValueRef		GetMember(const std::string& MemberName);
	void			SetMember(const std::string& Name,JSValueRef Value);

protected:
	//	this should go, but requiring the param for every func is a pain,
	//	so this context should be updated any time TObject is fetched from somewhere
	JSContextRef	mContext = nullptr;

public:
	JSObjectRef		mThis = nullptr;
};



class JsCore::TPersistent
{
public:
	//	gr: can't use && as we need = operator to work
	//TPersistent(const TPersistent&& That)	{	Steal( That );	}
	
	TPersistent()	{}
	TPersistent(const TPersistent& That)	{	Retain( That );	}
	//TPersistent(Bind::TLocalContext& Context,const TPersistent&& That)	{	Retain( Context, That );	}
	TPersistent(Bind::TLocalContext& Context,const TObject& Object,const std::string& DebugName)	{	Retain( Context, Object, DebugName );	}
	TPersistent(Bind::TLocalContext& Context,const TFunction& Object,const std::string& DebugName)	{	Retain( Context, Object, DebugName );	}
	~TPersistent();							//	dec refound
	
	operator		bool() const		{	return IsFunction() || IsObject();	}
	bool			IsFunction() const	{	return mFunction.mThis != nullptr;	}
	bool			IsObject() const	{	return mObject.mThis != nullptr;	}
	const std::string&	GetDebugName() const	{	return mDebugName;	}
	
	//	const for lambda[=] capture
	TObject			GetObject(TLocalContext& Context) const;
	TFunction		GetFunction() const;
	
	TPersistent&	operator=(const TPersistent& That)	{	Retain(That);	return *this;	}
	
private:
	void		Retain(Bind::TLocalContext& Context,const TObject& Object,const std::string& DebugName);
	void		Retain(Bind::TLocalContext& Context,const TFunction& Object,const std::string& DebugName);
	void		Retain(const TPersistent& That);
	void 		Release();

	void		DefferedRetain(const TObject& Object,const std::string& DebugName);
	void		DefferedRetain(const TFunction& Object,const std::string& DebugName);
	
	static void	Release(JSContextRef Context,JSObjectRef ObjectOrFunc,const std::string& DebugName);
	static void	Retain(JSContextRef Context,JSObjectRef ObjectOrFunc,const std::string& DebugName);

protected:
	std::string	mDebugName;

	//	these two make a local context!
	TContext*		mContext = nullptr;	//	hacky atm, storing this for = and deferred release in destructor
	JSContextRef	mRetainedContext = nullptr;	//	which context we retained with

public:
	TObject			mObject;
	TFunction		mFunction;
};


class JsCore::TContextDebug
{
public:
	void	OnPersitentRetained(TPersistent& Persistent);
	void	OnPersitentReleased(TPersistent& Persistent);

	std::map<std::string,int>		mPersistentObjectCount;
	int		mPersistentFunctionCount=0;
};


//	functions marked virtual need to become generic
class JsCore::TContext //: public JsCore::TContext
{
	friend class Bind::TInstance;
public:
	TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory);
	~TContext();
	
	virtual void		LoadScript(const std::string& Source,const std::string& Filename) bind_override;
	virtual void		Execute(std::function<void(TLocalContext&)> Function) bind_override;
	virtual void		Queue(std::function<void(TLocalContext&)> Function,size_t DeferMs=0) bind_override;
	virtual void		GarbageCollect(JSContextRef LocalContext);
	virtual void		Shutdown(int32_t ExitCode);	//	tell instance to destroy us
		
	template<const char* FunctionName>
	void				BindGlobalFunction(std::function<void(JsCore::TCallback&)> Function,const std::string& ParentName=std::string());
	
	JsCore::TObject			GetGlobalObject(TLocalContext& LocalContext,const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object
	virtual void			CreateGlobalObjectInstance(const std::string&  ObjectType,const std::string& Name) bind_override;
	virtual JsCore::TObject	CreateObjectInstance(TLocalContext& LocalContext,const std::string& ObjectTypeName=std::string());
	JsCore::TObject			CreateObjectInstance(TLocalContext& LocalContext,const std::string& ObjectTypeName,ArrayBridge<JSValueRef>&& ConstructorArguments);
	
	virtual JsCore::TPromise	CreatePromise(Bind::TLocalContext& LocalContext,const std::string& DebugName) bind_override;

	
	template<typename OBJECTWRAPPERTYPE>
	void				BindObjectType(const std::string& ParentName=std::string(),const std::string& OverrideLeafName=std::string());
	

	prmem::Heap&		GetObjectHeap()		{	return GetGeneralHeap();	}
	prmem::Heap&		GetImageHeap()		{	return mImageHeap;	}
	prmem::Heap&		GetGeneralHeap()	{	return JsCore::GetGlobalObjectHeap();	}
	std::string			GetResolvedFilename(const std::string& Filename);
	
	//	this can almost be static, but TCallback needs a few functions of TContext
	JSValueRef			CallFunc(TLocalContext& LocalContext,std::function<void(JsCore::TCallback&)> Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception,const std::string& FunctionContext);
	
	
	void				OnPersitentRetained(TPersistent& Persistent)	{	mDebug.OnPersitentRetained(Persistent);	}
	void				OnPersitentReleased(TPersistent& Persistent)	{	mDebug.OnPersitentReleased(Persistent);	}
	
protected:
	void				Cleanup();		//	actual cleanup called by instance & destructor
	void				ReleaseContext();	//	try and release javascript objects

private:
	void				BindRawFunction(const std::string& FunctionName,const std::string& ParentObjectName,JSObjectCallAsFunctionCallback Function);
		
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
	
	TContextDebug		mDebug;
};


class JsCore::TPromise
{
public:
	TPromise()	{}
	TPromise(Bind::TLocalContext& Context,TObject& Promise,TFunction& Resolve,TFunction& Reject,const std::string& DebugName);
	~TPromise();
	
	//	const for lambda[=] copy capture
	void			Resolve(Bind::TLocalContext& Context,const std::string& Value) const		{	Resolve( Context, GetValue( Context.mLocalContext, Value ) );	}
	void			Resolve(Bind::TLocalContext& Context,JsCore::TObject& Value) const			{	Resolve( Context, GetValue( Context.mLocalContext, Value ) );	}
	template<typename TYPE>
	void			Resolve(Bind::TLocalContext& Context,ArrayBridge<TYPE>&& Values) const		{	Resolve( Context, GetValue( Context.mLocalContext, Values ) );	}
	void			Resolve(Bind::TLocalContext& Context,JsCore::TArray& Value) const			{	Resolve( Context, GetValue( Context.mLocalContext, Value ) );	}
	void			Resolve(Bind::TLocalContext& Context,JSValueRef Value) const;//				{	mResolve.Call(nullptr,Value);	}
	void			ResolveUndefined(Bind::TLocalContext& Context) const;

	void			Reject(Bind::TLocalContext& Context,const std::string& Value) const			{	Reject( Context, GetValue( Context.mLocalContext, Value ) );	}
	void			Reject(Bind::TLocalContext& Context,JSValueRef Value) const;//				{	mReject.Call(nullptr,Value);	}
	
protected:
	
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

	virtual TObject	GetHandle(Bind::TLocalContext& Context);
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
	typedef JsCore::TObjectWrapper<TYPENAME,TYPE> THISTYPE;
	//typedef std::function<TObjectWrapper<TYPENAME,TYPE>*(TV8Container&,v8::Local<v8::Object>)> ALLOCATORFUNC;
	
public:
	TObjectWrapper(TContext& Context,TObject& This) :
		TObjectWrapperBase	( Context, This )
	{
	}
	
	static std::string		GetTypeName()	{	return TYPENAME;	}
	
	static TTemplate 		AllocTemplate(JsCore::TContext& Context);
	
protected:
	static void				Free(JSObjectRef ObjectRef)
	{
		//	gr: if this fails as it's null, the object being cleaned up may be the class/constructor, if it isn't attached to anything (ie. not attached to the global!)
		//		we shouldn't really have our own constructors being deleted!
		//	free the void
		//	cast to TObject and use This to do proper type checks
		//std::Debug << "Free object of type " << TYPENAME << std::endl;
		auto& Object = TObject::This<THISTYPE>( ObjectRef );
		auto* pObject = &Object;
		
		auto& Heap = JsCore::GetGlobalObjectHeap();
		if ( !Heap.Free(pObject) )
			std::Debug << "Global Heap failed to Free() " << Soy::GetTypeName<THISTYPE>() << std::endl;
		
		//	reset the void for safety?
		//std::Debug << "ObjectRef=" << ObjectRef << "(" << TYPENAME << ") to null" << std::endl;
		JSObjectSetPrivate( ObjectRef, nullptr );
	}
	
protected:
	std::shared_ptr<TYPE>			mObject;
};


template<const char* TYPENAME,class TYPE>
inline JsCore::TTemplate JsCore::TObjectWrapper<TYPENAME,TYPE>::AllocTemplate(JsCore::TContext& Context)
{
	//	setup constructor CFunc here
	static JSObjectCallAsConstructorCallback CConstructorFunc = [](JSContextRef ContextRef,JSObjectRef constructor,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef* Exception)
	{
		try
		{
			//	gr: constructor here, is this function.
			//		we need to create a new object and return it
			auto& Context = JsCore::GetContext( ContextRef );
			TLocalContext LocalContext( ContextRef, Context );
			auto ArgumentsArray = GetRemoteArray( Arguments, ArgumentCount );
			auto ThisObject = Context.CreateObjectInstance( LocalContext, TYPENAME, GetArrayBridge(ArgumentsArray) );
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
		TLocalContext LocalContext( Context, ContextPtr );
		return ContextPtr.CallFunc( LocalContext, FunctionCache, This, ArgumentCount, Arguments, *Exception, FunctionName );
	};
	
	BindRawFunction( FunctionName, ParentName, CFunc );
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
	if ( !TypeWrapper )
		throw Soy::AssertException("Failed to dynamically object pointer to " + Soy::GetTypeName<TYPE>() );
	return *TypeWrapper;
}



template<typename OBJECTWRAPPERTYPE>
inline void JsCore::TContext::BindObjectType(const std::string& ParentName,const std::string& OverrideLeafName)
{
	//	create a template that can be overloaded by the type
	auto Template = OBJECTWRAPPERTYPE::AllocTemplate( *this );

	Template.mAllocator = [this]() -> TObjectWrapperBase&
	{
		JsCore::TObject ThisObject;
		auto& Heap = this->GetObjectHeap();
		auto* NewObject = Heap.Alloc<OBJECTWRAPPERTYPE>( *this, ThisObject );
		return *NewObject;
	};
	
	//	catch duplicate class names, javascript lets us make them, but they'll just overwrite each other
	//	gr: can we name them with the heiarchy as part of the name?
	{
		for ( auto t=0;	t<mObjectTemplates.GetSize();	t++ )
		{
			auto& MatchTemplate = mObjectTemplates[t];
			if ( std::string(Template.mDefinition.className) != std::string(MatchTemplate.mDefinition.className) )
				continue;
			
			std::stringstream Error;
			Error << "Trying to bind duplicate class name " << Template.mDefinition.className << " (match: " << MatchTemplate.mDefinition.className << ")";
			throw Soy::AssertException(Error);
		}
	}

	
	//	init template with overloaded stuff
	OBJECTWRAPPERTYPE::CreateTemplate( Template );
	
	auto Exec = [&](Bind::TLocalContext& LocalContext)
	{
		//	finish off
		Template.RegisterClassWithContext( LocalContext, ParentName, OverrideLeafName );
		mObjectTemplates.PushBack( Template );
	};
	Execute( Exec );
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


template<typename INTTYPE>
inline INTTYPE JsCore::GetInt(JSContextRef Context,JSValueRef Handle)
{
	if ( !JSValueIsNumber( Context,Handle ) )
	{
		std::stringstream Error;
		Error << "Trying to convert value to number, but isn't";
		throw Soy::AssertException(Error.str());
	}
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
		TLocalContext LocalContext( Context, ContextPtr );
		return ContextPtr.CallFunc( LocalContext, FunctionCache, This, ArgumentCount, Arguments, *Exception, FUNCTIONNAME );
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
