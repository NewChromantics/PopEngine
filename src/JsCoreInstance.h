#pragma once


#include <JavaScriptCore/JavaScriptCore.h>
#include <memory>
#include "HeapArray.hpp"
#include "TBind.h"


//	https://karhm.com/JavaScriptCore_C_API/
namespace JsCore
{
	class TInstance;
	class TContext;
	class TCallbackInfo;
	
	std::string	HandleToString(JSContextRef Context,JSValueRef Handle);
	int32_t		HandleToInt(JSContextRef Context,JSValueRef Handle);
}

//	VM to contain multiple contexts/containers
class JsCore::TInstance
{
public:
	TInstance(const std::string& RootDirectory,const std::string& ScriptFilename);
	~TInstance();
	
	std::shared_ptr<TContext>	CreateContext();
	
private:
	JSContextGroupRef	mContextGroup;
	std::string			mRootDirectory;
	
	std::shared_ptr<TContext>	mContext;
};

class JsCore::TContext
{
public:
	TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory);
	~TContext();
	
	void				LoadScript(const std::string& Source,const std::string& Filename);
	
	template<const char* FunctionName>
	void				BindGlobalFunction(std::function<JSValueRef(Bind::TCallbackInfo&)> Function);

private:
	void				ThrowException(JSValueRef ExceptionHandle);	//	throws if value is not undefined
	void				BindRawFunction(const char* FunctionName,JSObjectCallAsFunctionCallback Function);
	JSValueRef			CallFunc(std::function<JSValueRef(TCallbackInfo&)> Function,JSContextRef Context,JSObjectRef FunctionJs,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception);
	
public://	temp
	TInstance&			mInstance;
	JSGlobalContextRef	mContext = nullptr;
	std::string			mRootDirectory;
};


class JsCore::TCallbackInfo : public Bind::TCallbackInfo
{
public:
	TCallbackInfo(TInstance& Instance) :
		mInstance	( Instance )
	{
	}
	virtual size_t		GetArgumentCount() const override	{	return mArguments.GetSize();	}
	virtual std::string	GetArgumentString(size_t Index) const override;
	virtual int32_t		GetArgumentInt(size_t Index) const override;

public:
	TInstance&			mInstance;
	JSValueRef			mThis;
	Array<JSValueRef>	mArguments;
	JSContextRef		mContext;
};



template<const char* FunctionName>
inline void JsCore::TContext::BindGlobalFunction(std::function<JSValueRef(Bind::TCallbackInfo&)> Function)
{
	static std::function<JSValueRef(TCallbackInfo&)> FunctionCache = Function;
	static TContext* ContextCache = nullptr;
	JSObjectCallAsFunctionCallback CFunc = [](JSContextRef Context,JSObjectRef Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef* Exception)
	{
		//JSValueRef
		return ContextCache->CallFunc( FunctionCache, Context, Function, This, ArgumentCount, Arguments, *Exception );
	};
	ContextCache = this;
	/*
	auto RawFunction = [](const v8::FunctionCallbackInfo<v8::Value>& Paramsv8)
	{
		return CallFunc( FunctionCache, Paramsv8, *ContainerCache );
	};
	ContainerCache = this;
	 */
	BindRawFunction( FunctionName, CFunc );
}

