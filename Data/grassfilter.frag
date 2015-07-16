in vec2 fTexCoord;
uniform sampler2D hsl;
vec4 MatchColour = vec4( 100/255, 140/255, 72/255, 0 );
float MaxColourDiff = 1;
float MinColourDiff = 0.263f;
float SaturationFloor = 0.17f;
float LuminanceFloor = 0.156f;
float LuminanceCeiling = 0.75f;
float HueWeight = 1;
float SatWeight = 0;
float LumWeight = 0.85f;



float max(float a,float b,float c)
{
	return max( a, max( b,c ) );
}

float min(float a,float b,float c)
{
	return min( a, min( b,c ) );
}


vec3 RgbToHsl(vec3 rgb)
{
	float r = rgb.x;
	float g = rgb.y;
	float b = rgb.z;

	float Max = max( r, g, b );
	float Min = min( r, g, b );

	float h = 0;
	float s = 0;
	float l = ( Max + Min ) / 2.f;

	if ( Max == Min )
	{
		//	achromatic/grey
        h = s = 0;
    }
	else
	{
        float d = Max - Min;
        s = l > 0.5f ? d / (2 - Max - Min) : d / (Max + Min);
        if ( Max == r )
		{
            h = (g - b) / d + (g < b ? 6 : 0);
		}
		else if ( Max == g )
		{
            h = (b - r) / d + 2;
        }
		else //if ( Max == b )
		{
			h = (r - g) / d + 4;
		}

        h /= 6;
    }

	return vec3( h, s, l );
}


//	gr: change this to HSL and check diff of each component
float GetRgbDiff(vec3 a,vec3 b)
{
	vec3 Diff4 = abs( a - b );
	//	average diff?
	float Diff = (Diff4.x + Diff4.y + Diff4.z) / 3.0f;
	return Diff;
}

float GetHslDiff(vec3 a,vec3 b)
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

	vec3 DiffHsl = abs( a - b );
	DiffHsl.x *= HueWeight;
	DiffHsl.y *= SatWeight;
	DiffHsl.z *= LumWeight;

	float Diff = DiffHsl.x + DiffHsl.y + DiffHsl.z;
	return Diff;
}


void main()
{
	vec4 Sample = texture2D(hsl,fTexCoord);
	vec3 MatchHsl = RgbToHsl( MatchColour.xyz );
	float Diff = GetHslDiff( Sample.xyz, MatchColour.xyz );

	if ( Diff < MinColourDiff )
		Sample.w = 0;
	if ( Diff > MaxColourDiff )
		Sample.w = 0;

	//Sample.xyz = 1.f - (Diff / MaxColourDiff);
	gl_FragColor = Sample;

}
