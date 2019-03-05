#pragma once

/*
	API binding.
 
 	Need a better name
*/
#include <string>

namespace Bind
{
	class TContainer;
	class TCallbackInfo;	//	rename to Meta
}

typedef const std::string &TString;


class Bind::TContainer
{
public:
	virtual void	CreateGlobalObjectInstance(TString ObjectType,TString Name)=0;
};


class Bind::TCallbackInfo
{
public:
	virtual size_t		GetArgumentCount() const=0;
	virtual std::string	GetArgumentString(size_t Index) const=0;
	virtual int32_t		GetArgumentInt(size_t Index) const=0;
};

