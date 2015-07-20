in vec2 fTexCoord;
uniform sampler2D grassfilter;
uniform vec2 grassfilter_TexelWidthHeight;
uniform vec2 grassfilter_PixelWidthHeight;

uniform int SampleRadius = 4;	//	range 0,9
uniform int HitCountMin = 4;
uniform bool IncludeSelf = true;


float4 GetSample(float2 PixelOffset)
{
	float2 uv = fTexCoord + (PixelOffset * grassfilter_TexelWidthHeight);
	return texture2D( grassfilter, uv );
}

void main()
{
	float4 ThisSample = GetSample( float2(0,0) );
	int HitCount = 0;
	
	for ( int y=-SampleRadius;	y<=SampleRadius;	y++ )
	{
		for ( int x=-SampleRadius;	x<=SampleRadius;	x++ )
		{
			if ( !IncludeSelf && y==0 && x==0 )
				continue;

			bool NeighbourHit = (GetSample( float2(x,y) ).w > 0.5f);
			HitCount += NeighbourHit ? 1 : 0;
		}
	}

	ThisSample.w = (HitCount > HitCountMin) ? 1 : 0;
	gl_FragColor = ThisSample;
}
