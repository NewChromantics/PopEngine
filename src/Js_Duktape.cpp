#include "Js_Duktape.h"



std::function<void(int,int,const char*)>* Js::TContext::mPrint = nullptr;


void OnError(void* pContext,const char* Error)
{
	if ( !pContext )
		return;
	auto& Context = *reinterpret_cast<Js::TContext*>(pContext);
	Context.mOnError( Error );
}

#define HEAP_SIZE	( 1024*1024*1 )
size_t HeapOffset = 0;
uint8_t Heap[HEAP_SIZE];

#include <SoyDebug.h>

void* Alloc(void* pContext,duk_size_t Size)
{
	uint8_t* NextData = &Heap[HeapOffset];
	for ( int i=0;	i<Size;	i++ )
		NextData[i] = 0;
	HeapOffset += Size;
	std::Debug << "Allocated " << Size << " now using " << HeapOffset << "/" << HEAP_SIZE << std::endl;
	return NextData;
}


void* Realloc(void* pContext,void* OldData,duk_size_t Size)
{
	return Alloc( pContext, Size );
}


void Free(void* pContext,void* Data)
{
}


Js::TContext::TContext(std::function<void(const char*)> OnError) :
	mContext	( nullptr ),
	mOnError	( OnError )
{
	HeapOffset = 0;
	
	if ( mOnError == nullptr )
		mOnError = [](const char* Error)	{};

	//mContext = duk_create_heap_default();
	mContext = duk_create_heap( Alloc, Realloc, Free, this, ::OnError );
	
	/*
	auto Print = [](duk_context* Context) ->duk_ret_t
	{
		auto& PrintFunc = *Js::TContext::mPrint;
		PrintFunc( 0,0,"I am Print()!");
		return 0;
	};
	 
	duk_push_c_function( mContext, Print, 3);
	duk_put_global_string( mContext, "print");
	*/
}

Js::TContext::~TContext()
{
	duk_destroy_heap( mContext );
}

void Js::TContext::Execute(const char* Script)
{
	duk_eval_string( mContext, Script );
	duk_pop( mContext );	//	pop eval result
}


