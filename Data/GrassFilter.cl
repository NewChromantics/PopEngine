#define const	__constant
#define vec3	float3
#define vec4	float4
//#define override	__attribute__((overloadable))	//	gr: doesn't seem to actually work

const float MaxColourDiff = 1;
const float MinColourDiff = 0.263f;
const float SaturationFloor = 0.17f;
const float LuminanceFloor = 0.156f;
const float LuminanceCeiling = 0.75f;
const float gHueWeight = 1;
const float gSatWeight = 0;
const float gLumWeight = 0.85f;
const float4 MatchColour = (float4)( 100/255.f, 140/255.f, 72/255.f, 0 );


static float max3(float a,float b,float c)
{
	return max( a, max( b,c ) );
}

static float min3(float a,float b,float c)
{
	return min( a, min( b,c ) );
}


static float3 RgbToHsl(vec3 rgb)
{
	float r = rgb.x;
	float g = rgb.y;
	float b = rgb.z;

	float Max = max3( r, g, b );
	float Min = min3( r, g, b );

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
static float GetRgbDiff(vec3 a,vec3 b)
{
	vec3 Diff4 = fabs( a - b );
	//	average diff?
	float Diff = (Diff4.x + Diff4.y + Diff4.z) / 3.0f;
	return Diff;
}

static float GetHslDiff(vec3 a,vec3 b)
{
	float HueWeight = gHueWeight;
	float SatWeight = gSatWeight;
	float LumWeight = gLumWeight;

	//	if saturation is low, hue doesn't matter
	if ( a.y < SaturationFloor )
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

	vec3 DiffHsl = fabs( a - b );
	DiffHsl.x *= HueWeight;
	DiffHsl.y *= SatWeight;
	DiffHsl.z *= LumWeight;

	float Diff = DiffHsl.x + DiffHsl.y + DiffHsl.z;
	return Diff;
}

static float4 texture2D(__read_only image2d_t Image,int2 uv)
{
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

	return read_imagef( Image, Sampler, uv );
}

__kernel void GrassFilter(int OffsetX,int OffsetY,__read_only image2d_t hsl,__write_only image2d_t Frag)
{
	float x = get_global_id(0) + OffsetX;
	float y = get_global_id(1) + OffsetY;
	int2 uv = (int2)( x, y );

	vec4 Sample = texture2D( hsl, uv );
	float Origw = Sample.w;
	
	vec3 MatchHsl = RgbToHsl( MatchColour.xyz );
	float Diff = GetHslDiff( Sample.xyz, MatchHsl.xyz );

	if ( Diff < MinColourDiff )
	{
		Sample.w = 0;
		Sample = (float4)(0,0,0,1);
	}
	else if ( Diff > MaxColourDiff )
	{
		Sample.w = 0;
		Sample = (float4)(0,0,0,1);
	}
	else if ( Origw == 0 )
	{
		Sample = (float4)(0,0,0,0);
	}
	else
	{
		Sample = (float4)(1,1,1,1);
	}
	
	write_imagef( Frag, uv, Sample );
}


