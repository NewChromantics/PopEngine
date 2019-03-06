#pragma once

/*
	API binding.
 
 	Need a better name
*/
#include <string>
#include "Array.hpp"

namespace Bind
{
	class TContext;
	class TCallback;
	class TObject;
	class TTemplate;

	class TPersistent;	//	might be nice to have TObject lockable maybe?
	class TPromise;
	
	//	see if we can get rid of these, in JsCore & v8, these are both objects
	//	(or is a function a value)
	class TFunction;
	class TArray;
}

typedef const std::string &TString;

//	a promise is always considered persistent
class Bind::TPromise
{
public:
	virtual void	Resolve(TObject Value)=0;
	//virtual void	Resolve(const std::string& Value)=0;
	
	//	this is usually an exception...
	virtual void	Reject(const std::string& Value)=0;
};

class Bind::TArray
{
};

class Bind::TFunction
{
};

class Bind::TContext
{
public:
	virtual TObject	CreateObjectInstance(const std::string& ObjectType=std::string())=0;
	TObject			GetGlobalObject(const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object
	virtual void	CreateGlobalObjectInstance(TString ObjectType,TString Name)=0;
	virtual std::shared_ptr<TPersistent>	CreatePersistent(TObject& Object)=0;
	virtual std::shared_ptr<TPersistent>	CreatePersistent(TFunction& Object)=0;
	virtual std::shared_ptr<TPromise>		CreatePromise()=0;

	//	see if we can get a better version of this
	virtual TArray	CreateArray(size_t ElementCount,std::function<std::string(size_t)> GetElement)=0;
	virtual TArray	CreateArray(size_t ElementCount,std::function<TObject(size_t)> GetElement)=0;
	template<typename TYPE>
	TArray			CreateArray(ArrayBridge<TYPE>&& Values);
	
	virtual void	LoadScript(const std::string& Source,const std::string& Filename)=0;
	//	consider using TCallback to wrap args + this
	virtual void	Execute(TFunction Function,TObject This,ArrayBridge<TObject>&& Args)=0;
	virtual void	Execute(std::function<void(TContext&)> Function)=0;
	virtual void	Queue(std::function<void(TContext&)> Function)=0;

	//	gr: maybe this is too API-Y and shouldn't be in such a generic base class
	std::string		GetResolvedFilename(const std::string& Filename);

protected:
	virtual TObject	GetRootGlobalObject()=0;
};


class Bind::TCallback
{
public:
	TCallback(TContext& Context) :
		mContext	( Context )
	{
	}
	
	virtual size_t		GetArgumentCount()=0;
	virtual std::string	GetArgumentString(size_t Index)=0;
	virtual int32_t		GetArgumentInt(size_t Index)=0;
	virtual float		GetArgumentFloat(size_t Index)=0;
	virtual bool		GetArgumentBool(size_t Index)=0;
	virtual void		GetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Array)=0;
	virtual void		GetArgumentArray(size_t Index,ArrayBridge<float>&& Array)=0;
	virtual TFunction	GetArgumentFunction(size_t Index)=0;
	virtual TObject		GetArgumentObject(size_t Index)=0;
	template<typename TYPE>
	TYPE&				GetArgumentPointer(size_t Index);

	virtual bool		IsArgumentString(size_t Index)=0;
	virtual bool		IsArgumentBool(size_t Index)=0;
	virtual bool		IsArgumentUndefined(size_t Index)=0;

	template<typename TYPE>
	TYPE&				This();
	virtual TObject		ThisObject()=0;
	
	virtual void		Return()=0;			//	return undefined
	virtual void		ReturnNull()=0;
	virtual void		Return(const std::string& Value)=0;
	virtual void		Return(uint32_t Value)=0;
	virtual void		Return(TObject Value)=0;
	virtual void		Return(TArray Value)=0;
	virtual void		Return(TPromise Value)=0;

	std::string			GetResolvedFilename(const std::string& Filename)
	{
		return mContext.GetResolvedFilename(Filename);
	}

protected:
	virtual void*		GetThis()=0;
	virtual void*		GetArgumentPointer(size_t Index)=0;
	
public:
	TContext&			mContext;
};





class Bind::TTemplate
{
public:
	//	generic
	bool			operator==(const std::string& Name) const	{	return this->mName == Name;	}
	
public:
	std::string		mName;
};


//	this should also be a Soy::TUniform type
//	need to differentiate between Object and void* object!
class Bind::TObject
{
public:
	template<typename TYPE>
	TYPE&				This();

	//	gr: these should all be pure
	//	member access
	virtual TObject		GetObject(const std::string& MemberName);
	virtual std::string	GetString(const std::string& MemberName);
	virtual uint32_t	GetInt(const std::string& MemberName);
	virtual float		GetFloat(const std::string& MemberName);
	
	virtual void		Set(const std::string& MemberName,const TObject& Value);
	virtual void		Set(const std::string& MemberName,const std::string& Value);

protected:
	//	gr: not pure so we can still return an instance without rvalue'ing it
	virtual void*		GetThis()	{	throw Soy::AssertException("Overload me");	}
};


class Bind::TPersistent
{
public:
	TPersistent(TObject& Object);		//	acquire
	TPersistent(TFunction& Function);	//	acquire
	virtual ~TPersistent()=0;			//	release
	
	std::unique_ptr<TObject>	mObject;
	std::unique_ptr<TFunction>	mFunction;
};



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
	return *reinterpret_cast<TYPE*>( This );
}
