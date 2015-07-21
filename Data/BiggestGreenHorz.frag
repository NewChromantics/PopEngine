in vec2 fTexCoord;
uniform sampler2D cylindervertfilter;
uniform float2 cylindervertfilter_TexelWidthHeight;
uniform float2 cylindervertfilter_PixelWidthHeight;

uniform int SampleRadiusHorz = 60;

float4 GetSample(float2 PixelOffset)
{
	float2 uv = fTexCoord + (PixelOffset * cylindervertfilter_TexelWidthHeight);
	return texture2D( cylindervertfilter, uv );
}

int GetSampleCount(float2 PixelDelta)
{
	float2 PixelOffset = float2(0,0);
	for ( int i=0;	i<SampleRadiusHorz;	i++ )
	{
		PixelOffset += PixelDelta;
		float4 OffsetSample = GetSample( PixelOffset );
		if ( OffsetSample.w < 0.5f )
			return i;
	}
	return 999;
}

void main()
{
	//	read samples across and only keep if we're the middle one
	float4 ThisSample = GetSample( float2(0,0) );
	int LeftCount = GetSampleCount( float2(-1,0) );
	int RightCount = GetSampleCount( float2(1,0) );
	
	if ( abs(LeftCount - RightCount) <= 1 )
	{
		ThisSample.xyz = float3(1,1,0);
		bool OmitDoubles = true;
		if ( OmitDoubles && RightCount == 1 && LeftCount == 0 )
			ThisSample.w = 0;
	}
	else if ( LeftCount == 999 && RightCount == 999 )
	{
		ThisSample.xyz = float3(1,0,0);
	}
	else
	{
		ThisSample.xyz = float3(1,0,1);
		ThisSample.w = 0;
	}
	
	//	filter verticals
	if ( ThisSample.w > 0 )
	{
		int UpCount = GetSampleCount( float2(0,-1) );
		int DownCount = GetSampleCount( float2(0,-1) );
		if ( DownCount > 0 )
			ThisSample.w = 0;
	}
	
	//if ( ThisSample.w < 1 )
	//	ThisSample = float4(1,1,1,1);
	
	gl_FragColor = ThisSample;
//	gl_FragColor = float4(1,1,1,1);
}
