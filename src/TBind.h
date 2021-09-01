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
	template<typename QUEUETYPE>
	class TPromiseQueueObjects;
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
	void			Flush(Bind::TLocalContext& Context,std::function<void(Bind::TLocalContext&,TPromise&)> HandlePromise);

	void			Resolve();
	void			Resolve(const std::string& Result);
	void			Resolve(Bind::TObject& Object);
	void			Reject(const std::string& Error);
	
	void			Resolve(Bind::TLocalContext& Context);
	void			Resolve(Bind::TLocalContext& Context,const std::string& Result);
	void			Resolve(Bind::TLocalContext& Context,Bind::TObject& Object);
	void			Reject(Bind::TLocalContext& Context,const std::string& Error);

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
	void			QueueFlush(size_t PromiseRef, std::function<void(Bind::TLocalContext&, TPromise&)> HandlePromise);
	void			QueueResolve(size_t PromiseRef);
	void			QueueReject(size_t PromiseRef, const std::string& Error);

private:
	std::shared_ptr<TPromise>	PopPromise(size_t PromiseRef);

private:
	Bind::TContext*	mContext = nullptr;
	std::mutex		mPromisesLock;
	Array<std::pair<size_t,std::shared_ptr<TPromise>>>	mPromises;
	size_t			mPromiseCounter = 0;
};


template<typename QUEUETYPE>
class Bind::TPromiseQueueObjects
{
public:
	TPromiseQueueObjects()
	{
		//	gr: when used, this is throwing a bad-execution thing in std::function
		mResolveObject = [](...)
		{
			std::Debug << __PRETTY_FUNCTION__ << " Resolved (not overloaded)" << std::endl;
		};
	}
	TPromiseQueueObjects(std::function<void(Bind::TPromise&,QUEUETYPE&)> ResolveObject) :
		mResolveObject	( ResolveObject )
	{
	}
	
	void				Push(const QUEUETYPE& Object);
	Bind::TPromise		AddPromise(Bind::TLocalContext& Context);
	
protected:
	//	overload this to do things like returning latest only
	//	would like to avoid this copy! for now, use smart pointers for large allocs
	//	set skip to abort resolve
	virtual QUEUETYPE	Pop(bool& Skip);

public:
	//	public for now, until there's a nice way to construct
	std::function<void(Bind::TLocalContext&,Bind::TPromise&,QUEUETYPE&)>	mResolveObject;

private:
	void				Flush();
	
private:
	Bind::TPromiseQueue		mPromiseQueue;
	std::mutex				mObjectsLock;
	Array<QUEUETYPE>		mObjects;
};


template<typename QUEUETYPE>
inline void Bind::TPromiseQueueObjects<QUEUETYPE>::Push(const QUEUETYPE& Object)
{
	{
		std::lock_guard<std::mutex> Lock(mObjectsLock);
		mObjects.PushBack(Object);
	}
	Flush();
}


template<typename QUEUETYPE>
inline Bind::TPromise Bind::TPromiseQueueObjects<QUEUETYPE>::AddPromise(Bind::TLocalContext& Context)
{
	auto Promise = mPromiseQueue.AddPromise(Context);
	//	gr: can I do the flush here?
	Flush();
	return Promise;
}

template<typename QUEUETYPE>
inline void Bind::TPromiseQueueObjects<QUEUETYPE>::Flush()
{
	if ( mObjects.IsEmpty() )
		return;
	if ( !mPromiseQueue.HasPromises() )
		return;
	
	auto Flush = [this](Bind::TLocalContext& Context) mutable
	{
		//	gr: try and avoid this copy without QUEUETYPE resorting to sharedptr
		bool Skip=false;
		auto Popped = Pop(Skip);

		//	gr: sometimes, probbaly because there's no mutex on mFrames.IsEmpty() above
		//		we get have an empty mFrames (this lambda is probably executing) and we have zero frames,
		//		abort the resolve
		if ( Skip )
			return;

		auto ResolveObject = [&](Bind::TLocalContext& Context,TPromise& Promise)
		{
			this->mResolveObject( Context, Promise, Popped );
		};
		
		mPromiseQueue.Flush(ResolveObject);
	};
	auto& Context = mPromiseQueue.GetContext();
	Context.Queue(Flush);
}

template<typename QUEUETYPE>
inline QUEUETYPE Bind::TPromiseQueueObjects<QUEUETYPE>::Pop(bool& Skip)
{
	std::lock_guard<std::mutex> Lock(mObjectsLock);
	if ( mObjects.IsEmpty() )
	{
		Skip = true;
		return QUEUETYPE();
	}
	
	auto FirstObject = mObjects.PopAt(0);
	return FirstObject;
}
