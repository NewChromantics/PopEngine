#pragma once


#if defined(JSAPI_V8)

//	this should always be a persistent now, but need a "make it weak" approach
#define PERSISTENT_OBJECT_HANDLE

#elif defined(JSAPI_CHAKRA)

#define PROTECT_OBJECT_THIS
#define PERSISTENT_OBJECT_HANDLE


#elif defined(JSAPI_JSCORE)
	//	gr: we're binding them ourselves
	#if defined(PLATFORM_WINDOWS)
	#include "JsCoreDll.h"
	#else
	#include <JavaScriptCore/JavaScriptCore.h>
	#endif
#else

#error No Javascript API defined

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
	void		GetString(JSContextRef Context,JSStringRef Handle,ArrayBridge<char>&& Buffer);
	float		GetFloat(JSContextRef Context,JSValueRef Handle);
	bool		GetBool(JSContextRef Context,JSValueRef Handle);
	template<typename INTTYPE>
	INTTYPE		GetInt(JSContextRef Context,JSValueRef Handle);
	uint8_t*	GetPointer_u8(JSContextRef Context,JSValueRef Handle);
	uint16_t*	GetPointer_u16(JSContextRef Context,JSValueRef Handle);
	uint32_t*	GetPointer_u32(JSContextRef Context,JSValueRef Handle);
	uint64_t*	GetPointer_u64(JSContextRef Context,JSValueRef Handle);
	int8_t*		GetPointer_s8(JSContextRef Context,JSValueRef Handle);
	int16_t*	GetPointer_s16(JSContextRef Context,JSValueRef Handle);
	int32_t*	GetPointer_s32(JSContextRef Context,JSValueRef Handle);
	int64_t*	GetPointer_s64(JSContextRef Context,JSValueRef Handle);
	float*		GetPointer_float(JSContextRef Context,JSValueRef Handle);
	
	//	create JS types
	template<typename TYPE>
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<TYPE>& Array);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<JSValueRef>& Values);
	JSObjectRef	GetArray(JSContextRef Context,size_t Size);	//	create array of undefineds
	
	//	typed arrays
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<uint8_t>& Values);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<uint16_t>& Values);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<uint32_t>& Values);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<float>& Values);

	//	gr: note: this JSStringRef needs explicit releasing (JSStringRelease) if not sent off to JS land
	//		todo: auto releasing string!
	JSStringRef	GetString(JSContextRef Context,const std::string& Value);
	JSObjectRef	GetObject(JSContextRef Context,JSValueRef Value);

	//	gr: consider templating this so that we can static_assert on non-specified implementation to avoid the auto-resolution to bool
	JSValueRef	GetValue(JSContextRef Context,const std::string& Value);
	JSValueRef	GetValue(JSContextRef Context,float Value);
	JSValueRef	GetValue(JSContextRef Context,bool Value);
	JSValueRef	GetValue(JSContextRef Context,uint8_t Value);
	JSValueRef	GetValue(JSContextRef Context,uint16_t Value);
	JSValueRef	GetValue(JSContextRef Context,uint32_t Value);
	JSValueRef	GetValue(JSContextRef Context,uint64_t Value);
	JSValueRef	GetValue(JSContextRef Context,size_t Value);
	JSValueRef	GetValue(JSContextRef Context,int8_t Value);
	JSValueRef	GetValue(JSContextRef Context,int16_t Value);
	JSValueRef	GetValue(JSContextRef Context,int32_t Value);
	JSValueRef	GetValue(JSContextRef Context,int64_t Value);
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
	bool		IsArray(JSContextRef Context,JSObjectRef Handle);
	bool		IsFunction(JSContextRef Context,JSValueRef Handle);

	//	JSON to object
	TObject		ParseObjectString(JSContextRef Context,const std::string& JsonString);
	std::string	StringifyObject(TLocalContext& Context,Bind::TObject& Object);

	//	throw c++ exception if the exception object is an exception
	void		ThrowException(JSContextRef Context, JSValueRef ExceptionHandle, const char* ThrowContext="");
	void		ThrowException(JSContextRef Context, JSValueRef ExceptionHandle, const std::string& ThrowContext);

	//	enum array supports single objects as well as arrays, so we can enumerate a single float into an array of one, as well as an array
	template<typename TYPE>
	void		EnumArray(JSContextRef Context,JSValueRef Value,ArrayBridge<TYPE>& Array);
	
	void		OnValueChangedExternally(JSContextRef Context,JSValueRef Value);
	
	prmem::Heap&	GetGlobalObjectHeap();
}


std::ostream& operator<<(std::ostream &out,const JSTypedArrayType& in);
std::ostream& operator<<(std::ostream &out,const JSType& in);


//	major abstractions from V8 to JSCore
//	JSCore has no global->local (maybe it should execute a run-next-in-queue func)
void JSLockAndRun(JSGlobalContextRef GlobalContext,std::function<void(JSContextRef&)> Functor);
void JSObjectSetPrivate(JSContextRef Context,JSObjectRef Object,void* Data);
void* JSObjectGetPrivate(JSContextRef Context,JSObjectRef Object);

//	new API. for V8 where we're mirroring data, we need to update the real data;
//	gr: we might have actually removed that
void JSObjectTypedArrayDirty(JSContextRef Context,JSObjectRef Object);
void JSGlobalContextSetQueueJobFunc(JSContextGroupRef ContextGroup, JSGlobalContextRef Context, std::function<void(std::function<void(JSContextRef)>)> QueueJobFunc);


//	preparing for virtuals, anything with this, we expect to overide at some point
#define bind_override

//	stricter type conversion to avoid implicit conversions, so static_assert if a type conversion hasn't been implemented
#define DEFINE_FROM_VALUE(TYPE,FUNCNAME)	\
	template<> inline TYPE JsCore::FromValue<TYPE>(JSContextRef Context,JSValueRef Handle)	{	return FUNCNAME( Context, Handle );	}
DEFINE_FROM_VALUE( bool, GetBool );
DEFINE_FROM_VALUE( uint8_t, GetInt<uint8_t> );
DEFINE_FROM_VALUE( uint16_t, GetInt<uint16_t> );
DEFINE_FROM_VALUE( uint32_t, GetInt<uint32_t> );
DEFINE_FROM_VALUE( uint64_t, GetInt<uint64_t> );
DEFINE_FROM_VALUE( int8_t, GetInt<int8_t> );
DEFINE_FROM_VALUE( int16_t, GetInt<int16_t> );
DEFINE_FROM_VALUE( int32_t, GetInt<int32_t> );
DEFINE_FROM_VALUE( int64_t, GetInt<int64_t> );
DEFINE_FROM_VALUE( std::string, GetString );
DEFINE_FROM_VALUE( float, GetFloat );
DEFINE_FROM_VALUE( uint8_t*, GetPointer_u8 );
DEFINE_FROM_VALUE( uint16_t*, GetPointer_u16 );
DEFINE_FROM_VALUE( uint32_t*, GetPointer_u32 );
//DEFINE_FROM_VALUE( uint64_t*, GetPointer_u64 );
DEFINE_FROM_VALUE( int8_t*, GetPointer_s8 );
DEFINE_FROM_VALUE( int16_t*, GetPointer_s16 );
DEFINE_FROM_VALUE( int32_t*, GetPointer_s32 );
//DEFINE_FROM_VALUE( int64_t*, GetPointer_s64 );
DEFINE_FROM_VALUE( float*, GetPointer_float );

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
	void		CopyTo(ArrayBridge<std::string>& Values);

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
	TFunction(JSContextRef Context,JSObjectRef Value);
	~TFunction();
	
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

	bool								OnJobQueueIteration(std::function<void(std::chrono::milliseconds)>& Sleep);
	
private:
	//	when the group is created it does async jobs on that thread's run loop
	//	for deadlock reasons we don't want that to be the main thread (opengl calls get stuck)
	//	so create it on some other thread and it'll use that runloop
	SoyWorkerJobThread	mContextGroupThread;
	
	JSContextGroupRef	mContextGroup = nullptr;
	
	Array<std::shared_ptr<JsCore::TContext>>	mContexts;

	std::function<void(int32_t)>	mOnShutdown;	//	callback when we want to die
};

class JsCore::TJobQueue : public SoyWorkerJobThread
{
public:
	TJobQueue(JsCore::TContext& Context,std::function<bool(std::function<void(std::chrono::milliseconds)>&)> OnIteration) :
		SoyWorkerJobThread	( "JsCore::TJobQueue" ),
		mContext			( Context ),
		mOnIteration		( OnIteration )
	{
	}

	virtual bool		Iteration(std::function<void(std::chrono::milliseconds)> Sleep) override;
	
	JsCore::TContext&		mContext;
	std::function<bool(std::function<void(std::chrono::milliseconds)>&)> mOnIteration;
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
	virtual int32_t			GetArgumentInt(size_t Index) bind_override	{	return JsCore::GetInt<int32_t>( GetContextRef(), GetArgumentValueNotUndefined(Index) );	}
	virtual float			GetArgumentFloat(size_t Index) bind_override;
	virtual JsCore::TFunction	GetArgumentFunction(size_t Index) bind_override;
	virtual JsCore::TArray	GetArgumentArray(size_t Index) bind_override;
	virtual TObject			GetArgumentObject(size_t Index) bind_override;
	template<typename TYPE>
	TYPE&					GetArgumentPointer(size_t Index);
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<bool>&& Array) bind_override			{	EnumArray( GetContextRef(), GetArgumentValueNotUndefined(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<uint32_t>&& Array) bind_override		{	EnumArray( GetContextRef(), GetArgumentValueNotUndefined(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<int32_t>&& Array) bind_override		{	EnumArray( GetContextRef(), GetArgumentValueNotUndefined(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Array) bind_override		{	EnumArray( GetContextRef(), GetArgumentValueNotUndefined(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<float>&& Array) bind_override			{	EnumArray( GetContextRef(), GetArgumentValueNotUndefined(Index), Array );	}
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<std::string>&& Array) bind_override	{	EnumArray( GetContextRef(), GetArgumentValueNotUndefined(Index), Array );	}

	
	template<typename TYPE>
	TYPE&					This();
	virtual TObject			ThisObject() bind_override;

	virtual bool			IsArgumentString(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeString;	}
	virtual bool			IsArgumentBool(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeBoolean; }
	virtual bool			IsArgumentNumber(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeNumber; }
	virtual bool			IsArgumentUndefined(size_t Index)bind_override	{	return GetArgumentType(Index) == kJSTypeUndefined;	}
	virtual bool			IsArgumentNull(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeNull;	}
	virtual bool			IsArgumentArray(size_t Index)bind_override		{	return IsArray(GetContextRef(), GetArgumentValue(Index)); }
	virtual bool			IsArgumentArrayU8(size_t Index)bind_override;
	virtual bool			IsArgumentFunction(size_t Index)bind_override	{	return IsFunction( GetContextRef(), GetArgumentValue(Index) );	}
	virtual bool			IsArgumentObject(size_t Index)bind_override		{	return GetArgumentType(Index) == kJSTypeObject;	}

	virtual void			Return() bind_override							{	return ReturnUndefined();	}
	void					ReturnUndefined() bind_override;
	virtual void			ReturnNull() bind_override;
	virtual void			Return(const std::string& Value) bind_override	{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(const char* Value) bind_override			{	mReturn = GetValue( GetContextRef(), std::string(Value) );	}
	virtual void			Return(bool Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
#if !defined(TARGET_WINDOWS)&&!defined(TARGET_LINUX)	//	on windows size_t and u64 are the same... not on osx?
	virtual void			Return(size_t Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
#endif
	virtual void			Return(uint8_t Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(uint16_t Value) bind_override			{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(uint32_t Value) bind_override			{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(uint64_t Value) bind_override			{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(int8_t Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(int16_t Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(int32_t Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(int64_t Value) bind_override				{	mReturn = GetValue( GetContextRef(), Value );	}
	virtual void			Return(JsCore::TObject& Value) bind_override	{	mReturn = GetValue( GetContextRef(), Value );	}
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
	virtual void			SetArgumentInt(size_t Index,int32_t Value) bind_override;
	virtual void			SetArgumentBool(size_t Index,bool Value) bind_override;
	virtual void			SetArgumentObject(size_t Index,JsCore::TObject& Value) bind_override;
	virtual void			SetArgumentFunction(size_t Index,JsCore::TFunction& Value) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<std::string>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<float>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index, ArrayBridge<JsCore::TObject>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index, JsCore::TArray& Value) bind_override;

	virtual bool			GetReturnBool() bind_override			{	return GetBool( GetContextRef(), mReturn );	}
	virtual TObject			GetReturnObject() bind_override;
	virtual TFunction		GetReturnFunction() bind_override;
	virtual std::string		GetReturnString() bind_override			{ return GetString(GetContextRef(), mReturn); }
	bool					IsReturnUndefined()						{ return JSValueGetType(GetContextRef(), mReturn) == kJSTypeUndefined; }
	bool					IsReturnString()						{ return JSValueGetType(GetContextRef(), mReturn) == kJSTypeString; }
	bool					IsReturnBool()							{ return JSValueGetType(GetContextRef(), mReturn) == kJSTypeBoolean; }
	bool					IsReturnNull()							{ return JSValueGetType(GetContextRef(), mReturn) == kJSTypeNull; }
	bool					IsReturnObject()						{ return JSValueGetType(GetContextRef(), mReturn) == kJSTypeObject; }

	JSContextRef			GetContextRef();

private:
	JSType					GetArgumentType(size_t Index);
public:	//	gr: exposed for Bind::FromValue() but maybe GetArgument() could be templated safely
	JSValueRef				GetArgumentValue(size_t Index);
	JSValueRef				GetArgumentValueNotUndefined(size_t Index);	//	throws if undefined
	
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
	template<const char* FUNCTIONNAME,typename TYPE>
	void			BindFunction(void(TYPE::* Function)(JsCore::TCallback&) );
	void			RegisterClassWithContext(TLocalContext& Context,const std::string& ParentObjectName,const std::string& OverrideLeafName);

	JsCore::TObjectWrapperBase&	AllocInstance(Bind::TContext& Context)		{	return mAllocator(Context);	}
	
public:
	JSClassDefinition	mDefinition = kJSClassDefinitionEmpty;

private:
	std::string			mName;
	JSClassRef			mClass = nullptr;
	TContext*			mContext = nullptr;
	Array<JSStaticFunction>	mFunctions;
	std::function<Bind::TObjectWrapperBase&(Bind::TContext&)>	mAllocator;
};


//	it should also be a Soy::TUniform type
//	this represents a LOCAL object, do not store!
class JsCore::TObject //: public JsCore::TObject
{
	friend class TPersistent;
	friend class TPromise;
	friend class TObjectWrapperBase;
public:
	TObject()	{}	//	for arrays
	TObject(JSContextRef Context, JSObjectRef This);	//	if This==null then it's the global
	TObject(const TObject& Copy) :
		TObject(Copy.mContext, Copy.mThis)
	{
	}
	//	should probbaly block = operator so any copy of an object always has a new Context
	~TObject();

	inline TObject&			operator=(const TObject& Copy);
	
	template<typename TYPE>
	inline TYPE&			This()	{	return This<TYPE>( mContext, mThis );	}
	template<typename TYPE>
	static TYPE&			This(JSContextRef Context,JSObjectRef Object);

	virtual bool			HasMember(const std::string& MemberName) bind_override;
	bool					IsMemberArray(const std::string& MemberName);

	virtual JsCore::TObject	GetObject(const std::string& MemberName) bind_override;
	virtual std::string		GetString(const std::string& MemberName) bind_override;
	virtual uint32_t		GetInt(const std::string& MemberName) bind_override;
	virtual float			GetFloat(const std::string& MemberName) bind_override;
	virtual JsCore::TFunction	GetFunction(const std::string& MemberName) bind_override;
	virtual bool			GetBool(const std::string& MemberName) bind_override;
	template<typename TYPE>
	void					GetArray(const std::string& MemberName,ArrayBridge<TYPE>&& Values)
	{
		auto Member = GetMember(MemberName);
		JsCore::EnumArray(mContext, Member,Values);
	}

	virtual void			SetObject(const std::string& Name, const JsCore::TObject& Object) bind_override;
	virtual void			SetObjectFromString(const std::string& Name, const std::string& JsonString) bind_override;
	virtual void			SetFunction(const std::string& Name,JsCore::TFunction& Function) bind_override;
	virtual void			SetFloat(const std::string& Name,float Value) bind_override;
	virtual void			SetString(const std::string& Name,const std::string& Value) bind_override;
	virtual void			SetBool(const std::string& Name,bool Value) bind_override;
	virtual void			SetInt(const std::string& Name, uint32_t Value) bind_override;
	virtual void			SetNull(const std::string& Name) bind_override;
	virtual void			SetUndefined(const std::string& Name) bind_override;
	virtual void			SetArray(const std::string& Name,JsCore::TArray& Array) bind_override;
	template<typename TYPE>
	inline void				SetArray(const std::string& Name,const ArrayBridge<TYPE>&& Values) bind_override
	{
		auto Array = JsCore::GetArray( mContext, Values );
		auto ArrayValue = JsCore::GetValue( mContext, Array );
		SetMember( Name, ArrayValue );
	}
	template<typename TYPE>
	inline void				SetArray(const std::string& Name,const ArrayBridge<TYPE>& Values) bind_override
	{
		auto Array = JsCore::GetArray( mContext, Values );
		auto ArrayValue = JsCore::GetValue( mContext, Array );
		SetMember( Name, ArrayValue );
	}
	
	//	Jscore specific
private:
	JSValueRef		GetMember(const std::string& MemberName);
	void			SetMember(const std::string& Name,JSValueRef Value);

private:
	//	this should go, but requiring the param for every func is a pain,
	//	so this context should be updated any time TObject is fetched from somewhere
	//	gr: TObject should never be stored, so this shouldn't be a problem!
	JSContextRef	mContext = nullptr;

public:
	JSObjectRef		mThis = nullptr;
};



class JsCore::TPersistent
{
public:
	TPersistent()	{}
	TPersistent(const TPersistent& That)	{	Retain( That );	}
	TPersistent(Bind::TLocalContext& Context,const TObject& Object,const std::string& DebugName)	{	Retain( Context, Object, DebugName );	}
	TPersistent(Bind::TLocalContext& Context,const TFunction& Object,const std::string& DebugName)	{	Retain( Context, Object, DebugName );	}
	~TPersistent();							//	dec refcount
	
	operator			bool() const						{	return mObject != nullptr;	}
	
	const std::string&	GetDebugName() const			{	return mDebugName;	}
	TObject				GetObject(TLocalContext& Context) const;
	TFunction			GetFunction(TLocalContext& Context) const;
	void				SetWeak(TObjectWrapperBase& Object,JSClassRef Class);		//	this is for v8, we pass the class as it has the destructor function pointer
	TContext&			GetContext()					{	return *mContext;	}
	TPersistent&		operator=(const TPersistent& That)	{	Retain(That);	return *this;	}
	
private:
	void		Retain(Bind::TLocalContext& Context,const TObject& Object,const std::string& DebugName);
	void		Retain(Bind::TLocalContext& Context,const TFunction& Object,const std::string& DebugName);
	void		Retain(const TPersistent& That);
	void 		Release();
	
	static void	Release(JSGlobalContextRef Context,JSObjectRef ObjectOrFunc,const std::string& DebugName);
	static void	Retain(JSGlobalContextRef Context,JSObjectRef ObjectOrFunc,const std::string& DebugName);

protected:
	std::string	mDebugName;

	//	these two make a local context!
	TContext*			mContext = nullptr;	//	hacky atm, storing this for = and deferred release in destructor
	//	gr: funcs actually take a JSContext, but we're explictly using global
	JSGlobalContextRef	mRetainedContext = nullptr;	//	which context we retained with

public:
#if defined(JSAPI_V8)
	std::shared_ptr<V8::TPersistent<v8::Object>>	mObject;
#else
	JSObjectRef		mObject = nullptr;
#endif
};


class JsCore::TContextDebug
{
public:
	void	OnPersitentRetained(TPersistent& Persistent);
	void	OnPersitentReleased(TPersistent& Persistent);

	std::map<std::string,int>		mPersistentObjectCount;
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
	void					ConstructObject(TLocalContext& LocalContext,const std::string& ObjectTypeName,JSObjectRef NewObject,ArrayBridge<JSValueRef>&& ConstructorArguments);

	virtual JsCore::TPromise	CreatePromise(Bind::TLocalContext& LocalContext, const std::string& DebugName) bind_override;
	virtual std::shared_ptr<JsCore::TPromise>	CreatePromisePtr(Bind::TLocalContext& LocalContext, const std::string& DebugName) bind_override;
	
	template<typename OBJECTWRAPPERTYPE>
	void				BindObjectType(const std::string& ParentName=std::string(),const std::string& OverrideLeafName=std::string());
	

	prmem::Heap&		GetObjectHeap()		{	return GetGeneralHeap();	}
	prmem::Heap&		GetImageHeap()		{	return GetGeneralHeap();	}
	prmem::Heap&		GetGeneralHeap()	{	return JsCore::GetGlobalObjectHeap();	}
	std::string			GetResolvedFilename(const std::string& Filename);
	
	//	this can almost be static, but TCallback needs a few functions of TContext
	JSValueRef			CallFunc(TLocalContext& LocalContext,std::function<void(JsCore::TCallback&)> Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception,const std::string& FunctionContext);
	
	
	void				OnPersitentRetained(TPersistent& Persistent)	{	mDebug.OnPersitentRetained(Persistent);	}
	void				OnPersitentReleased(TPersistent& Persistent)	{	mDebug.OnPersitentReleased(Persistent);	}
	
	template<const char* FUNCTIONNAME>
	static JSObjectCallAsFunctionCallback	GetRawFunction(std::function<void(JsCore::TCallback&)> Function);

	template<const char* FUNCTIONNAME,typename TYPE>
	static JSObjectCallAsFunctionCallback	GetRawFunction(void(TYPE::* Function)(JsCore::TCallback&));

	
protected:
	void				Cleanup();		//	actual cleanup called by instance & destructor
	void				ReleaseContext();	//	try and release javascript objects

private:
	void				BindRawFunction(const std::string& FunctionName,const std::string& ParentObjectName,JSObjectCallAsFunctionCallback Function);
	void				Execute_Reference(std::function<void(TLocalContext&)>& Function);

public:
	TInstance&			mInstance;
	JSGlobalContextRef	mContext = nullptr;
	
	prmem::Heap			mImageHeap = prmem::Heap(true,true,"Context Images");
	std::string			mRootDirectory = mInstance.mRootDirectory;

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
	void			Resolve(Bind::TLocalContext& Context,JSValueRef Value) const;
	void			Resolve(Bind::TLocalContext& Context,JSObjectRef Value) const;
	template<typename TYPE>
	void			Resolve(Bind::TLocalContext& Context,const TYPE& Value) const				{	Resolve(Context, GetValue(Context.mLocalContext, Value)); }
	void			ResolveUndefined(Bind::TLocalContext& Context) const;

	void			Reject(Bind::TLocalContext& Context,const std::string& Value) const			{	Reject( Context, GetValue( Context.mLocalContext, Value ) );	}
	void			Reject(Bind::TLocalContext& Context,JSValueRef Value) const;//				{	mReject.Call(nullptr,Value);	}
	
	//	risky?
	TContext&		GetContext() { return mPromise.GetContext(); }

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
	TObjectWrapperBase(Bind::TContext& Context) :
		mContext	( Context )
	{
	}
	virtual ~TObjectWrapperBase()	{}

	virtual TObject	GetHandle(Bind::TLocalContext& Context);
	virtual void	SetHandle(Bind::TLocalContext& Context,TObject& NewHandle);
	
	//	construct and allocate
	virtual void 	Construct(TCallback& Arguments)=0;
	/*
	template<typename TYPE>
	static TYPE*	Allocate(Bind::TContext& Context)
	{
		return new TYPE( Context );
	}
	*/
	TContext&		GetContext()	{	return mContext;	}	//	owner

public:
	//	gr: this is a weak reference so the object gets free'd
	//	gr: javascript core should also switch to a persistent (so context definitely isn't stored), but also
	//		a weak (non retained) reference for the main one, just as v8 does
#if defined(PERSISTENT_OBJECT_HANDLE)
	TPersistent		mHandle;
#else
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
	
public:
	TObjectWrapper(Bind::TContext& Context) :
		TObjectWrapperBase	( Context )
	{
	}
	
	static std::string		GetTypeName()	{	return TYPENAME;	}
	
	static TTemplate 		AllocTemplate(JsCore::TContext& Context);
	
protected:
#if defined(JSAPI_V8)
	static void				Free(const v8::WeakCallbackInfo<void>& Meta)
	{
		//	gr: this should be null, see TPersistent::SetWeak
		auto* Param = Meta.GetParameter();
		auto* This = Param;
		//auto* This = Meta.GetInternalField( V8::InternalFieldDataIndex );
		if ( !This )
			throw Soy::AssertException("Free/Weak callback from v8 has null internal field");
		auto* Wrapper = reinterpret_cast<TObjectWrapperBase*>( This );
		auto* TypeWrapper = dynamic_cast<THISTYPE*>( Wrapper );
		if ( !TypeWrapper )
			throw Soy::AssertException("Failed to dynamically object pointer to " + Soy::GetTypeName<THISTYPE>() );
		FreeObject( *TypeWrapper );
	}
#elif defined(JSAPI_JSCORE)
	static void				Free(JSObjectRef ObjectRef)
	{
		JSContextRef Context = nullptr;
		
		//	gr: if this fails as it's null, the object being cleaned up may be the class/constructor, if it isn't attached to anything (ie. not attached to the global!)
		//		we shouldn't really have our own constructors being deleted!
		//	free the void
		//	cast to TObject and use This to do proper type checks
		//std::Debug << "Free object of type " << TYPENAME << std::endl;
		auto& Object = TObject::This<THISTYPE>( Context, ObjectRef );
		FreeObject( Object );
	
		//	reset the void for safety?
		//std::Debug << "ObjectRef=" << ObjectRef << "(" << TYPENAME << ") to null" << std::endl;
		JSObjectSetPrivate( Context, ObjectRef, nullptr );
	}
#elif defined(JSAPI_CHAKRA)
	static void				Free(void* ObjectPtr)
	{
		auto* This = ObjectPtr;
		if (!This)
			throw Soy::AssertException("Free callback from chakracore has null internal field");
		auto* Wrapper = reinterpret_cast<TObjectWrapperBase*>(This);
		auto* TypeWrapper = dynamic_cast<THISTYPE*>(Wrapper);
		if (!TypeWrapper)
			throw Soy::AssertException("Failed to dynamically object pointer to " + Soy::GetTypeName<THISTYPE>());
		FreeObject(*TypeWrapper);
	}
#endif
	
	static void				FreeObject(THISTYPE& Object)
	{
		auto* pObject = &Object;
		
		auto& Heap = JsCore::GetGlobalObjectHeap();
		if ( !Heap.Free(pObject) )
			std::Debug << "Js global Heap failed to Free() " << Soy::GetTypeName<THISTYPE>() << std::endl;
	}
	
protected:
	std::shared_ptr<TYPE>			mObject;
};


template<const char* TYPENAME,class TYPE>
inline JsCore::TTemplate JsCore::TObjectWrapper<TYPENAME,TYPE>::AllocTemplate(JsCore::TContext& Context)
{
	//	setup constructor CFunc here
#if defined(JSAPI_V8)
	static JSObjectCallAsConstructorCallback CConstructorFunc = [](const v8::FunctionCallbackInfo<v8::Value>& Meta)
	{
		auto& Isolate = *Meta.GetIsolate();
		try
		{
			//	when using .apply() this will throw...
			//	need a good work around
			//	but this causes other problems too
			if ( !Meta.IsConstructCall() )
				throw Soy::AssertException("Calling constructor callback, but is not constructing");
	
			Array<JSValueRef> Arguments;
			for ( auto i=0;	i<Meta.Length();	i++ )
			{
				JSValueRef Value = JSValueRef(Meta[i]);
				Arguments.PushBack(Value);
			}
			
			//	in V8, the object is already made, we need to construct it
			JSObjectRef ThisObject( Meta.This() );
			
			JSContextRef ContextRef( Isolate.GetCurrentContext() );
			auto& Context = JsCore::GetContext( ContextRef );
			TLocalContext LocalContext( ContextRef, Context );
		
			JSValueRef Exception;
			Context.ConstructObject( LocalContext, TYPENAME, ThisObject, GetArrayBridge(Arguments) );
		
			Meta.GetReturnValue().Set( ThisObject );
		}
		catch(std::exception& e)
		{
			std::stringstream Error;
			Error << TYPENAME << "() constructor exception: " << e.what();
			auto ErrorString = JSStringRef( Isolate, Error.str() );
			auto Exception = Isolate.ThrowException( ErrorString.mThis );
			Meta.GetReturnValue().Set(Exception);
		}
	};
#elif defined(JSAPI_JSCORE)
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
#elif defined(JSAPI_CHAKRA)
	static JSObjectCallAsConstructorCallback CConstructorFunc = [](JsValueRef Function,bool ConstructorCall,JsValueRef* ArgumentValues,uint16_t ArgumentValuesCount,void* CallbackState) ->JsValueRef
	{
		JSContextRef ContextRef = reinterpret_cast<JSContextRef>(CallbackState);
		try
		{
			if ( !ConstructorCall )
				throw Soy::AssertException("Function callback expected to be constructor");
			
			auto& Context = JsCore::GetContext( ContextRef );
			
			//	argument 0 is this
			//	https://github.com/Microsoft/ChakraCore/wiki/JsNativeFunction
			//	but.... that's the parent (so assume is global for constructor)
			//	from this example https://github.com/microsoft/Chakra-Samples/blob/master/ChakraCore%20Samples/OpenGL%20Engine/OpenGLEngine/ChakraCoreHost.cpp#L270
			//	so we need to create a new one
			JsValueRef ParentValue = ArgumentValues[0];
	
			//	gr: we can remote-array this
			Array<JSValueRef> Arguments;
			for ( auto a=1;	a<ArgumentValuesCount;	a++ )
			{
				Arguments.PushBack( ArgumentValues[a] );
				/*
				auto Value = ArgumentValues[a];
				auto Type = JSValueGetType(Value);
				std::Debug << "Argument[" << a << "] is " << Type << std::endl;
				 */
			}
			
			TLocalContext LocalContext( ContextRef, Context );
			auto ThisObject = Context.CreateObjectInstance( LocalContext, TYPENAME, GetArrayBridge(Arguments) );
			auto ThisValue = ThisObject.mThis;
			
			//	retrn the new external object
			return ThisValue.mValue;
		}
		catch(std::exception& e)
		{
			auto StringValue = Bind::GetString( ContextRef, e.what() );

			auto Result = JSValueMakeUndefined(ContextRef);
			//	make undefined fails if we set exception, so JSValueMakeUndefined()
			//	then throws and we lose this error
			//	should this error be returning an exception object?
			JsSetException( StringValue.mValue );
			return Result;
		}
	};
#endif
	
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



template<const char* FUNCTIONNAME>
inline JSObjectCallAsFunctionCallback JsCore::TContext::GetRawFunction(std::function<void(JsCore::TCallback&)> Function)
{
	//	try and remove context cache
	static std::function<void(JsCore::TCallback&)> FunctionCache;
	if ( FunctionCache != nullptr )
		throw Soy::AssertException("This function is already bound. Duplicate string?");
	FunctionCache = Function;
	
#if defined(JSAPI_V8)
	JSObjectCallAsFunctionCallback CFunc = [](const v8::FunctionCallbackInfo<v8::Value>& Meta)
	{
		auto& Isolate = *Meta.GetIsolate();
		try
		{
			Array<JSValueRef> Arguments;
			for ( auto i=0;	i<Meta.Length();	i++ )
			{
				JSValueRef Value = JSValueRef(Meta[i]);
				Arguments.PushBack(Value);
			}
			JSObjectRef ThisObject( Meta.This() );

			JSContextRef ContextRef( Isolate.GetCurrentContext() );
			//	need local context, needs a new scope, but doesn't need a lock? or local scope I dont think
			//	for our funcs we just need an exception catcher
			v8::TryCatch TryCatch( &Isolate );
			ContextRef.mTryCatch = &TryCatch;

			auto& Context = GetContext( ContextRef );
			TLocalContext LocalContext( ContextRef, Context );

			JSValueRef Exception;
			auto ReturnValue = Context.CallFunc( LocalContext, FunctionCache, ThisObject, Arguments.GetSize(), Arguments.GetArray(), Exception, FUNCTIONNAME );
		
			if ( TryCatch.HasCaught() )
				throw V8::TException( Isolate, TryCatch, FUNCTIONNAME);
			
			Meta.GetReturnValue().Set( ReturnValue.mThis );
		}
		catch(std::exception& e)
		{
			auto Exception = Isolate.ThrowException( v8::String::NewFromUtf8( &Isolate, e.what() ));
			Meta.GetReturnValue().Set( Exception );
		}
	};
#elif defined(JSAPI_JSCORE)
	JSObjectCallAsFunctionCallback CFunc = [](JSContextRef Context,JSObjectRef Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef* Exception)
	{
		auto& ContextPtr = JsCore::GetContext( Context );
		TLocalContext LocalContext( Context, ContextPtr );
		return ContextPtr.CallFunc( LocalContext, FunctionCache, This, ArgumentCount, Arguments, *Exception, FUNCTIONNAME );
	};
#elif defined(JSAPI_CHAKRA)
	JSObjectCallAsConstructorCallback CFunc = [](JsValueRef Function,bool ConstructorCall,JsValueRef* ArgumentValues,uint16_t ArgumentValuesCount,void* CallbackState) ->JsValueRef
	{
		//	gr: turn callback state into context
		JSContextRef ContextRef = reinterpret_cast<JSContextRef>(CallbackState);
		auto& Context = JsCore::GetContext( ContextRef );
		try
		{
			JsValueRef ThisValue = ArgumentValues[0];
			JSObjectRef ThisObject( ThisValue );

			Array<JSValueRef> Arguments;
			for ( auto i=1;	i<ArgumentValuesCount;	i++ )
			{
				JSValueRef Value = ArgumentValues[i];
				Arguments.PushBack(Value);
			}
			
			TLocalContext LocalContext( ContextRef, Context );
			
			JSValueRef Exception = nullptr;
			auto ReturnValue = Context.CallFunc( LocalContext, FunctionCache, ThisObject, Arguments.GetSize(), Arguments.GetArray(), Exception, FUNCTIONNAME );
			
			return ReturnValue;
		}
		catch(std::exception& e)
		{
			auto StringValue = Bind::GetString( ContextRef, e.what() );
			JsSetException( StringValue.mValue );
			return JSValueMakeUndefined(ContextRef);
		}
	};
#endif
	
	return CFunc;
}


template<const char* FUNCTIONNAME,typename TYPE>
inline JSObjectCallAsFunctionCallback JsCore::TContext::GetRawFunction(void(TYPE::* Function)(JsCore::TCallback&))
{
	//	try and remove context cache
	static void(TYPE::* FunctionCache)(JsCore::TCallback&)  = nullptr;
	if ( FunctionCache != nullptr )
		throw Soy::AssertException("This function is already bound. Duplicate string?");
	FunctionCache = Function;
	
#if defined(JSAPI_V8)
	JSObjectCallAsFunctionCallback CFunc = [](const v8::FunctionCallbackInfo<v8::Value>& Meta)
	{
		auto& Isolate = *Meta.GetIsolate();
		try
		{
			Array<JSValueRef> Arguments;
			for (auto i = 0; i<Meta.Length(); i++)
			{
				JSValueRef Value = JSValueRef(Meta[i]);
				Arguments.PushBack(Value);
			}
			JSObjectRef ThisObject(Meta.This());
			
			JSContextRef ContextRef(Isolate.GetCurrentContext());
			//	need local context, needs a new scope, but doesn't need a lock? or local scope I dont think
			//	for our funcs we just need an exception catcher
			v8::TryCatch TryCatch(&Isolate);
			ContextRef.mTryCatch = &TryCatch;

			auto& Context = GetContext(ContextRef);
			TLocalContext LocalContext(ContextRef, Context);

			Bind::TObject ThisObjectObject(ContextRef, ThisObject);
			auto& pThis = ThisObjectObject.This<TYPE>();

			std::function<void(JsCore::TCallback&)> BoundFunction = [&](JsCore::TCallback& Params)
			{
				(pThis.*FunctionCache)(Params);
			};

			JSValueRef Exception;
			auto ReturnValue = Context.CallFunc(LocalContext, BoundFunction, ThisObject, Arguments.GetSize(), Arguments.GetArray(), Exception, FUNCTIONNAME);

			if (TryCatch.HasCaught())
				throw V8::TException(Isolate, TryCatch, FUNCTIONNAME);

			Meta.GetReturnValue().Set(ReturnValue.mThis);
		}
		catch (std::exception& e)
		{
			auto Exception = Isolate.ThrowException(v8::String::NewFromUtf8(&Isolate, e.what()));
			Meta.GetReturnValue().Set(Exception);
		}
	};
#elif defined(JSAPI_JSCORE)
	JSObjectCallAsFunctionCallback CFunc = [](JSContextRef Context,JSObjectRef Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef* Exception)
	{
		auto& ContextPtr = JsCore::GetContext( Context );
		
		Bind::TObject ThisObject( Context, This );
		auto& pThis = ThisObject.This<TYPE>();
		std::function<void(JsCore::TCallback&)> BoundFunction = [&](JsCore::TCallback& Params)
		{
			(pThis.*FunctionCache)( Params );
		};
		TLocalContext LocalContext( Context, ContextPtr );
		return ContextPtr.CallFunc( LocalContext, BoundFunction, This, ArgumentCount, Arguments, *Exception, FUNCTIONNAME );
	};
#elif defined(JSAPI_CHAKRA)
	JSObjectCallAsConstructorCallback CFunc = [](JsValueRef Function,bool ConstructorCall,JsValueRef* ArgumentValues,uint16_t ArgumentValuesCount,void* CallbackState) ->JsValueRef
	{
		//	gr: turn callback state into context
		JSContextRef ContextRef = reinterpret_cast<JSContextRef>(CallbackState);
		auto& Context = JsCore::GetContext(ContextRef);
		try
		{
			JsValueRef ThisValue = ArgumentValues[0];
			JSObjectRef ThisObject(ThisValue);

			Array<JSValueRef> Arguments;
			for (auto i = 1; i < ArgumentValuesCount; i++)
			{
				JSValueRef Value = ArgumentValues[i];
				Arguments.PushBack(Value);
			}

			TLocalContext LocalContext(ContextRef, Context);

			Bind::TObject ThisObjectObject(ContextRef, ThisObject);
			auto& pThis = ThisObjectObject.This<TYPE>();

			std::function<void(JsCore::TCallback&)> BoundFunction = [&](JsCore::TCallback& Params)
			{
				(pThis.*FunctionCache)(Params);
			};

			JSValueRef Exception = nullptr;
			auto ReturnValue = Context.CallFunc(LocalContext, BoundFunction, ThisObject, Arguments.GetSize(), Arguments.GetArray(), Exception, FUNCTIONNAME);

			return ReturnValue;
		}
		catch (std::exception& e)
		{
			auto StringValue = Bind::GetString(ContextRef, e.what());
			JsSetException(StringValue.mValue);
			return JSValueMakeUndefined(ContextRef);
		}
	};
#endif
	
	return CFunc;
}

template<const char* FUNCTIONNAME>
inline void JsCore::TContext::BindGlobalFunction(std::function<void(JsCore::TCallback&)> Function,const std::string& ParentName)
{
	auto FunctionPointer = GetRawFunction<FUNCTIONNAME>( Function );
	BindRawFunction( FUNCTIONNAME, ParentName, FunctionPointer );
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
inline TYPE& JsCore::TObject::This(JSContextRef Context,JSObjectRef Object)
{
	auto* This = JSObjectGetPrivate( Context, Object);
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

	Template.mAllocator = [this](Bind::TContext& Context) -> TObjectWrapperBase&
	{
		auto& Heap = this->GetObjectHeap();
		auto* NewObject = Heap.Alloc<OBJECTWRAPPERTYPE>( Context );
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
	
	std::function<void(Bind::TLocalContext&)> Exec = [&](Bind::TLocalContext& LocalContext)
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
	Values.Alloc(TypeArray.GetSize());
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
		auto Type = JSValueGetType(Context, Handle);
		std::stringstream Error;
		Error << "Trying to convert value to number, but is " << Type;
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
	auto FunctionPointer = JsCore::TContext::GetRawFunction<FUNCTIONNAME>( Function );
	
	JSStaticFunction NewFunction;
	NewFunction.name = FUNCTIONNAME;
	NewFunction.callAsFunction = FunctionPointer;
	NewFunction.attributes = kJSPropertyAttributeNone;
	
	mFunctions.PushBack(NewFunction);
}

template<const char* FUNCTIONNAME,typename TYPE>
inline void JsCore::TTemplate::BindFunction(void(TYPE::* Function)(JsCore::TCallback&))
{
	auto FunctionPointer = JsCore::TContext::GetRawFunction<FUNCTIONNAME>( Function );
	
	JSStaticFunction NewFunction;
	NewFunction.name = FUNCTIONNAME;
	NewFunction.callAsFunction = FunctionPointer;
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
	auto SingleValue = FromValue<TYPE>( Context, Value );
	Array.PushBack( SingleValue );
}
