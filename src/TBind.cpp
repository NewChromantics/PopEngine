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


Bind::TPromise Bind::TPromiseQueue::AddPromise(Bind::TLocalContext& Context)
{
	if ( mContext != &Context.mGlobalContext && mContext != nullptr )
	{
		throw Soy::AssertException("Promise queue context has changed");
	}
	mContext = &Context.mGlobalContext;
	
	std::lock_guard<std::mutex> Lock( mPendingLock );
	auto NewPromise = Context.mGlobalContext.CreatePromise( Context, __FUNCTION__ );
	mPending.PushBack( NewPromise );
	return NewPromise;
}


void Bind::TPromiseQueue::Flush(std::function<void(Bind::TLocalContext& Context,Bind::TPromise&)> HandlePromise)
{
	//	grab a copy so the queue never becomes infinite
	Array<TPromise> Promises;
	{
		std::lock_guard<std::mutex> Lock(mPendingLock);
		Promises = mPending;
		mPending.Clear();
	}
	
	if ( Promises.IsEmpty() )
		mMissedFlushes++;

	auto& Context = *this->mContext;
	
	//	gr: should we block or not...
	auto DoFlush = [&](Bind::TLocalContext& Context)
	{
		for ( auto p=0;	p<Promises.GetSize();	p++ )
		{
			auto& Promise = Promises[p];
			HandlePromise( Context, Promise );
		}
	};
	Context.Execute( DoFlush );
}

void Bind::TPromiseQueue::Resolve()
{
	auto Handle = [](Bind::TLocalContext& Context,TPromise& Promise)
	{
		Promise.ResolveUndefined(Context);
	};
	Flush( Handle );
}

void Bind::TPromiseQueue::Resolve(const std::string& Result)
{
	auto Handle = [=](Bind::TLocalContext& Context,TPromise& Promise)
	{
		Promise.Resolve(Context,Result);
	};
	Flush(Handle);
}

void Bind::TPromiseQueue::Reject(const std::string& Error)
{
	auto Handle = [=](Bind::TLocalContext& Context,TPromise& Promise)
	{
		Promise.Reject( Context, Error );
	};
	Flush(Handle);
}

