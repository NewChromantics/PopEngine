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
	
	//	see if we can get rid of these, in JsCore & v8, these are both objects
	//	(or is a function a value)
	class TFunction;
	class TArray;
}

typedef const std::string &TString;


class Bind::TArray
{
};

class Bind::TContext
{
public:
	virtual TObject	CreateObjectInstance(const std::string& ObjectType=std::string())=0;
	TObject			GetGlobalObject(const std::string& ObjectName=std::string());	//	get an object by it's name. empty string = global/root object

	//	see if we can get a better version of this
	virtual TArray	CreateArray(size_t ElementCount,std::function<std::string(size_t)> GetElement)=0;
	virtual TArray	CreateArray(size_t ElementCount,std::function<TObject(size_t)> GetElement)=0;

	virtual void	LoadScript(const std::string& Source,const std::string& Filename)=0;
	virtual void	CreateGlobalObjectInstance(TString ObjectType,TString Name)=0;
	virtual std::string	GetResolvedFilename(const std::string& Filename)=0;

protected:
	//	create object and return a final Bake() function
	virtual std::function<void()>	CreateArrayObject(size_t ElementCount)=0;
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
	virtual void		GetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Array)=0;
	virtual TFunction	GetArgumentFunction(size_t Index)=0;
	virtual TObject		GetArgumentObject(size_t Index)=0;
	template<typename TYPE>
	TYPE&				GetArgumentPointer(size_t Index);

	virtual bool		IsArgumentString(size_t Index)=0;
	virtual bool		IsArgumentBool(size_t Index)=0;

	template<typename TYPE>
	TYPE&				This();
	
	virtual void		Return()=0;			//	return undefined
	virtual void		ReturnNull()=0;
	virtual void		Return(const std::string& Value)=0;
	virtual void		Return(uint32_t Value)=0;
	virtual void		Return(TObject Value)=0;
	virtual void		Return(TArray Value)=0;
	
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
	virtual TObject		GetObject(const std::string& MemberName);
	virtual std::string	GetString(const std::string& MemberName);
	virtual uint32_t	GetInt(const std::string& MemberName);
	virtual float		GetFloat(const std::string& MemberName);
	
	virtual void		SetObject(const std::string& Name,const TObject& Object);
};

