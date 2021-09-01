#include "TBind.h"
#include <SoyString.h>



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


void Bind::TPromiseQueue::Flush(Bind::TLocalContext& Context,std::function<void(Bind::TLocalContext& Context,Bind::TPromise&)> HandlePromise)
{
	//	grab a copy so the queue never becomes infinite
	BufferArray<TPromise,10> Promises;
	{
		std::lock_guard<std::mutex> Lock(mPendingLock);
		Promises = mPending;
		mPending.Clear();
	}
	
	if ( Promises.IsEmpty() )
		mMissedFlushes++;
	
	auto DoFlush = [&](Bind::TLocalContext& Context)
	{
		for ( auto p=0;	p<Promises.GetSize();	p++ )
		{
			auto& Promise = Promises[p];
			try
			{
				HandlePromise( Context, Promise );
			}
			catch(std::exception& e)
			{
				Promise.Reject(Context,e.what());
			}
		}
	};
	DoFlush(Context);
}

void Bind::TPromiseQueue::Flush(std::function<void(Bind::TLocalContext& Context,Bind::TPromise&)> HandlePromise)
{
	auto& Context = *this->mContext;
	
	auto Run = [&](Bind::TLocalContext& Context)
	{
		this->Flush(Context,HandlePromise);
	};
	Context.Execute( Run );
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
	auto Handle = [&](Bind::TLocalContext& Context, TPromise& Promise) mutable
	{
		Promise.Resolve(Context, Result);
	};
	Flush(Handle);
}

void Bind::TPromiseQueue::Resolve(Bind::TObject& Result)
{
	auto Handle = [&](Bind::TLocalContext& Context, TPromise& Promise)
	{
		Promise.Resolve(Context, Result);
	};
	Flush(Handle);
}

void Bind::TPromiseQueue::Resolve(Bind::TLocalContext& Context,Bind::TObject& Result)
{
	auto Handle = [&](Bind::TLocalContext& Context, TPromise& Promise)
	{
		Promise.Resolve(Context, Result);
	};
	Flush(Context,Handle);
}

void Bind::TPromiseQueue::Reject(const std::string& Error)
{
	auto Handle = [=](Bind::TLocalContext& Context,TPromise& Promise)
	{
		Promise.Reject( Context, Error );
	};
	Flush(Handle);
}

void Bind::TPromiseQueue::Reject(Bind::TLocalContext& Context,const std::string& Error)
{
	auto Handle = [=](Bind::TLocalContext& Context,TPromise& Promise)
	{
		Promise.Reject( Context, Error );
	};
	Flush( Context, Handle );
}


Bind::TPromise Bind::TPromiseMap::CreatePromise(Bind::TLocalContext& Context,const char* DebugName,size_t& PromiseRef)
{
	if (mContext != &Context.mGlobalContext && mContext != nullptr)
	{
		throw Soy::AssertException("Promise queue context has changed");
	}
	mContext = &Context.mGlobalContext;

	std::lock_guard<std::mutex> Lock(mPromisesLock);
	PromiseRef = mPromiseCounter++;
	auto NewPromise = Context.mGlobalContext.CreatePromisePtr(Context, DebugName);
	auto Pair = std::make_pair(PromiseRef, NewPromise);
	mPromises.PushBack(Pair);
	return *NewPromise;
}


void Bind::TPromiseMap::Flush(size_t PromiseRef,std::function<void(Bind::TLocalContext& Context, Bind::TPromise&)> HandlePromise)
{
	std::shared_ptr<TPromise> Promise;

	{
		//	pop this promise
		std::lock_guard<std::mutex> Lock(mPromisesLock);
		for (auto i = 0; i < mPromises.GetSize(); i++)
		{
			auto& Element = mPromises[i];
			auto& MatchRef = Element.first;
			if (MatchRef != PromiseRef)
				continue;
			Promise = Element.second;
			mPromises.RemoveBlock(i, 1);
			break;
		}
		if (!Promise)
			throw Soy::AssertException("Promise Ref not found");
	}
	
	auto& Context = *this->mContext;

	//	gr: should we block or not...
	auto DoFlush = [&](Bind::TLocalContext& Context)
	{
		HandlePromise(Context, *Promise);
		//	clear promise whilst in js thread
		Promise.reset();
	};
	Context.Execute(DoFlush);
}

void Bind::TPromiseMap::QueueFlush(size_t PromiseRef,std::function<void(Bind::TLocalContext& Context, Bind::TPromise&)> HandlePromise)
{
	auto Promise = PopPromise(PromiseRef);
	auto& Context = *this->mContext;

	auto DoFlush = [=,Promise=move(Promise)](Bind::TLocalContext& Context) mutable
	{
		HandlePromise(Context, *Promise);

		//	gr: if we wait, this gets down to 0.
		//	the other thread is still finishing Context.Queue
		//	and use count can be 3. I think the JS queue is executing this job, before the outer func has
		//	finished, where it disposes of this lambda copy
		//	we need a nicer thing to say this lambda is the only instance at this point
		//	or maybe we can pass lambda as a unique?
		//	gr: move() in capture nulls the outer promise... may have solved all problems?
		//	also moved this to AFTER the call, so any pause is at the end
		while (Promise.use_count() > 1)
		{
			auto PromiseRefCountC = Promise.use_count();
			//if (PromiseRefCountC != 1)
			std::Debug << "Warning promise ref count is not 1; " << PromiseRefCountC << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		//	clear promise whilst in js thread
		Promise.reset();
	};
	//	gr: with move() this is now null'd by here
	auto PromiseRefCountB = Promise.use_count();
	Promise.reset();
	Context.Queue(DoFlush);
}

std::shared_ptr<Bind::TPromise> Bind::TPromiseMap::PopPromise(size_t PromiseRef)
{
	//	pop this promise
	std::lock_guard<std::mutex> Lock(mPromisesLock);
	for (auto i = 0; i < mPromises.GetSize(); i++)
	{
		auto& Element = mPromises[i];
		auto& MatchRef = Element.first;
		if (MatchRef != PromiseRef)
			continue;
		auto Promise = Element.second;
		mPromises.RemoveBlock(i, 1);
		return Promise;
	}
	throw Soy::AssertException("Promise Ref not found");
}

void Bind::TPromiseMap::QueueResolve(size_t PromiseRef)
{
	auto Promise = PopPromise(PromiseRef);
	Promise->ResolveUndefined();
}

void Bind::TPromiseMap::QueueReject(size_t PromiseRef,const std::string& Error)
{
	auto Promise = PopPromise(PromiseRef);
	Promise->Reject(Error);
}