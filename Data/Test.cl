
__kernel void DebugUv(__write_only image2d_t Frag,float2 Frag_PixelWidthHeight,int OffsetX,int OffsetY)
{
	//const sampler_t Sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	
	float x = get_global_id(0) + OffsetX;
	float y = get_global_id(1) + OffsetY;
	float w = Frag_PixelWidthHeight.x;
	float h = Frag_PixelWidthHeight.y;
	
	float4 rgba = float4( x/w, y/h, 0, 1 );
	
	int2 uv = int2( x, y );
	
	write_imagef( Frag, uv, rgba );
}

__kernel void RunTest(int OffsetX,int OffsetY)
{
}

