#define const	__constant

const bool IncludeSelf = true;



static float4 texture2D(__read_only image2d_t Image,int2 uv,int2 PixelOffset)
{
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	
	return read_imagef( Image, Sampler, uv + PixelOffset );
}



__kernel void AlphaFill_GreenRed(int OffsetX,int OffsetY,__read_only image2d_t grassfilter,__write_only image2d_t Frag)
{
	__read_only image2d_t Source = grassfilter;
	float x = get_global_id(0) + OffsetX;
	float y = get_global_id(1) + OffsetY;
	int2 uv = (int2)( x, y );

	float4 ThisSample = texture2D( Source, uv, (int2)(0,0) );
	int HitCount = 0;
#define SampleRadius	4	//	range 0,9
	int HitCountMin = 8;

#pragma unroll 
	for ( int y=-SampleRadius;	y<=SampleRadius;	y++ )
	{
#pragma unroll 
		for ( int x=-SampleRadius;	x<=SampleRadius;	x++ )
		{
			bool Ignore = ( !IncludeSelf && y==0 && x==0 );

			float4 NeighbourSample = texture2D( Source, uv, (int2)(x,y) );
			bool NeighbourHit = (NeighbourSample.w > 0.5f);
			HitCount += ((!Ignore) && NeighbourHit) ? 1 : 0;
		}
	}

	bool Filled = ( HitCount > HitCountMin );
	ThisSample.xyzw = Filled ? (float4)(0,1,0,1) : (float4)(1,0,0,0);
	write_imagef( Frag, uv, ThisSample );
}


__kernel void AlphaFill_Centroids(int OffsetX,int OffsetY,__read_only image2d_t Centroids,__write_only image2d_t Frag)
{
	__read_only image2d_t Source = Centroids;
	float x = get_global_id(0) + OffsetX;
	float y = get_global_id(1) + OffsetY;
	int2 uv = (int2)( x, y );
	
	float4 ThisSample = texture2D( Source, uv, (int2)(0,0) );
	int HitCount = 0;
#undef SampleRadius
#define SampleRadius	2	//	range 0,9
	int HitCountMin = 1;

#pragma unroll
	for ( int y=-SampleRadius;	y<=SampleRadius;	y++ )
	{
#pragma unroll
		for ( int x=-SampleRadius;	x<=SampleRadius;	x++ )
		{
			bool Ignore = ( !IncludeSelf && y==0 && x==0 );
			
			float4 NeighbourSample = texture2D( Source, uv, (int2)(x,y) );
			bool NeighbourHit = (NeighbourSample.w > 0.5f);
			HitCount += ((!Ignore) && NeighbourHit) ? 1 : 0;
			
			if ( NeighbourHit && ThisSample.w == 0 )
				ThisSample.xyz += NeighbourSample.xyz;
		}
	}
	
	//	if source was transparent, we become a blurred colour
	if ( ThisSample.w == 0 )
	{
		ThisSample /= (float)(max(HitCount,1));
	}

	bool Filled = ( HitCount > HitCountMin );
	ThisSample.w = Filled ? 1 : 0;
	ThisSample.xyzw = Filled ? 1 : 0;
	write_imagef( Frag, uv, ThisSample );
}