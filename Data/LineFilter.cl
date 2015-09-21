#define const	__constant

//const int SampleRadius = 8;	//	range 0,9
#define SampleRadius	5	//	range 0,9
const int HitCountMin = 2;
const bool IncludeSelf = true;


const float MinLum = 0.5f;
const float Tolerance = 0.01f;
const float AngleRange = 360.0f;


static float Range(float Time,float Start,float End)
{
	return (Time-Start) / (End-Start);
}


static float Lerp(float Time,float Start,float End)
{
	return Start + (Time * (End-Start));
}

static float3 Lerp3(float Time,float3 Start,float3 End)
{
	return (float3)( Lerp(Time,Start.x,End.x), Lerp(Time,Start.y,End.y), Lerp(Time,Start.z,End.z) );
}


static float4 texture2D(__read_only image2d_t Image,int2 uv)
{
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	
	return read_imagef( Image, Sampler, uv );
}

float DegToRad(float Degrees)
{
#define PIf 3.14159265359f
	return Degrees * (PIf / 180.f);
}

float2 GetVectorAngle(float AngleDeg,float Radius)
{
	float UvOffsetx = cosf( DegToRad( AngleDeg ) ) * Radius;
	float UvOffsety = sinf( DegToRad( AngleDeg ) ) * Radius;
	return (float2)(UvOffsetx,UvOffsety);
}

bool GetRingMatch(float3 BaseHsl,int2 BaseUv,float Radius,float AngleDeg,__read_only image2d_t Hsl)
{
	float2 UvOffset = GetVectorAngle( AngleDeg, Radius );
	float2 Uv = (float2)(BaseUv.x + UvOffset.x, BaseUv.y + UvOffset.y);

	float4 MatchHsl = texture2D( Hsl, (int2)(Uv.x,Uv.y) );
	
	if ( MatchHsl.z+Tolerance < BaseHsl.z )
		return false;

	return true;
}


float GetHslHslDifference(float3 a,float3 b)
{
	float ha = a.x;
	float hb = b.x;
	float sa = a.y;
	float sb = b.y;
	float la = a.z;
	float lb = b.z;
	
	float sdiff = fabs( sa - sb );
	float ldiff = fabs( la - lb );
	
	//	hue wraps, so difference needs to be calculated differently
	//	convert -1..1 to -0.5...0.5
	float hdiff = ha - hb;
	hdiff = ( hdiff > 0.5f ) ? hdiff - 1.f : hdiff;
	hdiff = ( hdiff < -0.5f ) ? hdiff + 1.f : hdiff;
	hdiff = fabs( hdiff );
	
	//	the higher the weight, the MORE difference it detects
	float hweight = 1.f;
	float sweight = 1.f;
	float lweight = 2.f;
#define NEAR_WHITE	0.8f
#define NEAR_BLACK	0.3f
#define NEAR_GREY	0.3f
	
	//	if a or b is too light, tone down the influence of hue and saturation
	{
		float l = max(la,lb);
		float Change = ( max(la,lb) > NEAR_WHITE ) ? ((l - NEAR_WHITE) / ( 1.f - NEAR_WHITE )) : 0.f;
		hweight *= 1.f - Change;
		sweight *= 1.f - Change;
	}
	//	else
	{
		float l = min(la,lb);
		float Change = ( min(la,lb) < NEAR_BLACK ) ? l / NEAR_BLACK : 1.f;
		hweight *= Change;
		sweight *= Change;
	}
	
	//	if a or b is undersaturated, we reduce weight of hue
	
	{
		float s = min(sa,sb);
		hweight *= ( min(sa,sb) < NEAR_GREY ) ? s / NEAR_GREY : 1.f;
	}
	
	
	//	normalise weights to 1.f
	float Weight = hweight + sweight + lweight;
	hweight /= Weight;
	sweight /= Weight;
	lweight /= Weight;
	
	float Diff = 0.f;
	Diff += hdiff * hweight;
	Diff += sdiff * sweight;
	Diff += ldiff * lweight;
	
	//	nonsense HSL values result in nonsense diff, so limit output
	Diff = min( Diff, 1.f );
	Diff = max( Diff, 0.f );
	return Diff;
}



void GetSample(float* SampleScores,int2 PatternSize,float2 SampleSize,__read_only image2d_t Hsl,float3 BaseHsl,float HslDiffMax,float2 BaseUv)
{
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	float2 Middle = (float2)(PatternSize.x,PatternSize.y) / (float2)(2,2);
	for ( int x=0;	x<PatternSize.x;	x++ )
	{
		for ( int y=0;	y<PatternSize.y;	y++ )
		{
			float2 uv = (float2)(x,y);
			uv -= Middle;
			uv *= SampleSize;
			uv += BaseUv;

			float3 MatchHsl = read_imagef( Hsl, Sampler, uv ).xyz;
			float Diff = GetHslHslDifference( BaseHsl, MatchHsl );
			float Score = (Diff < HslDiffMax) ? 1 : 0;
			SampleScores[x+(y*PatternSize.x)] = Score;
		}
	}
	
}



__kernel void LineFilter(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__read_only image2d_t Frame,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	int2 wh = get_image_dim(Hsl);
	
	//	this is the image we're trying to match
#define PatternWidth	6
#define PatternHeight	5
	float PatternScores[PatternWidth*PatternHeight] =
	{
		0,0,1,1,0,0,
		0,0,1,1,0,0,
		0,0,1,1,0,0,
		0,0,1,1,0,0,
		0,0,1,1,0,0,
	};
	
	
	//	create sample to compare against the pattern
	float3 BaseHsl = texture2D( Hsl, uv ).xyz;
	float HslDiffMax = 0.05f;
	float SampleWidthPx = PatternWidth * 4.0f;
	float SampleHeightPx = PatternHeight * 1.0f;
	float SampleScores[PatternWidth*PatternHeight];
	for ( int p=0;	p<PatternWidth*PatternHeight;	p++ )
		SampleScores[p] = 0;
	GetSample( SampleScores, (int2)(PatternWidth, PatternHeight), (float2)(SampleWidthPx,SampleHeightPx), Hsl, BaseHsl, HslDiffMax, (float2)(uv.x,uv.y) );
	
	//	compare scores
	float Score = 0;
	for ( int p=0;	p<PatternWidth*PatternHeight;	p++ )
	{
		Score += PatternScores[p] == SampleScores[p];
	}
	Score /= (float)(PatternWidth*PatternHeight);

	float4 Rgba = (float4)(Score,Score,Score,1);

	//	score red-lime
	float RedScore = 0.6f;
	float YellowScore = 0.8f;
	if ( Score >= YellowScore )
	{
		Rgba.xyz = Lerp3( Range( Score, YellowScore, 1.0f ), (float3)(1,1,0), (float3)(0,1,0) );
	}
	else if ( Score >= RedScore )
	{
		Rgba.xyz = Lerp3( Range( Score, RedScore, YellowScore ), (float3)(1,0,0), (float3)(1,1,0) );
	}

	
	write_imagef( Frag, uv, Rgba );
}
