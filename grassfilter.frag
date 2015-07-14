in vec2 fTexCoord;
uniform sampler2D hsl;
float4 MatchColour;
float MaxColourDiff;
float MinColourDiff;
float SaturationFloor;
float LuminanceFloor;
float LuminanceCeiling;
float HueWeight;
float SatWeight;
float LumWeight;

//	gr: change this to HSL and check diff of each component
float GetRgbDiff(float3 a,float3 b)
{
	float3 Diff4 = abs( a - b );
	//	average diff?
	float Diff = (Diff4.x + Diff4.y + Diff4.z + Diff4) / 3.0f;
	return Diff;
}

float GetHslDiff(float3 a,float3 b)
{
	//	if saturation is low, hue doesn't matter
	if ( a.s < SaturationFloor )
		HueWeight = 0.0f;

	if ( a.z < LuminanceFloor )
	{
		HueWeight = 0;
		SatWeight = 0;
	}
	if ( a.z > LuminanceCeiling )
	{
		HueWeight = 0;
		SatWeight = 0;
	}

	//	balance weights
	float TotalWeight = HueWeight + SatWeight + LumWeight;
	HueWeight /= TotalWeight;
	SatWeight /= TotalWeight;
	LumWeight /= TotalWeight;

	float3 DiffHsl = abs( a - b );
	DiffHsl.x *= HueWeight;
	DiffHsl.y *= SatWeight;
	DiffHsl.z *= LumWeight;

	float Diff = DiffHsl.x + DiffHsl.y + DiffHsl.z;
	return Diff;
}


void main()
{
	float4 Sample = texture2D(hsl,fTexCoord);
	float3 MatchHsl = RgbToHsl( MatchColour.xyz );
	float Diff = GetHslDiff( Sample.xyz, MatchColour.xyz );

	if ( Diff < MinColourDiff )
		Sample.w = 0;
	if ( Diff > MaxColourDiff )
		Sample.w = 0;

	//Sample.xyz = 1.f - (Diff / MaxColourDiff);
	gl_FragColor = Sample;
}
