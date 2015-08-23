#define const	__constant
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable

#define DECLARE_DYNAMIC_ARRAY(TYPE)		\
typedef struct							\
{										\
	__global TYPE*			mData;		\
	volatile __global int*	mOffset;	\
	int				mMax;		\
} TArray_ ## TYPE;						\
\
static bool PushArrayGetIndex_ ## TYPE(TArray_ ## TYPE Array,const TYPE* Value,int* pNewIndex)	{		\
	int NewIndex = atomic_inc( Array.mOffset );	/*get next unused index*/	\
	bool Success = (NewIndex < Array.mMax);		/*out of space*/			\
	NewIndex = min( NewIndex, Array.mMax-1 );	/* dont go out of bounds */	\
	Array.mData[NewIndex] = *Value;			\
	*pNewIndex = NewIndex;				\
	return Success;		\
}						\
static bool PushArray_ ## TYPE(TArray_ ## TYPE Array,TYPE* Value)	{	\
	int NewIndex = atomic_inc( Array.mOffset );	/*get next unused index*/	\
	bool Success = (NewIndex < Array.mMax);		/*out of space*/			\
	NewIndex = min( NewIndex, Array.mMax-1 );	/* dont go out of bounds */	\
	Array.mData[NewIndex] = *Value;			\
	return Success;		\
}						\

DECLARE_DYNAMIC_ARRAY(float4);

const float2 MaxRectSize = (float2)(80,100);

static bool MakeRectMatch(float4* Match,float2 TexCoord,float2 TexSize,float4 Sample)
{
	float2 RectSize = Sample.xy * MaxRectSize;
	float2 Alignment = Sample.zw * MaxRectSize;
	
	//	if width or height zero then not a match
	if ( RectSize.x == 0 || RectSize.y == 0 )
		return false;

	float2 Min = TexCoord - Alignment;
	float2 Max = Min + RectSize;
	
	//	store min/max of rect
	(*Match)[0] = Min.x;
	Match->y = Min.y;
	Match[2] = Max.x;
	Match[3] = Max.y;
	
	return true;
}


static float4 texture2D(__read_only image2d_t Image,float2 uv)
{
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	
	return read_imagef( Image, Sampler, uv );
}


__kernel void GatherRects(int OffsetX,int OffsetY,__read_only image2d_t rectfilter,
							global float4*			Matches,
							global volatile int*	MatchesCount,
							int						MatchesMax)
{
	float tx = get_global_id(0) + OffsetX;
	float ty = get_global_id(1) + OffsetY;
	float2 wh = (float2)get_image_dim(rectfilter);
	float w = wh.x;
	float h = wh.y;
	float2 TexCoord = (float2)( tx, ty );
	
	float4 Sample = texture2D( rectfilter, TexCoord );
	//float4 Match = (float4)(tx,ty,get_global_id(0),get_global_id(1));
	float4 Match;
	//float4 Match = (float4)(1,2,3,4);
	if ( !MakeRectMatch( &Match, TexCoord, wh, Sample ) )
		return;

	TArray_float4 MatchArray = { Matches, MatchesCount, MatchesMax };
	PushArray_float4( MatchArray, &Match );

}

