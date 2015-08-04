
__kernel void DebugUv(__read_only image2d_t Frame,__write_only image2d_t Destination,int OffsetX,int OffsetY)
{
	//const sampler_t Sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	
	float x = get_global_id(0);
	float y = get_global_id(1);
	float w = get_global_size(0);
	float h = get_global_size(1);
	
	float4 rgba = float4( x/w, y/h, 0, 1 );
	
	int2 uv = int2( x, y );
	
	write_imagef( Destination, uv, rgba );
}

__kernel void RunTest(int OffsetX,int OffsetY)
{
}

