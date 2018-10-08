#pragma once

#define DUK_ASSERT(x)	do{ if(!x) Js::OnAssert();	}while(0)
#include "duktape-2.3.0/src/duktape.h"
#include <functional>


namespace Js
{
	class TContext;
	void	OnAssert();
}


class Js::TContext
{
public:
	TContext(std::function<void(const char*)> OnError);
	~TContext();
	
	void		Execute(const char* Script);
	
	void		BindPrint();
	
	public:
	static std::function<void(int,int,const char*)>*	mPrint;
	duk_context*	mContext;
	std::function<void(const char*)>	mOnError;
};

