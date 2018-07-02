
__kernel void GetTestLines(global float4* Lines,global int* LineCount,int LinesSize)
{
	Lines[0] = (float4)( 0, 0, 1, 1 );
	Lines[1] = (float4)( 0.5, 0, 0.5, 1 );
	Lines[2] = (float4)( 1, 0, 0, 1 );
	Lines[3] = (float4)( 0, 0.5, 1, 0.5 );
	LineCount[0] = 4;
	//int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	//int2 wh = get_image_dim(Hsl);
}
