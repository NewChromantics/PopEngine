in vec2 fTexCoord;
uniform sampler2D grassfilter;

float SampleRadius = 4;	//	range 0,9




sampler2D _MainTex;	//	new lum
in vec2 grassfilter_TexelWidthHeight;
in vec2 grassfilter_PixelWidthHeight;
float DiffTolerance;
float DepthComponentInf;
bool Debug_HighlightFill;

const bool IncludeSelf = true;

//	ios can't cope without fixed loops
#if defined(TARGET_IOS)
const int SampleRadius = 1;
const int HitCountMin = 4;
#else
int SampleRadius;
int HitCountMin;
#endif

//	returns good depth (if inf, false)
bool RgbaToDepth(out float Depth,float4 Rgba)
{
	Depth = Rgba.x;
	return Rgba.w > 0.5f;
}

bool GetDepth(out float Depth,FragInput In,int2 PixelOffset)
{
	//	work out uv
	float2 uv = In.uv_MainTex + PixelOffset * _MainTex_TexelSize.xy;
	float4 rgba = tex2D( _MainTex, uv );
	return RgbaToDepth( Depth, rgba );
}

FragInput vert(VertexInput In) {
	FragInput Out;
	Out.Position = mul (UNITY_MATRIX_MVP, In.Position );
	Out.uv_MainTex = In.uv_MainTex;
	return Out;
}



void main()
{
	float BaseDepth = 0.0f;
	bool BaseGoodDepth = GetDepth( BaseDepth, In, int2(0,0) );
	if ( BaseGoodDepth )
		return float4( BaseDepth, 0, 0, 1 );
	
	float MinNeighbourDepth = 9999;
	//	match depth around me, if I'm wildly out, discard.
	//	if I'm not good, just pick a good one
#ifdef SHADER_API_D3D11
	[unroll(12)]
#endif
	for ( int y=-SampleRadius;	y<=SampleRadius;	y++ )
	{
#ifdef SHADER_API_D3D11
		[unroll(12)]
#endif
		for ( int x=-SampleRadius;	x<=SampleRadius;	x++ )
		{
			float MatchDepth = 0.0f;
			bool MatchGoodDepth = GetDepth( MatchDepth, In, int2(x,y) );
			
			//	ignore not-good neighbours
			MinNeighbourDepth = min(MinNeighbourDepth, MatchGoodDepth ? MatchDepth : MinNeighbourDepth);
		}
	}
	
	//	no data
	if ( MinNeighbourDepth > 1 )
		return float4(0,0,0,0);
	
	return float4(MinNeighbourDepth,0,Debug_HighlightFill?MinNeighbourDepth:0,1);
}


void main()
{
	vec4 Sample = texture2D(grassfilter,fTexCoord);
	vec3 MatchHsl = RgbToHsl( MatchColour.xyz );
	float Diff = GetHslDiff( Sample.xyz, MatchColour.xyz );

	if ( Diff < MinColourDiff )
		Sample.w = 0;
	if ( Diff > MaxColourDiff )
		Sample.w = 0;

	//Sample.xyz = 1.f - (Diff / MaxColourDiff);
	gl_FragColor = Sample;

}
