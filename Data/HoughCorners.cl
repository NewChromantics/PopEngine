#include "Common.cl"
#include "Array.cl"

DECLARE_DYNAMIC_ARRAY(float4);


__kernel void ExtractHoughCorners(int OffsetHoughLineAIndex,
								  int OffsetHoughLineBIndex,
								  global float8* HoughLines,
								  int HoughLineCount,
								  global float4* HoughCorners
								  )
{
	int HoughLineAIndex = get_global_id(0) + OffsetHoughLineAIndex;
	int HoughLineBIndex = get_global_id(1) + OffsetHoughLineBIndex;
	float8 HoughLineA = HoughLines[HoughLineAIndex];
	float8 HoughLineB = HoughLines[HoughLineBIndex];
	int CornerIndex = (HoughLineAIndex*HoughLineCount) + HoughLineBIndex;
	
	float2 Intersection = 0;
	float Score = 0;
	float w = 0;

	//	same-index will always intersect, (or be parallel?) score zero
	//	and we only need to compare lines once. so B must be >A
	if ( HoughLineAIndex < HoughLineBIndex )
	{
		float3 Intersection3 = GetLineLineInfiniteIntersection( HoughLineA.xyzw, HoughLineB.xyzw );
		Intersection = Intersection3.xy;
	
		//	just for neat output
		Intersection.x = round(Intersection.x);
		Intersection.y = round(Intersection.y);

		//	invalidate score if intersection was bad
		Score = HoughLineA[6] * HoughLineB[6];
		Score *= Intersection3.z;
		
		//	invalidate score if cross is very far away
		float FarCoord = 10000;
		if ( fabsf(Intersection.x) > FarCoord || fabsf(Intersection.y) > FarCoord )
			Score = -1;
	}
	
	HoughCorners[CornerIndex] = (float4)( Intersection, Score, w );
}




__kernel void DrawHoughCorners(int OffsetIndex,__write_only image2d_t Frag,global float4* HoughCorners)
{
	int LineIndex = get_global_id(0) + OffsetIndex;
	float4 HoughCorner = HoughCorners[LineIndex];
	
	float2 Corner = HoughCorner.xy;
	float Score = HoughCorner.z;
	
	float4 Rgba = 1;
	Rgba.xyz = NormalToRgb( Score );
	
	int2 wh = get_image_dim(Frag);
	
	for ( int y=-2;	y<=2;	y++ )
	{
		for ( int x=-2;	x<=2;	x++ )
		{
			int2 xy = (int2)( Corner.x+x, Corner.y+y );
			xy.x = clamp( xy.x, 0, wh.x );
			xy.y = clamp( xy.y, 0, wh.y );
			write_imagef( Frag, xy, Rgba );
		}
	}
}


