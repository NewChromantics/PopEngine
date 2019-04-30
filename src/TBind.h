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





//	these classes should be pretty agnostic
namespace Bind
{
	class TInstanceBase;
	class TPromiseQueue;
}



class Bind::TInstanceBase
{
protected:
	std::string						mRootDirectory;
	std::function<void(int32_t)>	mOnShutdown;	//	callback when we want to die
};



#if defined(JSAPI_V8)
#include "V8Bind.h"
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

