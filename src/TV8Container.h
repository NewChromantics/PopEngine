#pragma once
//#include "PopTrack.h"
#include <memory>
#include <functional>
#include <SoyTypes.h>
#include <SoyAssert.h>
#include <Array.hpp>

class PopV8Allocator;
class TV8Container;

//	forward decalrations
namespace v8
{
	class Platform;
	class Isolate;
	class Context;
	class Value;

	template<typename T>
	class Local;

	//	our wrapper
	class CallbackInfo;
	
	template<typename T>
	inline T&	GetInternalFieldObject(Local<Value> Value,size_t InternalFieldIndex);

	template<typename T>
	inline T&	GetObject(Local<Value> Value);
	
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<float>& FloatArray);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<float>&& FloatArray);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<int>& IntArray);
	void	EnumArray(Local<Value> ValueHandle,ArrayBridge<int>&& IntArray);

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
	
	const v8::FunctionCallbackInfo<v8::Value>&	mParams;
	TV8Container&								mContainer;
	v8::Isolate*								mIsolate;
	v8::Local<v8::Context>						mContext;
};



class TV8Container
{
public:
	TV8Container();
	
    void        CreateContext();
	void		LoadScript(const std::string& Source);
	
 	void		ExecuteGlobalFunc(const std::string& FunctionName);
	template<const char* FunctionName>
	void		BindGlobalFunction(std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function);
    void        BindObjectType(const char* ObjectName,std::function<v8::Local<v8::FunctionTemplate>(TV8Container&)> GetTemplate);

	template<const char* FunctionName>
	void					BindFunction(v8::Local<v8::Object> This,std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function);
	template<const char* FunctionName>
	void					BindFunction(v8::Local<v8::ObjectTemplate> This,std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function);
	
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,const std::string& FunctionName,v8::Local<v8::Object> This);
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>&& Params);
	void					RunScoped(std::function<void(v8::Local<v8::Context>)> Lambda);
									  
	
private:
	void		BindRawFunction(v8::Local<v8::Object> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&));
	void		BindRawFunction(v8::Local<v8::ObjectTemplate> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&));

public:
	v8::Persistent<v8::Context>		mContext;		//	our "document", keep adding scripts toit
	v8::Isolate*					mIsolate;
	std::shared_ptr<v8::Platform>	mPlatform;
	std::shared_ptr<PopV8Allocator>	mAllocator;
};



inline void CallFunc(std::function<v8::Local<v8::Value>(v8::CallbackInfo&)> Function,const v8::FunctionCallbackInfo<v8::Value>& Paramsv8,TV8Container& Container)
{
	v8::CallbackInfo Params( Paramsv8, Container );
	try
	{
		auto ReturnValue = Function( Params );
		Params.mParams.GetReturnValue().Set(ReturnValue);
	}
	catch(std::exception& e)
	{
		//	pass exception to javascript
		auto* Isolate = Params.mParams.GetIsolate();
		auto Exception = Isolate->ThrowException( v8::String::NewFromUtf8( Isolate, e.what() ));
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

