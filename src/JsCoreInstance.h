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
	class TObject;
	
	std::string	GetString(JSContextRef Context,JSValueRef Handle);
	std::string	GetString(JSContextRef Context,JSStringRef Handle);
	int32_t		GetInt(JSContextRef Context,JSValueRef Handle);
	JSStringRef	GetString(JSContextRef Context,const std::string& String);
	
	void		ThrowException(JSContextRef Context,JSValueRef ExceptionHandle,const std::string& ThrowContext=std::string());
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


class JsCore::TContext : public Bind::TContainer
{
public:
	TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory);
	~TContext();
	
	void				LoadScript(const std::string& Source,const std::string& Filename);
	
	template<const char* FunctionName>
	void				BindGlobalFunction(std::function<JSValueRef(Bind::TCallbackInfo&)> Function);

	virtual void		CreateGlobalObjectInstance(TString ObjectType,TString Name) override;
	TObject				CreateObjectInstance(const std::string& ObjectTypeName);
	
	//	api calls with context provided
	template<typename IN,typename OUT>
	OUT					GetString(IN Handle)			{	return JsCore::GetString(mContext,Handle);	}
	void				ThrowException(JSValueRef ExceptionHandle)	{	JsCore::ThrowException( mContext, ExceptionHandle );	}

	
private:
	
	void				BindRawFunction(const char* FunctionName,JSObjectCallAsFunctionCallback Function);
	JSValueRef			CallFunc(std::function<JSValueRef(TCallbackInfo&)> Function,JSContextRef Context,JSObjectRef FunctionJs,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception);
	
	JSObjectRef			GetGlobalObject(const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object

	
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



//	make this generic for v8 & jscore
//	it should also be a Soy::TUniform type
class JsCore::TObject
{
public:
	//	generic
	TObject(JSContextRef Context,JSObjectRef This);	//	if This==null then it's the global
	virtual ~TObject()	{}
	
	virtual TObject		GetObject(const std::string& MemberName);
	virtual std::string	GetString(const std::string& MemberName);
	virtual uint32_t	GetInt(const std::string& MemberName);
	virtual float		GetFloat(const std::string& MemberName);
	
	virtual void		SetObject(const std::string& Name,const TObject& Object);
	
	//	Jscore specific
private:
	JSValueRef		GetMember(const std::string& MemberName);
	void			SetMember(const std::string& Name,JSValueRef Value);
	JSContextRef	mContext = nullptr;
	
public:
	JSObjectRef		mThis = nullptr;
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

