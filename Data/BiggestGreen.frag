in vec2 fTexCoord;
uniform sampler2D cylinderfiltered;
uniform float2 cylinderfiltered_TexelWidthHeight;

uniform int SampleRadius = 8;	//	range (0,30)

float4 GetSample(float2 PixelOffset)
{
	//	work out uv
	float2 uv = fTexCoord + PixelOffset * cylinderfiltered_TexelWidthHeight.xy;
	return texture2D( cylinderfiltered, uv );
}

bool IsGoodSample(float4 Sample)
{
	return Sample.w > 0.5f;
}

float GetValue(float4 Sample)
{
	return Sample.g;
}

void main()
{
	float4 BaseSample = GetSample( float2(0,0) );
	if ( !IsGoodSample( BaseSample ) )
	{
		gl_FragColor = BaseSample*float4(1,1,1,0);
		return;
	}
	float BaseValue = GetValue( BaseSample );
	
	//	find a bigger neighbour radius, if we do, bailout with invalid
	int BiggerNeighbourRadius = 0;
	float BigValue = GetValue(BaseSample);
	for ( int y=-SampleRadius;	y<=SampleRadius;	y++ )
	{
		for ( int x=-SampleRadius;	x<=SampleRadius;	x++ )
		{
			//	ignore self
			if ( x==0 && y==0 )
				continue;
			float4 MatchSample = GetSample( float2(x,y) );

			if ( IsGoodSample(MatchSample) )
			{
				float MatchValue = GetValue( MatchSample );
				BigValue = max( BigValue, MatchValue );
				BiggerNeighbourRadius += (MatchValue < BaseValue) ? 1 : 0;
			}
		}
	}

	//	we're best if we keep alpha	
	float WeAreBiggestf = (BiggerNeighbourRadius == 0) ? 1 : 0;
	BaseSample.w = WeAreBiggestf;
	gl_FragColor = BaseSample;
}
