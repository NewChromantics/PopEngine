in vec2 fTexCoord;
uniform sampler2D cylinderfiltered;
uniform float2 cylinderfiltered_TexelWidthHeight;

uniform int SampleRadius = 8;	//	range 0,9
uniform bool IncludeSelf = true;


float4 GetSample(float2 PixelOffset)
{
	float2 uv = fTexCoord + (PixelOffset * cylinderfiltered_TexelWidthHeight);
	return texture2D( cylinderfiltered, uv );
}

float GetCompareValue(float4 Sample)
{
	return Sample.w < 1 ? -1 : Sample.y;
}

void main()
{
	float4 ThisSample = GetSample( float2(0,0) );
	int HitCount = 0;
	
	int BiggerNeighbourCount = 0;
	float ThisValue = GetCompareValue( ThisSample );
	
	for ( int y=-SampleRadius;	y<=SampleRadius;	y++ )
	{
		//	gather the sum of all the coords with the same value
		int BiggerOffsetsOnRow = 0;
		
		for ( int x=-SampleRadius;	x<=SampleRadius;	x++ )
		{
			if ( !IncludeSelf && y==0 && x==0 )
				continue;
			
			float4 NeighbourSample = GetSample( float2(x,y) );
			float NeighbourValue = GetCompareValue(NeighbourSample);
			
			//	work out if there's one more-central than 0
			if ( NeighbourValue == ThisValue && x < 0 )
			{
				BiggerNeighbourCount++;
				BiggerOffsetsOnRow += x;
			}
			
			if ( NeighbourValue > ThisValue )
				BiggerNeighbourCount++;
		}
		
		//	the
	}
	
	ThisSample.w = (BiggerNeighbourCount == 0) ? ThisSample.w : 0;
	ThisSample.xyz = float3(1,1,1);
	gl_FragColor = ThisSample;
}
