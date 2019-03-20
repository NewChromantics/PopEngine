#include "TBind.h"
#include "SoyLib/src/SoyString.h"



Bind::TContext& Bind::TPromiseQueue::GetContext()
{
	if ( !mContext )
	{
		throw Soy::AssertException("Context not set, no promises must have been added");
	}
	return *mContext;
}


Bind::TPromise Bind::TPromiseQueue::AddPromise(Bind::TContext& Context)
{
	if ( mContext != &Context && mContext != nullptr )
	{
		throw Soy::AssertException("Promise queue context has changed");
	}
	mContext = &Context;
	
	std::lock_guard<std::mutex> Lock( mPendingLock );
	auto NewPromise = Context.CreatePromise();
	mPending.PushBack( NewPromise );
	return NewPromise;
}


void Bind::TPromiseQueue::Flush(std::function<void(Bind::TPromise&)> HandlePromise)
{
	//	grab a copy so the queue never becomes infinite
	Array<TPromise> Promises;
	{
		std::lock_guard<std::mutex> Lock(mPendingLock);
		Promises = mPending;
		mPending.Clear();
	}

	for ( auto p=0;	p<Promises.GetSize();	p++ )
	{
		auto& Promise = Promises[p];
		HandlePromise( Promise );
	}
}

void Bind::TPromiseQueue::Resolve()
{
	auto Handle = [](TPromise& Promise)
	{
		Promise.ResolveUndefined();
	};
	Flush(Handle);
}

void Bind::TPromiseQueue::Resolve(const std::string& Result)
{
	auto Handle = [&](TPromise& Promise)
	{
		Promise.Resolve(Result);
	};
	Flush(Handle);
}

void Bind::TPromiseQueue::Reject(const std::string& Error)
{
	auto Handle = [&](TPromise& Promise)
	{
		Promise.Reject(Error);
	};
	Flush(Handle);
}

