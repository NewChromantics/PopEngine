#pragma once

#include <JavaScriptCore/JavaScriptCore.h>
#include <memory>
#include "HeapArray.hpp"
//#include "TBind.h"



//	https://karhm.com/JavaScriptCore_C_API/
namespace JsCore
{
	class TInstance;	//	vm
	class TContext;
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

	//	value conversion
	std::string	GetString(JSContextRef Context,JSValueRef Handle);
	std::string	GetString(JSContextRef Context,JSStringRef Handle);
	int32_t		GetInt(JSContextRef Context,JSValueRef Handle);
	float		GetFloat(JSContextRef Context,JSValueRef Handle);
	bool		GetBool(JSContextRef Context,JSValueRef Handle);
	
	JSStringRef	GetString(JSContextRef Context,const std::string& Value);
	JSValueRef	GetValue(JSContextRef Context,const std::string& Value);
	JSValueRef	GetValue(JSContextRef Context,float Value);
	JSValueRef	GetValue(JSContextRef Context,uint32_t Value);
	JSValueRef	GetValue(JSContextRef Context,int32_t Value);
	JSValueRef	GetValue(JSContextRef Context,bool Value);
	JSValueRef	GetValue(JSContextRef Context,uint8_t Value);
	JSValueRef	GetValue(JSContextRef Context,TObject& Value);
	JSObjectRef	GetObject(JSContextRef Context,JSValueRef Value);
	
	template<typename TYPE>
	JSObjectRef	GetArray(JSContextRef Context,ArrayBridge<TYPE>& Array);
	JSObjectRef	GetArray(JSContextRef Context,const ArrayBridge<JSValueRef>& Values);

	template<typename TYPE>
	TYPE		FromValue(JSContextRef Context,JSValueRef Handle);

	void		ThrowException(JSContextRef Context,JSValueRef ExceptionHandle,const std::string& ThrowContext=std::string());
}

#define DEFINE_FROM_VALUE(TYPE,FUNCNAME)	\
	template<> TYPE JsCore::FromValue<TYPE>(JSContextRef Context,JSValueRef Handle)	{	return FUNCNAME( Context, Handle );	}
DEFINE_FROM_VALUE( bool, GetBool );
DEFINE_FROM_VALUE( int32_t, GetInt );
DEFINE_FROM_VALUE( std::string, GetString );
DEFINE_FROM_VALUE( float, GetFloat );

namespace Bind = JsCore;
#define bind_override


class JsCore::TArray
{
public:
	TArray(JSContextRef Context,JSObjectRef Object) :
		mContext	( Context ),
		mThis		( Object )
	{
	}
	
	void		Set(size_t Index,Bind::TObject& Object);
	void		CopyTo(ArrayBridge<uint8_t>&& Values);

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
	
	operator		bool() const	{	return mThis != nullptr;	}
	
	//	would be nice to capture return, but it's contained inside Params for now. Maybe template & error for type mismatch
	void			Call(Bind::TCallback& Params);
	void			Call(Bind::TObject& This);
	JSValueRef		Call(JSObjectRef This=nullptr,JSValueRef Value=nullptr);
	
public:
	JSContextRef	mContext = nullptr;
	JSObjectRef		mThis = nullptr;
};


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
class JsCore::TContext //: public Bind::TContext
{
public:
	TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory);
	~TContext();
	
	virtual void		LoadScript(const std::string& Source,const std::string& Filename) bind_override;
	//virtual void		Execute(TFunction Function,TObject This,ArrayBridge<TObject>&& Args) bind_override;
	virtual void		Execute(std::function<void(TContext&)> Function) bind_override;
	virtual void		Execute(Bind::TCallback& Callback) bind_override;
	virtual void		Queue(std::function<void(TContext&)> Function) bind_override;
	virtual void		QueueDelay(std::function<void(TContext&)> Function,size_t DelayMs) bind_override;

	template<const char* FunctionName>
	void				BindGlobalFunction(std::function<void(Bind::TCallback&)> Function,const std::string& ParentName=std::string());

	Bind::TObject			GetGlobalObject(const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object
	virtual void			CreateGlobalObjectInstance(const std::string&  ObjectType,const std::string& Name) bind_override;
	virtual Bind::TObject	CreateObjectInstance(const std::string& ObjectTypeName=std::string());

	virtual Bind::TPersistent	CreatePersistent(Bind::TObject& Object) bind_override;
	virtual Bind::TPersistent	CreatePersistent(Bind::TFunction& Object) bind_override;
	virtual Bind::TPromise		CreatePromise() bind_override;
	virtual Bind::TArray	CreateArray(size_t ElementCount,std::function<std::string(size_t)> GetElement) bind_override;
	virtual Bind::TArray	CreateArray(size_t ElementCount,std::function<TObject(size_t)> GetElement) bind_override;
	virtual Bind::TArray	CreateArray(size_t ElementCount,std::function<int32_t(size_t)> GetElement) bind_override;
	template<typename TYPE>
	Bind::TArray			CreateArray(ArrayBridge<TYPE>&& Values);

	
	template<typename OBJECTWRAPPERTYPE>
	void				BindObjectType(const std::string& ParentName=std::string());
	
	//	api calls with context provided
	template<typename IN,typename OUT>
	OUT					GetString(IN Handle)			{	return JsCore::GetString(mContext,Handle);	}
	void				ThrowException(JSValueRef ExceptionHandle)	{	JsCore::ThrowException( mContext, ExceptionHandle );	}

	
	
	prmem::Heap&		GetImageHeap()	{	return mImageHeap;	}
	prmem::Heap&		GetV8Heap()		{	return mAllocatorHeap;	}
	std::string			GetResolvedFilename(const std::string& Filename);
	
private:
	
	void				BindRawFunction(const std::string& FunctionName,const std::string& ParentObjectName,JSObjectCallAsFunctionCallback Function);
	JSValueRef			CallFunc(std::function<void(Bind::TCallback&)> Function,JSContextRef Context,JSObjectRef FunctionJs,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception,const std::string& FunctionContext);
	
	//JSObjectRef			GetGlobalObject(const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object

	
public:
	TInstance&			mInstance;
	JSGlobalContextRef	mContext = nullptr;
	
	prmem::Heap			mAllocatorHeap = prmem::Heap(true,true,"Context Heap");
	prmem::Heap			mImageHeap = prmem::Heap(true,true,"Context Images");
	std::string			mRootDirectory;

	//	"templates" in v8, "classes" in jscore
	Array<TTemplate>	mObjectTemplates;
	
	JsCore::TFunction	mMakePromiseFunction;
};


class JsCore::TCallback //: public Bind::TCallback
{
public:
	TCallback(TContext& Context) :
		//Bind::TCallback	( Context ),
		mContext		( Context )
	{
	}
	
	virtual size_t			GetArgumentCount() bind_override	{	return mArguments.GetSize();	}
	virtual std::string		GetArgumentString(size_t Index) bind_override;
	std::string				GetArgumentFilename(size_t Index);
	virtual bool			GetArgumentBool(size_t Index) bind_override;
	virtual int32_t			GetArgumentInt(size_t Index) bind_override;
	virtual float			GetArgumentFloat(size_t Index) bind_override;
	virtual Bind::TFunction	GetArgumentFunction(size_t Index) bind_override;
	virtual Bind::TArray	GetArgumentArray(size_t Index) bind_override;
	virtual TObject			GetArgumentObject(size_t Index) bind_override;
	template<typename TYPE>
	TYPE&					GetArgumentPointer(size_t Index);
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<uint32_t>&& Array) bind_override;
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<int32_t>&& Array) bind_override;
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Array) bind_override;
	virtual void			GetArgumentArray(size_t Index,ArrayBridge<float>&& Array) bind_override;
	
	template<typename TYPE>
	TYPE&					This();
	virtual TObject			ThisObject() bind_override;

	virtual bool			IsArgumentString(size_t Index)bind_override;
	virtual bool			IsArgumentBool(size_t Index)bind_override;
	virtual bool			IsArgumentUndefined(size_t Index)bind_override;
	virtual bool			IsArgumentArray(size_t Index)bind_override;

	virtual void			Return() bind_override;
	virtual void			ReturnNull() bind_override;
	virtual void			Return(const std::string& Value) bind_override	{	mReturn = GetValue( mContext.mContext, Value );	}
	virtual void			Return(uint32_t Value) bind_override			{	mReturn = GetValue( mContext.mContext, Value );	}
	virtual void			Return(Bind::TObject& Value) bind_override		{	mReturn = GetValue( mContext.mContext, Value );	}
	virtual void			Return(Bind::TArray& Value) bind_override		{	mReturn = GetValue( mContext.mContext, Value.mThis );	}
	virtual void			Return(Bind::TPromise& Value) bind_override;
	virtual void			Return(ArrayBridge<Bind::TObject>&& Values) bind_override	{	mReturn = GetArray( mContext.mContext, Values );	}

	//	functions for c++ calling JS
	virtual void			SetThis(Bind::TObject& This) bind_override;
	virtual void			SetArgumentString(size_t Index,const std::string& Value) bind_override;
	virtual void			SetArgumentInt(size_t Index,uint32_t Value) bind_override;
	virtual void			SetArgumentObject(size_t Index,Bind::TObject& Value) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<std::string>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,ArrayBridge<float>&& Values) bind_override;
	virtual void			SetArgumentArray(size_t Index,Bind::TArray& Value) bind_override;

	virtual bool			GetReturnBool() bind_override			{	return GetBool( mContext.mContext, mReturn );	}
	
protected:
	virtual void*		GetThis() bind_override;
	virtual void*		GetArgumentPointer(size_t Index) bind_override;
	
public:
	TContext&			mContext;
	JSValueRef			mThis = nullptr;
	JSValueRef			mReturn = nullptr;
	Array<JSValueRef>	mArguments;
};


class JsCore::TTemplate //: public Bind::TTemplate
{
	friend JsCore::TContext;
public:
	TTemplate()	{}
	TTemplate(JSContextRef Context,const std::string& Name) :
		mName		( Name ),
		mContext	( Context )
	{
	}
	
	bool			operator==(const std::string& Name) const	{	return mName == Name;	}


	template<const char* FUNCTIONNAME>
	void			BindFunction(std::function<void(Bind::TCallback&)> Function);
	void			RegisterClassWithContext();
	
private:
	std::string		mName;
	JSClassRef		mClass = nullptr;
	JSContextRef	mContext = nullptr;
};

//	make this generic for v8 & jscore
//	it should also be a Soy::TUniform type
class JsCore::TObject //: public Bind::TObject
{
	friend class TPersistent;
public:
	TObject()	{}	//	for arrays
	TObject(JSContextRef Context,JSObjectRef This);	//	if This==null then it's the global
	
	template<typename TYPE>
	TYPE&				This();

	virtual Bind::TObject	GetObject(const std::string& MemberName) bind_override;
	virtual std::string		GetString(const std::string& MemberName) bind_override;
	virtual uint32_t		GetInt(const std::string& MemberName) bind_override;
	virtual float			GetFloat(const std::string& MemberName) bind_override;
	virtual Bind::TFunction	GetFunction(const std::string& MemberName) bind_override;

	virtual void			SetObject(const std::string& Name,const Bind::TObject& Object) bind_override;
	virtual void			SetFunction(const std::string& Name,Bind::TFunction& Function) bind_override;
	virtual void			SetFloat(const std::string& Name,float Value) bind_override;
	virtual void			SetString(const std::string& Name,const std::string& Value) bind_override;
	virtual void			SetArray(const std::string& Name,Bind::TArray& Array) bind_override;
	virtual void			SetArray(const std::string& Name,ArrayBridge<Bind::TObject>&& Values) bind_override;
	virtual void			SetArray(const std::string& Name,ArrayBridge<bool>&& Values) bind_override;
	virtual void			SetInt(const std::string& Name,uint32_t Value) bind_override;

	//	Jscore specific
private:
	JSValueRef		GetMember(const std::string& MemberName);
	void			SetMember(const std::string& Name,JSValueRef Value);
	JSContextRef	mContext = nullptr;

protected:
	//	gr: not pure so we can still return an instance without rvalue'ing it
	virtual void*	GetThis() bind_override;

public:
	JSObjectRef		mThis = nullptr;
};



class JsCore::TPromise
{
public:
	//TPromise(JSContextRef Context,JSValueRef Promise,JSValueRef ResolveFunc,JSValueRef RejectFunc);
	TPromise(TObject& Promise,TFunction& Resolve,TFunction& Reject) :
		mPromise	( Promise ),
		mResolve	( Resolve ),
		mReject		( Reject )
	{
	}
	
	//	const for lambda[=] copy capture
	void			Resolve(const std::string& Value) const;
	void			Resolve(Bind::TObject& Value) const;
	void			Resolve(ArrayBridge<std::string>&& Values) const;
	void			Resolve(ArrayBridge<float>&& Values) const;
	void			Resolve(Bind::TArray& Value) const;
	//void			Resolve(JSValueRef Value) const	{	mResolve.Call(nullptr,Value);	}
	
	void			Reject(const std::string& Value) const;
	//void			Reject(JSValueRef Value) const	{	mReject.Call(nullptr,Value);	}
	
public:
	TObject			mPromise;
	TFunction		mResolve;
	TFunction		mReject;
};



class JsCore::TPersistent
{
public:
	TPersistent()	{}
	TPersistent(TObject& Object);	//	inc refcount
	TPersistent(TFunction& Object);	//	inc refcount
	~TPersistent();					//	dec refound
	
	operator	bool() const		{	return IsFunction() || IsObject();	}
	bool		IsFunction() const;
	bool		IsObject() const;
	
	//	const for lambda[=] capture
	TObject		GetObject() const		{	return mObject;	}
	TFunction	GetFunction() const		{	return mFunction;	}
	
private:
	TObject		mObject;
	TFunction	mFunction;
};


class Bind::TObjectWrapperBase
{
public:
	TObjectWrapperBase(TContext& Context,TObject& This) :
		mHandle		( This ),
		mContext	( Context )
	{
	}
	virtual ~TObjectWrapperBase()	{}

	TObject						GetHandle()	{	return mHandle.GetObject();	}
	
	//	construct and allocate
	virtual void 	Construct(TCallback& Arguments)=0;
	
	template<typename TYPE>
	//static TObjectWrapperBase*	Allocate(Bind::TContext& Context,Bind::TObject& This)
	static TYPE*	Allocate(TContext& Context,TObject& This)
	{
		return new TYPE( Context, This );
	}

protected:
	TPersistent		mHandle;
	TContext&		mContext;
};


//	template name? that's right, need unique references.
//	still working on getting rid of that, but still allow dynamic->static function binding
template<const char* TYPENAME,class TYPE>
class Bind::TObjectWrapper : public TObjectWrapperBase
{
public:
	//typedef std::function<TObjectWrapper<TYPENAME,TYPE>*(TV8Container&,v8::Local<v8::Object>)> ALLOCATORFUNC;
	
public:
	TObjectWrapper(TContext& Context,TObject& This);
	
	static std::string		GetTypeName()	{	return TYPENAME;	}
	static void				CreateTemplate(TTemplate& Template);
	
	
protected:
	
protected:
	std::shared_ptr<TYPE>			mObject;
	
protected:
};





template<const char* FunctionName>
inline void JsCore::TContext::BindGlobalFunction(std::function<void(Bind::TCallback&)> Function,const std::string& ParentName)
{
	static std::function<void(Bind::TCallback&)> FunctionCache = Function;
	static TContext* ContextCache = nullptr;
	JSObjectCallAsFunctionCallback CFunc = [](JSContextRef Context,JSObjectRef Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef* Exception)
	{
		return ContextCache->CallFunc( FunctionCache, Context, Function, This, ArgumentCount, Arguments, *Exception, FunctionName );
	};
	ContextCache = this;
	/*
	auto RawFunction = [](const v8::FunctionCallbackInfo<v8::Value>& Paramsv8)
	{
		return CallFunc( FunctionCache, Paramsv8, *ContainerCache );
	};
	ContainerCache = this;
	 */
	BindRawFunction( FunctionName, ParentName, CFunc );
}



template<typename TYPE>
inline Bind::TArray Bind::TContext::CreateArray(ArrayBridge<TYPE>&& Values)
{
	auto GetElement = [&](size_t Index)
	{
		return Values[Index];
	};
	auto Array = CreateArray( Values.GetSize(), GetElement );
	return Array;
}


template<typename TYPE>
inline TYPE& Bind::TCallback::GetArgumentPointer(size_t Index)
{
	auto* Ptr = GetArgumentPointer(Index);
	return *reinterpret_cast<TYPE*>( Ptr );
}

template<typename TYPE>
inline TYPE& Bind::TCallback::This()
{
	auto* This = GetThis();
	return *reinterpret_cast<TYPE*>( This );
}

template<typename TYPE>
inline TYPE& Bind::TObject::This()
{
	auto* This = GetThis();
	auto* Wrapper = reinterpret_cast<TObjectWrapperBase*>( This );
	auto* TypeWrapper = dynamic_cast<TYPE*>( Wrapper );
	return *TypeWrapper;
}



template<typename OBJECTWRAPPERTYPE>
void JsCore::TContext::BindObjectType(const std::string& ParentName)
{
	//	create a template
	TTemplate Template( mContext, OBJECTWRAPPERTYPE::GetTypeName() );
	OBJECTWRAPPERTYPE::CreateTemplate( Template );
	Template.RegisterClassWithContext();
	mObjectTemplates.PushBack( Template );
}

template<typename TYPE>
JSObjectRef JsCore::GetArray(JSContextRef Context,ArrayBridge<TYPE>& Array)
{
	JSValueRef Values[Array.GetSize()];
	for ( auto i=0;	i<Array.GetSize();	i++ )
		Values[i] = GetValue( Context, Array[i] );
	
	size_t Counter = Array.GetSize();
	auto ValuesRemote = GetRemoteArray( Values, Counter );
	auto ValuesRemoteBridge = GetArrayBridge( ValuesRemote );

	auto ArrayObject = GetArray( Context, ValuesRemoteBridge );
	return ArrayObject;
}

template<>
JSObjectRef JsCore::GetArray(JSContextRef Context,ArrayBridge<uint8_t>& Array)
{
	throw Soy::AssertException("Make typed array");
	//	make typed array
}


