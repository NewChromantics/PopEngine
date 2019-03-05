#pragma once

/*
	API binding.
 
 	Need a better name
*/
#include <string>

namespace Bind
{
	class TContainer;
	class TCallback;
	class TObject;
	class TTemplate;
}

typedef const std::string &TString;


class Bind::TContainer
{
public:
	virtual void	LoadScript(const std::string& Source,const std::string& Filename)=0;
	virtual void	CreateGlobalObjectInstance(TString ObjectType,TString Name)=0;
};


class Bind::TCallback
{
public:
	virtual size_t		GetArgumentCount() const=0;
	virtual std::string	GetArgumentString(size_t Index) const=0;
	virtual int32_t		GetArgumentInt(size_t Index) const=0;
	virtual void		GetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Array)=0;
	
	template<typename TYPE>
	TYPE&				This()
	{
		auto* This = GetThis();
		return reinterpret_cast<TYPE*>( This );
	}
	
	virtual void		Return()=0;			//	return undefined
	virtual void		ReturnNull()=0;
	virtual void		Return(const std::string& Value)=0;
	virtual void		Return(const uint32_t& Value)=0;
	virtual void		Return(const TObject& Value)=0;
	
protected:
	virtual void*		GetThis()=0;
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
class Bind::TObject
{
public:
	virtual TObject		GetObject(const std::string& MemberName);
	virtual std::string	GetString(const std::string& MemberName);
	virtual uint32_t	GetInt(const std::string& MemberName);
	virtual float		GetFloat(const std::string& MemberName);
	
	virtual void		SetObject(const std::string& Name,const TObject& Object);
};

