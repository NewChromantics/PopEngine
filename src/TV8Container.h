#pragma once
//#include "PopTrack.h"
#include <memory>
#include <functional>

class PopV8Allocator;

//	forward decalrations
namespace v8
{
	class Platform;
	class Isolate;
	class Context;
	
	//	our wrapper
	class CallbackInfo;
	
	template<typename T>
	class Local;
}


#include "include/libplatform/libplatform.h"
#include "include/v8.h"


class v8::CallbackInfo
{
public:
	CallbackInfo(const v8::FunctionCallbackInfo<v8::Value>& Params) :
	mParams	( Params )
	{
	}
	
	const v8::FunctionCallbackInfo<v8::Value>& mParams;
};



class TV8Container
{
public:
	TV8Container();
	
    void        CreateContext();
	void		LoadScript(const std::string& Source);
	
 	void		ExecuteGlobalFunc(const std::string& FunctionName);
	template<const char* FunctionName>
	void		BindGlobalFunction(std::function<void(v8::CallbackInfo&)> Function)
	{
		auto Bind = [&](v8::Local<v8::Context> Context)
		{
			auto This = Context->Global();
			BindFunction<FunctionName>(This,Function);
		};
		RunScoped(Bind);
	};
    void        BindObjectType(const char* ObjectName,std::function<v8::Local<v8::FunctionTemplate>(TV8Container&)> GetTemplate);

	template<typename T>
	void Test(const T& x);

	template<const char* FunctionName>
	void		BindFunction(v8::Local<v8::Object> This,std::function<void(v8::CallbackInfo&)> Function);
	template<const char* FunctionName>
	void		BindFunction(v8::Local<v8::ObjectTemplate> This,std::function<void(v8::CallbackInfo&)> Function);
	
	v8::Local<v8::Value>	ExecuteFunc(v8::Local<v8::Context> ContextHandle,const std::string& FunctionName,v8::Local<v8::Object> This);
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



template<const char* FunctionName>
inline void TV8Container::BindFunction(v8::Local<v8::Object> This,std::function<void(v8::CallbackInfo&)> Function)
{
	static std::function<void(v8::CallbackInfo&)> FunctionCache = Function;
	auto RawFunction = [](const v8::FunctionCallbackInfo<v8::Value>& Paramsv8)
	{
		v8::CallbackInfo Params( Paramsv8 );
		FunctionCache( Params );
	};
	BindRawFunction( This, FunctionName, RawFunction );
}


template<const char* FunctionName>
inline void TV8Container::BindFunction(v8::Local<v8::ObjectTemplate> This,std::function<void(v8::CallbackInfo&)> Function)
{
	static std::function<void(v8::CallbackInfo&)> FunctionCache = Function;
	auto RawFunction = [](const v8::FunctionCallbackInfo<v8::Value>& Paramsv8)
	{
		v8::CallbackInfo Params( Paramsv8 );
		FunctionCache( Params );
	};
	BindRawFunction( This, FunctionName, RawFunction );
}

