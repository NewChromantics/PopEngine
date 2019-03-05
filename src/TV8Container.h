#pragma once
//#include "PopTrack.h"
#include <memory>
#include <functional>
#include "SoyTypes.h"
#include "SoyAssert.h"
#include "Array.hpp"
#include "HeapArray.hpp"
#include "TBind.h"

//	gr: the diffs are external vs internal as well as API changes
//#define V8_VERSION	5
#define V8_VERSION	6

#if !defined(V8_VERSION)
#error need V8_VERSION 5 or 6
#endif

class PopV8Allocator;
class TV8Container;
class TV8Inspector;

template<typename TYPE>
class V8Storage;

//	forward decalrations
namespace v8
{
	class Array;
	class Platform;
	class Isolate;
	class Context;
	class Value;
	class Task;
	class String;
	class Float32Array;
	class Function;
	class Object;

	template<typename T>
	class Local;
	
	template<typename TYPE>
	class CopyablePersistentTraits;
	template<typename T,typename Traits>
	class Persistent;

	template<typename TYPE>
	using Persist = Persistent<TYPE,CopyablePersistentTraits<TYPE>>;
	
	template<typename TYPE>
	class FunctionCallbackInfo;
	
	//	our wrappers
	class CallbackInfo;
	class LambdaTask;
	
	template<typename TYPE>
	TYPE&			GetInternalFieldObject(Local<Value> Value,size_t InternalFieldIndex);

	template<typename TYPE>
	TYPE&			GetObject(Local<Value> Value);

	//template<typename TYPE>
	Local<Value>	GetException(v8::Isolate& Isolate,const std::exception& Exception);
	
	template<typename TYPE>
	std::shared_ptr<V8Storage<TYPE>>	GetPersistent(v8::Isolate& Isolate,Local<TYPE> LocalHandle);
	
	template<typename TYPE>
	Local<TYPE> 	GetLocal(v8::Isolate& Isolate,Persist<TYPE> PersistentHandle);
	
	//	todo: specialise this for other types
	template<typename NUMBERTYPE>
	Local<Array>	GetArray(v8::Isolate& Isolate,ArrayBridge<NUMBERTYPE>&& Values);
	Local<Array>	GetArray(v8::Isolate& Isolate,size_t ElementCount,std::function<Local<Value>(size_t)> GetElement);

	//	get a specific typed/memory backed array
	//	uint8_t -> uint8clampedarray
	Local<Value>	GetTypedArray(v8::Isolate& Isolate,ArrayBridge<uint8_t>&& Values);
	void			CopyToTypedArray(v8::Isolate& Isolate,ArrayBridge<uint8_t>&& Values,Local<v8::Value> ArrayHandle);

	
	std::string		GetString(Local<Value> Str);
	Local<Value>	GetString(v8::Isolate& Isolate,const std::string& Str);
	Local<Value>	GetString(v8::Isolate& Isolate,const char* Str);
	Local<Function>	GetFunction(Local<Context> Context,Local<Object> This,const std::string& FunctionName);
	std::string		GetTypeName(v8::Local<v8::Value> Handle);

	void	CallFunc(std::function<Local<Value>(CallbackInfo&)> Function,const FunctionCallbackInfo<Value>& Paramsv8,TV8Container& Container);

	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<float>& FloatArray,const std::string& Context);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<float>&& FloatArray,const std::string& Context);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<int>& IntArray,const std::string& Context);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<int>&& IntArray,const std::string& Context);
	
	//	fast copy from typed arrays
	template<typename ARRAYTYPE,typename ELEMENTTYPE>
	void	EnumArray(Local<Value> ValueArrayHandle,ArrayBridge<ELEMENTTYPE>&& IntArray);
	template<typename ARRAYTYPE,typename ELEMENTTYPE>
	void	EnumArray(Local<Value> ValueArrayHandle,ArrayBridge<ELEMENTTYPE>& IntArray);
	
	void	EnumArray(Local<Array> ArrayHandle,std::function<void(size_t,Local<Value>)> EnumElement);

	
	//	our own type caster which throws if cast fails.
	//	needed because my v8 built doesnt have cast checks, and I can't determine if they're enabled or not
	template<typename TYPE>
	Local<TYPE>	SafeCast(Local<Value> ValueHandle);
	template<typename TYPE>
	bool		IsType(Local<Value>& ValueHandle);
}


//#include "include/libplatform/libplatform.h"
#include "libplatform/libplatform.h"
#include "include/v8.h"


class V8Exception : public std::exception
{
public:
	V8Exception(v8::TryCatch& TryCatch,const std::string& Context);

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
class V8Storage
{
public:
	V8Storage(v8::Isolate& Isolate,v8::Local<TYPE>& Local)
	{
		/*
		Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
		PersistentHandle.Reset( &Isolate, LocalHandle );
		return PersistentHandle;
		 */
		mPersistent.Reset( &Isolate, Local );
	}
	~V8Storage()
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
inline std::shared_ptr<V8Storage<TYPE>> v8::GetPersistent(v8::Isolate& Isolate,Local<TYPE> LocalHandle)
{
	auto ResolverPersistent = std::make_shared<V8Storage<TYPE>>( Isolate, LocalHandle );
	return ResolverPersistent;
}


class v8::CallbackInfo : public Bind::TCallbackInfo
{
public:
	CallbackInfo(const v8::FunctionCallbackInfo<v8::Value>& Params,TV8Container& Container) :
		mParams		( Params ),
		mContainer	( Container ),
		mIsolate	( mParams.GetIsolate() ),
		mContext	( mIsolate->GetCurrentContext() )
	{
	}
	
	v8::Isolate&	GetIsolate() const		{	return *mIsolate;	}
	template<typename TYPE>
	TYPE&			GetThis() const			{	return v8::GetObject<TYPE>( mParams.This() );	}
	
	std::string		GetResolvedFilename(const std::string& Filename) const;
	
	virtual size_t		GetArgumentCount() const override	{	return mParams.Length();	}
	virtual std::string	GetArgumentString(size_t Index) const override;
	virtual int32_t		GetArgumentInt(size_t Index) const override;

public:
	const v8::FunctionCallbackInfo<v8::Value>&	mParams;
	TV8Container&								mContainer;
	v8::Isolate*								mIsolate;
	v8::Local<v8::Context>						mContext;
};

class v8::LambdaTask : public v8::Task
{
public:
	LambdaTask(std::function<void(v8::Local<v8::Context>)> Lambda,TV8Container& Container):
		mLambda		( Lambda ),
		mContainer	( Container )
	{
	}
	virtual void Run() override;
	
public:
	TV8Container&								mContainer;
	std::function<void(v8::Local<v8::Context>)>	mLambda;
};



class TV8ObjectWrapperBase
{
public:
	virtual ~TV8ObjectWrapperBase()	{}
	
	//	handle actual constructor (arguments etc), throw on error
	virtual void 	Construct(const v8::CallbackInfo& Arguments)=0;
	
	template<typename TYPE>
	static TV8ObjectWrapperBase*	Allocate(TV8Container& Container,v8::Local<v8::Object> This)
	{
		return new TYPE( Container, This );
	}
};


class TV8ObjectTemplate
{
public:
	typedef std::function<TV8ObjectWrapperBase*(TV8Container&,v8::Local<v8::Object>)> ALLOCATOR;

public:
	TV8ObjectTemplate()	{}
	TV8ObjectTemplate(std::shared_ptr<V8Storage<v8::ObjectTemplate>> Template,const std::string& Name) :
		mTemplate	( Template ),
		mName		( Name )
	{
	}
	
	bool			operator==(const std::string& Name) const	{	return this->mName == Name;	}
	
public:
	ALLOCATOR						mAllocator;
	std::shared_ptr<V8Storage<v8::ObjectTemplate>>	mTemplate;
	std::string						mName;
};





class TV8Allocator : public v8::ArrayBuffer::Allocator
{
public:
	TV8Allocator(const char* Name) :
		mHeap	( true, true, Name )
	{
	}
	
	virtual void*	Allocate(size_t length) override;
	virtual void*	AllocateUninitialized(size_t length) override;
	virtual void	Free(void* data, size_t length) override;
	
public:
	prmem::Heap		mHeap;
};



class TV8Container
{
public:
	TV8Container(const std::string& RootDirectory);
	
	v8::Isolate&	GetIsolate()	{	return *mIsolate;	}
	void		ProcessJobs(std::function<bool()> IsRunning);	//	run all the queued jobs then return

	void		RunScoped(std::function<void(v8::Local<v8::Context>)> Lambda);
	void		QueueScoped(std::function<void(v8::Local<v8::Context>)> Lambda);
	void		QueueDelayScoped(std::function<void(v8::Local<v8::Context>)> Lambda,size_t DelayMs);

	void		Yield(size_t SleepMilliseconds);
	
	//	run these with RunScoped (internal) or QueueJob (external)
	v8::Local<v8::Value>	LoadScript(v8::Local<v8::Context> Context,const std::string& Source,const std::string& SourceFilename);
	v8::Local<v8::Value>	LoadScript(v8::Local<v8::Context> Context,v8::Local<v8::String> Source,const std::string& SourceFilename);

	TV8ObjectTemplate::ALLOCATOR	GetAllocator(const char* TYPENAME);
	//	deprecated for object
	template<typename WRAPPERTYPE,typename TYPE>
	v8::Local<v8::Object>	CreateObjectInstance(TYPE& Object)
	{
		return CreateObjectInstance( WRAPPERTYPE::GetObjectTypeName(), &Object );
	}
	v8::Local<v8::Object>	CreateObjectInstance(const std::string& ObjectTypeName,void* Object);
	v8::Local<v8::Object>	CreateObjectInstance(const std::string& ObjectTypeName);
	void					CreateGlobalObjectInstance(const std::string& ObjectTypeName,const std::string& ObjectName);
	v8::Local<v8::Object>	GetGlobalObject(v8::Local<v8::Context>& Context,const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object

	template<const char* FunctionName>
	void		BindGlobalFunction(std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function,const std::string& ParentName);
	void        BindObjectType(const std::string& ObjectName,std::function<v8::Local<v8::FunctionTemplate>(TV8Container&)> GetTemplate,TV8ObjectTemplate::ALLOCATOR Allocator,const std::string& ParentObject=std::string());

	template<const char* FunctionName>
	void					BindFunction(v8::Local<v8::Object> This,std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function);
	template<const char* FunctionName>
	void					BindFunction(v8::Local<v8::ObjectTemplate> This,std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function);
	
	//	execute, but catch c++ or v8 exceptions and return them at v8 exceptions back to javascript
	v8::Local<v8::Value>	ExecuteFuncAndCatch(v8::Local<v8::Context> ContextHandle,const std::string& FunctionName,v8::Local<v8::Object> This);
	v8::Local<v8::Value>	ExecuteFuncAndCatch(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>& Params);
	v8::Local<v8::Value>	ExecuteFuncAndCatch(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>&& Params)
	{
		return ExecuteFuncAndCatch(ContextHandle,FunctionHandle,This,Params);
	}

	//	execute, but throw c++ or v8 exceptions
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,const std::string& FunctionName,v8::Local<v8::Object> This);
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>& Params);
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>&& Params)
	{
		return ExecuteFunc(ContextHandle,FunctionHandle,This,Params);
	}
	
	//	less v8-y stuff
	prmem::Heap&			GetImageHeap()	{	return mImageHeap;	}
	prmem::Heap&			GetV8Heap()		{	return mAllocator.mHeap;	}
	std::string				GetResolvedFilename(const std::string& Filename) const;

private:
	void		BindRawFunction(v8::Local<v8::Object> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&));
	void		BindRawFunction(v8::Local<v8::ObjectTemplate> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&));

	void     	CreateContext();
	void     	CreateInspector();
	

public:
	std::shared_ptr<V8Storage<v8::Context>>		mContext;		//	our "document", keep adding scripts to it
	std::shared_ptr<TV8Inspector>	mInspector;		//	the remote debugger!
	v8::Isolate*					mIsolate;
	std::shared_ptr<v8::Platform>	mPlatform;

	Array<TV8ObjectTemplate>		mObjectTemplates;
	
private:
	std::string						mRootDirectory;
	TV8Allocator					mAllocator;
	prmem::Heap						mImageHeap;
};


inline void v8::CallFunc(std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function,const v8::FunctionCallbackInfo<v8::Value>& Paramsv8,TV8Container& Container)
{
	v8::CallbackInfo Params( Paramsv8, Container );
	try
	{
		auto ReturnValue = Function( Params );
		Params.mParams.GetReturnValue().Set(ReturnValue);
	}
	catch(std::exception& e)
	{
		auto Exception = v8::GetException( Container.GetIsolate(), e );
		Params.mParams.GetReturnValue().Set(Exception);
	}
}


template<const char* FunctionName>
inline void TV8Container::BindFunction(v8::Local<v8::Object> This,std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function)
{
	static std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> FunctionCache = Function;
	static TV8Container* ContainerCache = nullptr;
	auto RawFunction = [](const v8::FunctionCallbackInfo<v8::Value>& Paramsv8)
	{
		CallFunc( FunctionCache, Paramsv8, *ContainerCache );
	};
	ContainerCache = this;
	BindRawFunction( This, FunctionName, RawFunction );
}


template<const char* FunctionName>
inline void TV8Container::BindFunction(v8::Local<v8::ObjectTemplate> This,std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function)
{
	static std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> FunctionCache = Function;
	static TV8Container* ContainerCache = nullptr;
	auto RawFunction = [](const v8::FunctionCallbackInfo<v8::Value>& Paramsv8)
	{
		CallFunc( FunctionCache, Paramsv8, *ContainerCache );
	};
	ContainerCache = this;
	BindRawFunction( This, FunctionName, RawFunction );
}

template<const char* FunctionName>
inline void TV8Container::BindGlobalFunction(std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function,const std::string& ParentName)
{
	auto Bind = [&](v8::Local<v8::Context> Context)
	{
		auto This = GetGlobalObject( Context, ParentName );
		BindFunction<FunctionName>(This,Function);
	};
	RunScoped(Bind);
};


template<typename T>
inline T& v8::GetInternalFieldObject(v8::Local<v8::Value> Value,size_t InternalFieldIndex)
{
	auto Obj = v8::Local<v8::Object>::Cast( Value );
	auto FieldCount = Obj->InternalFieldCount();
	if ( InternalFieldIndex >= FieldCount )
	{
		std::stringstream Error;
		Error << "Object missing internal field " << InternalFieldIndex << "/" << FieldCount;
		throw Soy::AssertException(Error.str());
	}
	auto InternalFieldIndexi = static_cast<int>(InternalFieldIndex);
	auto WindowHandle = Obj->GetInternalField( InternalFieldIndexi );
	return GetObject<T>( WindowHandle );
}


inline v8::Local<v8::Value> v8::GetException(v8::Isolate& Isolate,const std::exception& Exception)
{
	auto ErrorStr = v8::String::NewFromUtf8( &Isolate, Exception.what() );
	auto JsException = Isolate.ThrowException( ErrorStr );
	return JsException;
}

template<typename T>
inline T& v8::GetObject(v8::Local<v8::Value> Handle)
{
	//	if this isn't an external, lets assume it's it's inernal field
	if ( !Handle->IsExternal())
	{
		if ( Handle->IsObject() )
		{
			auto HandleObject = v8::Local<v8::Object>::Cast( Handle );
			Handle = HandleObject->GetInternalField(0);
		}
	}
	
	if ( !Handle->IsExternal() )
	{
		std::stringstream Error;
		Error << "Getting object from Value(" << v8::GetTypeName(Handle) << ") is not internally backed (!IsExternal)";
		throw Soy::AssertException(Error.str());
	}
	
	//	gr: this needs to do type checks, and we need to verify the internal type as we're blindly casting!
	//	gr: also, to deal with multiple inheritence,
	//		cast this to the base object wrapper, then dynamic cast to T (which'll solve all our problems)
	auto* WindowVoid = v8::Local<v8::External>::Cast( Handle )->Value();
	if ( WindowVoid == nullptr )
		throw Soy::AssertException("Internal Field is null");
	auto* Window = reinterpret_cast<T*>( WindowVoid );
	return *Window;
}


template<typename TYPE>
inline v8::Local<TYPE> v8::GetLocal(v8::Isolate& Isolate,Persist<TYPE> PersistentHandle)
{
	Local<TYPE> LocalHandle = Local<TYPE>::New( &Isolate, PersistentHandle );
	return LocalHandle;
}

template<typename NUMBERTYPE>
inline v8::Local<v8::Array> v8::GetArray(v8::Isolate& Isolate,ArrayBridge<NUMBERTYPE>&& Values)
{
	auto ArrayHandle = Array::New( &Isolate );
	for ( auto i=0;	i<Values.GetSize();	i++ )
	{
		double Value = Values[i];
		auto ValueHandle = Number::New( &Isolate, Value );
		ArrayHandle->Set( i, ValueHandle );
	}
	return ArrayHandle;
}



template<typename ARRAYTYPE,typename ELEMENTTYPE>
inline void v8::EnumArray(Local<Value> ValueHandle,ArrayBridge<ELEMENTTYPE>& IntArray)
{
	auto ValueArrayHandle = v8::SafeCast<ARRAYTYPE>( ValueHandle );
	
	//	skip div0 checks
	if ( ValueArrayHandle->Length() == 0 )
		return;
	
	//	check arrays align
	auto ElementSize = IntArray.GetElementSize();
	auto ElementSizev8 = ValueArrayHandle->ByteLength() / ValueArrayHandle->Length();
	if ( ElementSize != ElementSizev8 )
	{
		std::stringstream Error;
		Error << "Trying to copy v8 array(elementsize=" << ElementSizev8 <<") into array(elementsize=" << ElementSize <<" but element sizes misaligned";
		throw Soy::AssertException( Error.str() );
	}
	
	auto ArraySize = ValueArrayHandle->Length();
	auto* NewElements = IntArray.PushBlock(ArraySize);
	auto NewElementsByteSize = IntArray.GetElementSize() * ArraySize;
	auto BytesWritten = ValueArrayHandle->CopyContents( NewElements, NewElementsByteSize );
	if ( NewElementsByteSize != BytesWritten )
	{
		std::stringstream Error;
		Error << "Copying v8 array, wrote " << BytesWritten << " bytes, expected " << NewElementsByteSize;
		throw Soy::AssertException( Error.str() );
	}
}

template<typename ARRAYTYPE,typename ELEMENTTYPE>
inline void v8::EnumArray(Local<Value> ValueHandle,ArrayBridge<ELEMENTTYPE>&& IntArray)
{
	EnumArray<ARRAYTYPE,ELEMENTTYPE>( ValueHandle, IntArray );
}


//	our own type caster which throws if cast fails.
//	needed because my v8 built doesnt have cast checks, and I can't determine if they're enabled or not
template<typename TYPE>
inline v8::Local<TYPE> v8::SafeCast(Local<Value> ValueHandle)
{
	if ( !IsType<TYPE>(ValueHandle) )
	{
		std::stringstream Error;
		Error << "Trying to cast " << GetTypeName(ValueHandle) << " to other type " << Soy::GetTypeName<TYPE>();
		throw Soy::AssertException(Error.str());
	}
	return ValueHandle.As<TYPE>();
}

/*	gr: I wanted a static assert, but
	a) xcode/clang resolves error at source.cpp:1 so I can't find caller
	b) can't error type info :/
	c) Just omitting the base implementation means we get link errors for specific types, which is a bit easier
template<typename TYPE>
inline bool v8::IsType(Local<Value>& ValueHandle)
{
	//static_assert(false, "This function needs specialising");
}
*/

#define ISTYPE_DEFINITION(TYPE)	\
template<> inline bool v8::IsType<v8::TYPE>(Local<Value>& ValueHandle)	{	return ValueHandle->Is##TYPE();	}

ISTYPE_DEFINITION(Int8Array);
ISTYPE_DEFINITION(Uint8Array);
ISTYPE_DEFINITION(Uint8ClampedArray);
ISTYPE_DEFINITION(Int16Array);
ISTYPE_DEFINITION(Uint16Array);
ISTYPE_DEFINITION(Int32Array);
ISTYPE_DEFINITION(Uint32Array);
ISTYPE_DEFINITION(Float32Array);
ISTYPE_DEFINITION(Number);
ISTYPE_DEFINITION(Function);
ISTYPE_DEFINITION(Boolean);
ISTYPE_DEFINITION(Array);



