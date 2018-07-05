#pragma once
//#include "PopTrack.h"
#include <memory>
#include <functional>
#include <SoyTypes.h>
#include <SoyAssert.h>
#include <Array.hpp>
#include <HeapArray.hpp>

//	gr: the diffs are external vs internal as well as API changes
#define V8_VERSION	5
//#define V8_VERSION	6

#if !defined(V8_VERSION)
#error need V8_VERSION 5 or 6
#endif

class PopV8Allocator;
class TV8Container;

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
	Persist<TYPE>	GetPersistent(v8::Isolate& Isolate,Local<TYPE> LocalHandle);
	
	template<typename TYPE>
	Local<TYPE> 	GetLocal(v8::Isolate& Isolate,Persist<TYPE> PersistentHandle);
	
	//	todo: specialise this for other types
	template<typename NUMBERTYPE>
	Local<Array>	GetArray(v8::Isolate& Isolate,ArrayBridge<NUMBERTYPE>&& Values);

	std::string		GetString(Local<Value> Str);
	Local<Value>	GetString(v8::Isolate& Isolate,const std::string& Str);

	void	CallFunc(std::function<Local<Value>(CallbackInfo&)> Function,const FunctionCallbackInfo<Value>& Paramsv8,TV8Container& Container);

	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<float>& FloatArray,const std::string& Context);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<float>&& FloatArray,const std::string& Context);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<int>& IntArray,const std::string& Context);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<int>&& IntArray,const std::string& Context);
	
	//	fast copy from typed arrays
	template<typename ARRAYTYPE,typename ELEMENTTYPE>
	void	EnumArray(Local<Value> ValueArrayHandle,ArrayBridge<ELEMENTTYPE>&& IntArray);
	
}


#include "include/libplatform/libplatform.h"
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



class v8::CallbackInfo
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


class TV8ObjectTemplate
{
public:
	TV8ObjectTemplate()	{}
	TV8ObjectTemplate(v8::Persist<v8::ObjectTemplate>& Template,const std::string& Name) :
		mTemplate	( Template ),
		mName		( Name )
	{
	}
	
	bool			operator==(const std::string& Name) const	{	return this->mName == Name;	}
	
public:
	v8::Persist<v8::ObjectTemplate>	mTemplate;
	std::string						mName;
};

class TV8Container
{
public:
	TV8Container();
	
    void     	   CreateContext();
	v8::Isolate&	GetIsolate()	{	return *mIsolate;	}
	
	void		RunScoped(std::function<void(v8::Local<v8::Context>)> Lambda);
	void		QueueScoped(std::function<void(v8::Local<v8::Context>)> Lambda);
	
	void		ProcessJobs();	//	run all the queued jobs then return
	
	//	run these with RunScoped (internal) or QueueJob (external)
	void		LoadScript(v8::Local<v8::Context> Context,const std::string& Source);
	void		ExecuteGlobalFunc(v8::Local<v8::Context> Context,const std::string& FunctionName);

	template<typename WRAPPERTYPE,typename TYPE>
	v8::Local<v8::Object>	CreateObjectInstance(TYPE& Object)
	{
		return CreateObjectInstance( WRAPPERTYPE::GetObjectTypeName(), &Object );
	}
	v8::Local<v8::Object>	CreateObjectInstance(const std::string& ObjectTypeName,void* Object);

	template<const char* FunctionName>
	void		BindGlobalFunction(std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function);
	void        BindObjectType(const std::string& ObjectName,std::function<v8::Local<v8::FunctionTemplate>(TV8Container&)> GetTemplate);

	template<const char* FunctionName>
	void					BindFunction(v8::Local<v8::Object> This,std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function);
	template<const char* FunctionName>
	void					BindFunction(v8::Local<v8::ObjectTemplate> This,std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function);
	
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,const std::string& FunctionName,v8::Local<v8::Object> This);
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>& Params);
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>&& Params);
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,v8::Persist<v8::Function> FunctionHandle,ArrayBridge<v8::Local<v8::Value>>&& Params);
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,v8::Persist<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>&& Params);

private:
	void		BindRawFunction(v8::Local<v8::Object> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&));
	void		BindRawFunction(v8::Local<v8::ObjectTemplate> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&));

public:
	v8::Persist<v8::Context>		mContext;		//	our "document", keep adding scripts toit
	v8::Isolate*					mIsolate;
	std::shared_ptr<v8::Platform>	mPlatform;
	std::shared_ptr<v8::ArrayBuffer::Allocator>	mAllocator;
	
	Array<TV8ObjectTemplate>		mObjectTemplates;
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
inline void TV8Container::BindGlobalFunction(std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function)
{
	auto Bind = [&](v8::Local<v8::Context> Context)
	{
		auto This = Context->Global();
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
	auto WindowHandle = Obj->GetInternalField(InternalFieldIndex);
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
		throw Soy::AssertException("Value is not internally backed (external)");

	//	gr: this needs to do type checks, and we need to verify the internal type as we're blindly casting!
	auto WindowVoid = v8::Local<v8::External>::Cast( Handle )->Value();
	if ( WindowVoid == nullptr )
		throw Soy::AssertException("Internal Field is null");
	auto Window = reinterpret_cast<T*>( WindowVoid );
	return *Window;
}

template<typename TYPE>
inline v8::Persist<TYPE> v8::GetPersistent(v8::Isolate& Isolate,Local<TYPE> LocalHandle)
{
	Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
	PersistentHandle.Reset( &Isolate, LocalHandle );
	return PersistentHandle;
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
inline void v8::EnumArray(Local<Value> ValueHandle,ArrayBridge<ELEMENTTYPE>&& IntArray)
{
	auto ValueArrayHandle = Local<ARRAYTYPE>::Cast( ValueHandle );

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
