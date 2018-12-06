#pragma once

/*
	API binding.
 
 	Need a better name
*/
#include <string>

namespace Bind
{
	class TCallbackInfo;	//	rename to Meta
}


class Bind::TCallbackInfo
{
public:
	virtual size_t		GetArgumentCount() const=0;
	virtual std::string	GetArgumentString(size_t Index) const=0;
};

