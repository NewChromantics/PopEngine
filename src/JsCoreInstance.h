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
	class TCallback;
	class TObject;
	class TTemplate;
	
	std::string	GetString(JSContextRef Context,JSValueRef Handle);
	std::string	GetString(JSContextRef Context,JSStringRef Handle);
	int32_t		GetInt(JSContextRef Context,JSValueRef Handle);
	bool		GetBool(JSContextRef Context,JSValueRef Handle);
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

//	functions marked virtual need to become generic
class JsCore::TContext : public Bind::TContext
{
public:
	TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory);
	~TContext();
	
	virtual void		LoadScript(const std::string& Source,const std::string& Filename) override;
	
	template<const char* FunctionName>
	void				BindGlobalFunction(std::function<void(Bind::TCallback&)> Function);

	virtual void			CreateGlobalObjectInstance(TString ObjectType,TString Name) override;
	virtual Bind::TObject	CreateObjectInstance(const std::string& ObjectTypeName);
	
	//	api calls with context provided
	template<typename IN,typename OUT>
	OUT					GetString(IN Handle)			{	return JsCore::GetString(mContext,Handle);	}
	void				ThrowException(JSValueRef ExceptionHandle)	{	JsCore::ThrowException( mContext, ExceptionHandle );	}

	
private:
	
	void				BindRawFunction(const char* FunctionName,JSObjectCallAsFunctionCallback Function);
	JSValueRef			CallFunc(std::function<void(Bind::TCallback&)> Function,JSContextRef Context,JSObjectRef FunctionJs,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception);
	
	JSObjectRef			GetGlobalObject(const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object

	
public://	temp
	TInstance&			mInstance;
	JSGlobalContextRef	mContext = nullptr;
	std::string			mRootDirectory;

	//	"templates" in v8, "classes" in jscore
	Array<TTemplate>	mObjectTemplates;
};


class JsCore::TCallback : public Bind::TCallback
{
public:
	TCallback(TContext& Context) :
		Bind::TCallback	( Context ),
		mContext		( Context ),
		mContextRef		( mContext.mContext )
	{
	}
	virtual size_t		GetArgumentCount() override	{	return mArguments.GetSize();	}
	virtual std::string	GetArgumentString(size_t Index) override;
	virtual bool		GetArgumentBool(size_t Index) override;
	virtual int32_t		GetArgumentInt(size_t Index) override;
	virtual float		GetArgumentFloat(size_t Index) override;

	virtual bool		IsArgumentString(size_t Index)override;
	virtual bool		IsArgumentBool(size_t Index)override;

	virtual void		Return() override;
	virtual void		ReturnNull() override;
	virtual void		Return(const std::string& Value) override;
	virtual void		Return(uint32_t Value) override;
	virtual void		Return(Bind::TObject Value) override;
	virtual void		Return(Bind::TArray Value) override;
	
public:
	TContext&			mContext;
	JSValueRef			mThis = nullptr;
	JSValueRef			mReturn = nullptr;
	Array<JSValueRef>	mArguments;
	JSContextRef		mContextRef = nullptr;
};


class JsCore::TTemplate : public Bind::TTemplate
{
public:
	JSClassRef		mClass = nullptr;
};

//	make this generic for v8 & jscore
//	it should also be a Soy::TUniform type
class JsCore::TObject : public Bind::TObject
{
public:
	//	generic
	TObject(JSContextRef Context,JSObjectRef This);	//	if This==null then it's the global
	TObject(TObject&& That);
	
	virtual Bind::TObject	GetObject(const std::string& MemberName) override;
	virtual std::string		GetString(const std::string& MemberName) override;
	virtual uint32_t		GetInt(const std::string& MemberName) override;
	virtual float			GetFloat(const std::string& MemberName) override;
	
	virtual void			SetObject(const std::string& Name,const Bind::TObject& Object) override;
	
	//	Jscore specific
private:
	JSValueRef		GetMember(const std::string& MemberName);
	void			SetMember(const std::string& Name,JSValueRef Value);
	JSContextRef	mContext = nullptr;
	
public:
	JSObjectRef		mThis = nullptr;
};






template<const char* FunctionName>
inline void JsCore::TContext::BindGlobalFunction(std::function<void(Bind::TCallback&)> Function)
{
	static std::function<void(Bind::TCallback&)> FunctionCache = Function;
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

