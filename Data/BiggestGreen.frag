in vec2 fTexCoord;
uniform sampler2D cylinderfiltered;
uniform float2 cylinderfiltered_TexelWidthHeight;

uniform int SampleRadiusHorz = 2;	//	range 0,9
uniform int SampleRadiusVert = 8;	//	range 0,9
uniform bool IncludeSelf = false;

//#define MATCH_FIRST_X
//#define MATCH_MIDDLE_X	//	doesn't work as I'd hoped

float4 GetSample(float2 PixelOffset)
{
	float2 uv = fTexCoord + (PixelOffset * cylinderfiltered_TexelWidthHeight);
	return texture2D( cylinderfiltered, uv );
}

float GetCompareValue(float4 Sample)
{
	return Sample.w < 0.5f ? -1 : Sample.y;
}

void main()
{
	float4 ThisSample = GetSample( float2(0,0) );
	int HitCount = 0;
	
	int BiggerNeighbourCount = 0;
	float ThisValue = GetCompareValue( ThisSample );
	
	//for ( int y=-SampleRadiusVert;	y<=SampleRadiusVert;	y++ )
	for ( int y=1;	y<=SampleRadiusVert;	y++ )
	//for ( int y=-SampleRadiusVert;	y<=0;	y++ )
	{
	#if defined(MATCH_MIDDLE_X)
		//	gather the sum of all the x-coords with the same value
		//	this will tell us where we are horizontally with the match
		//	assuming the radius is large enough, neighbours should have corresponding values
		//	then we can just pick the one in the middle (sum=0)
		int BiggerOffsetsOnRow = 0;
		bool HasNeighbourOnRow = false;
	#endif
		
		for ( int x=-SampleRadiusHorz;	x<=SampleRadiusHorz;	x++ )
		{
			if ( !IncludeSelf && y==0 && x==0 )
				continue;
			
			float4 NeighbourSample = GetSample( float2(x,y) );
			if ( NeighbourSample.w < 1 )
				continue;
			
			float NeighbourValue = GetCompareValue(NeighbourSample);
			
	#if defined MATCH_FIRST_X
			//	work out if there's one more-central than 0
			if ( NeighbourValue == ThisValue && x < 0 )
			{
				BiggerNeighbourCount++;
			}
	#endif
			
	#if defined(MATCH_MIDDLE_X)
			//	work out if there's one more-central than 0
			if ( NeighbourValue == ThisValue )
			{
				BiggerOffsetsOnRow += x;
				HasNeighbourOnRow = true;
			}
	#endif
			
			if ( NeighbourValue > ThisValue )
				BiggerNeighbourCount++;
		}
		
	#if defined(MATCH_MIDDLE_X)
		//	is there a more central neighbour? if so count it
		if ( HasNeighbourOnRow )
		{
			int range = 4;
			if ( BiggerOffsetsOnRow>range || BiggerOffsetsOnRow <-range )
				BiggerNeighbourCount++;
		}
	#endif
	}
	
	ThisSample.w = (BiggerNeighbourCount == 0) ? ThisSample.w : 0;
	ThisSample.xyz = float3(0,0,0);
	gl_FragColor = ThisSample;
}
