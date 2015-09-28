#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable

#define DECLARE_DYNAMIC_ARRAY(TYPE)		\
typedef struct							\
{										\
	__global TYPE*			mData;		\
	volatile __global int*	mOffset;	\
	int				mMax;		\
} TArray_ ## TYPE;						\
\
static bool PushArrayGetIndex_ ## TYPE(TArray_ ## TYPE Array,const TYPE* Value,int* pNewIndex)	\
{		\
	int NewIndex = atomic_inc( Array.mOffset );	/*get next unused index*/	\
	bool Success = (NewIndex < Array.mMax);		/*out of space*/			\
	NewIndex = min( NewIndex, Array.mMax-1 );	/* dont go out of bounds */	\
	Array.mData[NewIndex] = *Value;			\
	*pNewIndex = NewIndex;				\
	return Success;		\
}						\
static bool PushArray_ ## TYPE(TArray_ ## TYPE Array,TYPE* Value)		\
{	\
	int NewIndex = atomic_inc( Array.mOffset );	/*get next unused index*/	\
	bool Success = (NewIndex < Array.mMax);		/*out of space*/			\
	NewIndex = min( NewIndex, Array.mMax-1 );	/* dont go out of bounds */	\
	Array.mData[NewIndex] = *Value;			\
	return Success;		\
}	\
\




