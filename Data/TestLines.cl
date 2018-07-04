
__kernel void GetTestLines(global float4* Lines,global int* LineCount,int LinesSize)
{
	Lines[0] = (float4)( 0, 0, 1, 1 );
	Lines[1] = (float4)( 1, 0, 0, 1 );
	LineCount[0] = 2;
	//int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	//int2 wh = get_image_dim(Hsl);
}
