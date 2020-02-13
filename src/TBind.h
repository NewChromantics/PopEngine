#pragma once

/*
	API binding.
 
 	Need a better name
*/
#include <string>
#include "HeapArray.hpp"

//	gr: to deal with different attributes on different platforms... lets make a macro
//	gr: now to aid auto complete, declarations are
//		BindFunction::YourName
//	and
//		BindType::YourName
#define DEFINE_BIND_FUNCTIONNAME(Name)					namespace BindFunction	{	extern const char Name [] = #Name ;	}
#define DEFINE_BIND_FUNCTIONNAME_OVERRIDE(Name,ApiName)	namespace BindFunction	{	extern const char Name [] = #ApiName ;	}
#define DEFINE_BIND_TYPENAME(Name)						namespace BindType		{	extern const char Name [] = #Name ;	}
#define DECLARE_BIND_TYPENAME(Name)						namespace BindType		{	extern const char Name [] ;	}





//	these classes should be pretty agnostic
namespace Bind
{
	class TInstanceBase;
	class TPromiseQueue;
	class TPromiseMap;
}



class Bind::TInstanceBase
{
public:
	std::string						mRootDirectory;
	
protected:
	std::function<void(int32_t)>	mOnShutdown;	//	callback when we want to die
};



#if defined(JSAPI_V8)
#include "V8Bind.h"

#elif defined(JSAPI_CHAKRA)
#include "ChakraBind.h"

#elif defined(JSAPI_JSCORE)
//	nothing to include atm

#else
#error No Javascript API defined
#endif

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
	
	TPromise		AddPromise(Bind::TLocalContext& Context);
	bool			HasPromises() const	{	return !mPending.IsEmpty();	}

	//	callback so you can handle how to resolve the promise rather than have tons of overloads here
	void			Flush(std::function<void(Bind::TLocalContext&,TPromise&)> HandlePromise);

	void			Resolve();
	void			Resolve(const std::string& Result);
	void			Reject(const std::string& Error);
	
private:
	size_t			mMissedFlushes = 0;
	Bind::TContext*	mContext = nullptr;
	std::mutex		mPendingLock;
	Array<TPromise>	mPending;
};


//	bit of a temp class to get around captures in lambdas so we can pass simpler references
//	case: getting a race condition where JS thread allocs promise, returns it
//	captured in thread B, which creates a JS callback lambda, copies promise again and
//	both threads try and release promise
class Bind::TPromiseMap
{
public:
	bool			HasContext() { return mContext != nullptr; }	//	if not true, we've never requested
	Bind::TContext&	GetContext();
	
	TPromise		CreatePromise(Bind::TLocalContext& Context,const char* DebugName,size_t& PromiseRef);

	//	callback so you can handle how to resolve the promise rather than have tons of overloads here
	void			Flush(size_t PromiseRef,std::function<void(Bind::TLocalContext&, TPromise&)> HandlePromise);
	
private:
	Bind::TContext*	mContext = nullptr;
	std::mutex		mPromisesLock;
	Array<std::pair<size_t,std::shared_ptr<TPromise>>>	mPromises;
	size_t			mPromiseCounter = 0;
};

