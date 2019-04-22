#pragma once

/*
	API binding.
 
 	Need a better name
*/
#include <string>
#include "Array.hpp"

//	gr: to deal with different attributes on different platforms... lets make a macro
#define DEFINE_BIND_FUNCTIONNAME(Name)	extern const char Name ## _FunctionName[] = #Name
#define DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Name,ApiName)	extern const char Name ## _FunctionName[] = #ApiName
#define DEFINE_BIND_TYPENAME(Name)		extern const char Name ## _TypeName[] = #Name
#define DECLARE_BIND_TYPENAME(Name)		extern const char Name ## _TypeName[];

#if defined(JSAPI_V8)
	#include "V8Bind.h"
	//namespace Bind = V8;
#else

namespace JsCore
{
	//class TInstance;
}
namespace Bind
{
	using namespace JsCore;
	//typedef JsCore::TInstance TInstance;
}
	#include "JsCoreBind.h"
	//namespace Bind = JsCore;
#endif






//	thsese classes should be pretty agnostic
namespace Bind
{
	class TPromiseQueue;
}


class Bind::TPromiseQueue
{
public:
	bool			HasContext()	{	return mContext != nullptr;	}	//	if not true, we've never requested
	Bind::TContext&	GetContext();
	bool			PopMissedFlushes()
	{
		if ( mMissedFlushes == 0 )
			return false;
		mMissedFlushes = 0;
		return true;
	}
	
	TPromise		AddPromise(Bind::TContext& Context);
	bool			HasPromises() const	{	return !mPending.IsEmpty();	}

	//	callback so you can handle how to resolve the promise rather than have tons of overloads here
	void			Flush(std::function<void(TPromise&)> HandlePromise);

	void			Resolve();
	void			Resolve(const std::string& Result);
	void			Reject(const std::string& Error);
	
private:
	size_t			mMissedFlushes = 0;
	Bind::TContext*	mContext = nullptr;
	std::mutex		mPendingLock;
	Array<TPromise>	mPending;
};

