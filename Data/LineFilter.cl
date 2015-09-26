#define const	__constant

//const int SampleRadius = 8;	//	range 0,9
#define SampleRadius	5	//	range 0,9
const int HitCountMin = 2;
const bool IncludeSelf = true;

const float MinLum = 0.5f;
const float Tolerance = 0.01f;
const float AngleRange = 360.0f;


//	gr: this score needs to vary for the MAX pixels that could even be on a line (a line clipped in a corner will have less pixels, but could be perfect)
//#define EDGE_FILTER_WHITE_PIXELS
#if defined(EDGE_FILTER_WHITE_PIXELS)
#define MIN_HOUGH_SCORE	800
#define MAX_HOUGH_SCORE	1000
#else
#define MIN_HOUGH_SCORE	300
#define MAX_HOUGH_SCORE	1500
#endif


//	filter out lines if a neighbour is better
#define CHECK_MAXIMA_ANGLES		20
#define CHECK_MAXIMA_DISTANCES	20


static float Range(float Time,float Start,float End)
{
	return (Time-Start) / (End-Start);
}


static float Lerp(float Time,float Start,float End)
{
	return Start + (Time * (End-Start));
}

static float2 Lerp2(float Time,float2 Start,float2 End)
{
	return (float2)( Lerp(Time,Start.x,End.x), Lerp(Time,Start.y,End.y) );
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

static float DegToRad(float Degrees)
{
//	if ( Degrees < 0 )		Degrees += 360;
//	if ( Degrees > 360 )	Degrees -= 360;
#define PIf 3.14159265359f
	return Degrees * (PIf / 180.f);
}

static float2 GetVectorAngle(float AngleDeg,float Radius)
{
	float UvOffsetx = cosf( DegToRad( AngleDeg ) ) * Radius;
	float UvOffsety = sinf( DegToRad( AngleDeg ) ) * Radius;
	return (float2)(UvOffsetx,UvOffsety);
}

static bool GetRingMatch(float3 BaseHsl,int2 BaseUv,float Radius,float AngleDeg,__read_only image2d_t Hsl)
{
	float2 UvOffset = GetVectorAngle( AngleDeg, Radius );
	float2 Uv = (float2)(BaseUv.x + UvOffset.x, BaseUv.y + UvOffset.y);

	float4 MatchHsl = texture2D( Hsl, (int2)(Uv.x,Uv.y) );
	
	if ( MatchHsl.z+Tolerance < BaseHsl.z )
		return false;

	return true;
}



static float hue2rgb(float p,float q,float t)
{
	if(t < 0) t += 1.f;
	if(t > 1) t -= 1.f;
	if(t < 1.f/6.f) return p + (q - p) * 6.f * t;
	if(t < 1.f/2.f) return q;
	if(t < 2.f/3.f) return p + (q - p) * (2.f/3.f - t) * 6.f;
	return p;
}

static float3 HslToRgb(float3 Hsl)
{
	float h = Hsl.x;
	float s = Hsl.y;
	float l = Hsl.z;
	
	if(s == 0){
		return (float3)(l,l,l);
	}else{
		float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
		float p = 2.f * l - q;
		float r = hue2rgb(p, q, h + 1.f/3.f);
		float g = hue2rgb(p, q, h);
		float b = hue2rgb(p, q, h - 1.f/3.f);
		return (float3)(r,g,b);
	}
}



static float3 NormalToRgb(float Normal)
{
	//return (float3)(Normal,Normal,Normal);
	//	red to green
	//float Hue = Lerp( Normal, -0.3f, 0.4f );	//	blue to green
	float Hue = Lerp( Normal, 0, 0.4f );	//	red to green
	float3 Hsl = (float3)( Hue, 1.0f, 0.6f );
	
	return HslToRgb( Hsl );
}

static float4 NormalToRgba(float Normal)
{
	float4 Rgba = 1;
	Rgba.xyz = NormalToRgb(Normal);
	return Rgba;
}

static int RgbToIndex(float3 Rgb,int IndexCount)
{
	float Index = Rgb.x * IndexCount;
	return Index;
}

static float3 IndexToRgb(int Index,int IndexCount)
{
	if ( Index == 0 )
		return (float3)(0,0,0);
	
	float Norm = Index / (float)IndexCount;
	//return NormalToRgb( Index / (float)IndexCount );
	return (float3)( Norm, Norm, Norm );
}
static float4 IndexToRgba(int Index,int IndexCount)
{
	float4 Rgba = 1;
	Rgba.xyz = IndexToRgb( Index, IndexCount );
	return Rgba;
}


static float GetHslHslDifference(float3 a,float3 b)
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



static void GetSample(float* SampleScores,int2 PatternSize,float2 SampleScale,float AngleDeg,__read_only image2d_t Hsl,float3 BaseHsl,float HslDiffMax,float2 BaseUv)
{
	
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	float2 Middle = (float2)(PatternSize.x,PatternSize.y) / (float2)(2,2);
	for ( int x=0;	x<PatternSize.x;	x++ )
	{
		for ( int y=0;	y<PatternSize.y;	y++ )
		{
			float2 Offset = (float2)(x,y);
			Offset -= Middle;
			Offset *= SampleScale;
			
			//	rotate sample
			float2 uv;
			uv.x = cosf( DegToRad(AngleDeg) ) * Offset.x;
			uv.y = sinf( DegToRad(AngleDeg) ) * Offset.y;
			
			uv += BaseUv;

			float3 MatchHsl = read_imagef( Hsl, Sampler, uv ).xyz;
			float Diff = GetHslHslDifference( BaseHsl, MatchHsl );
			float Score = (Diff < HslDiffMax) ? 1 : 0;
			SampleScores[x+(y*PatternSize.x)] = Score;
		}
	}
	
}



//	x is positive score, y is negative score
static float2 GetPatternScore(float2 SampleSizePx,float AngleDeg,__read_only image2d_t Hsl,float HslDiffMax,float2 BaseUv)
{
	//	this is the image we're trying to match
	/*
#define PatternWidth	6
#define PatternHeight	6
	float PatternScores[PatternWidth*PatternHeight] =
	{
		0,0,1,1,0,0,
		0,0,1,1,0,0,
		0,0,1,1,0,0,
		0,0,1,1,0,0,
		0,0,1,1,0,0,
		0,0,1,1,0,0,
	};
	*/
	#define PatternWidth	3
	#define PatternHeight	3
	float PatternScores[PatternWidth*PatternHeight] =
	{
		0,1,0,
		0,1,0,
		0,1,0,
	};
	int2 PatternSize = (int2)(PatternWidth,PatternHeight);
	
	//	create sample to compare against the pattern
	float3 BaseHsl = texture2D( Hsl, (int2)(BaseUv.x,BaseUv.y) ).xyz;
	float2 SampleScale = (float2)( SampleSizePx.x / PatternWidth, SampleSizePx.y / PatternHeight );
	float SampleScores[PatternWidth*PatternHeight];

	GetSample( SampleScores, PatternSize, SampleScale, AngleDeg, Hsl, BaseHsl, HslDiffMax, BaseUv );
	
	//	compare scores
	float PositiveScore = 0;
	int PositiveCount = 0;
	float NegativeScore = 0;
	int NegativeCount = 0;
	
	for ( int p=0;	p<PatternWidth*PatternHeight;	p++ )
	{
		if ( PatternScores[p] > 0.5f )
		{
			PositiveScore += PatternScores[p] == SampleScores[p];
			PositiveCount++;
		}
		else
		{
			NegativeScore += PatternScores[p] == SampleScores[p];
			NegativeCount++;
		}
	}
	PositiveScore /= (float)(max(1,PositiveCount));
	NegativeScore /= (float)(max(1,NegativeCount));
	
	return (float2)(PositiveScore,NegativeScore);
}

__kernel void LineFilter(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__read_only image2d_t Frame,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	int2 wh = get_image_dim(Hsl);
	
	float HslDiffMax = 0.11f;
#define AngleCount	7
	float Angles[AngleCount] =
	{
		100, 110, 90, 80, 70, 60, 50,
	};
#define ScaleCount	4
	float Scales[ScaleCount] =
	{
		1,2,3,4
	};

	//float2 SampleSizePx = (float2)(20,20);
	//float2 SampleSizePx = (float2)(20,20);
	//	minimum positive/negative scores
	float2 MinScore = (float2)(1.0f,0.5f);
	int BestAngleIndex = -1;
	int BestScaleIndex = -1;
	float2 BestScore = 0;
	for ( int s=0;	s<ScaleCount;	s++ )
	for ( int a=0;	a<AngleCount;	a++ )
	{
		float2 SampleSizePx = (float2)( Scales[s], Scales[s] );
		float2 Score = GetPatternScore( SampleSizePx, Angles[a], Hsl, HslDiffMax, (float2)(uv.x,uv.y) );
		if ( Score.x < MinScore.x || Score.y < MinScore.y )
			continue;
		if ( Score.x < BestScore.x && Score.y < BestScore.y )
			continue;

		//	weight the score
		float Scoref = Score.x + Score.y;
		float BestScoref = BestScore.x + BestScore.y;
		if ( Scoref < BestScoref )
			continue;
		
		BestScore = Score;
		BestAngleIndex = a;
		BestScaleIndex = s;
	}

	float Score = (BestScore.x + BestScore.y)/2.f;
	float MinScoref = (MinScore.x + MinScore.y)/2.f;
	
	//	colour by angle
	float4 Rgba = (float4)(Score,Score,Score,1);
	bool ColourByAngle = false;
	bool ColourByRainbowScore = false;
	if ( BestAngleIndex < 0 )
	{
		Rgba.xyz = 0;
	}
	else if ( ColourByAngle )
	{
		float AngleNorm = Range( Angles[BestAngleIndex], Angles[0], Angles[AngleCount-1] );
		if ( AngleNorm < 0.25f )
		{
			Rgba.xyz = Lerp3( Range( AngleNorm, 0, 0.25f ), (float3)(1,0,0), (float3)(1,1,0) );
		}
		else if ( AngleNorm < 0.50f )
		{
			Rgba.xyz = Lerp3( Range( AngleNorm, 0.25f, 0.50f ), (float3)(1,1,0), (float3)(0,1,0) );
		}
		else if ( AngleNorm < 0.75f )
		{
			Rgba.xyz = Lerp3( Range( AngleNorm, 0.50f, 0.75f ), (float3)(0,1,0), (float3)(0,1,1) );
		}
		else
		{
			Rgba.xyz = Lerp3( Range( AngleNorm, 0.75f, 1.0f ), (float3)(0,1,1), (float3)(0,0,1) );
		}
	}
	else if ( ColourByRainbowScore )
	{
		//	score red-lime
		float RedScore = MinScoref;
		float YellowScore = Lerp( 0.5f, MinScoref, 1.0f );
		if ( Score >= YellowScore )
		{
			Rgba.xyz = Lerp3( Range( Score, YellowScore, 1.0f ), (float3)(1,1,0), (float3)(0,1,0) );
		}
		else if ( Score >= RedScore )
		{
			Rgba.xyz = Lerp3( Range( Score, RedScore, YellowScore ), (float3)(1,0,0), (float3)(1,1,0) );
		}
	}
	
	write_imagef( Frag, uv, Rgba );

#undef AngleCount
#undef ScaleCount
}



bool RgbaToWhite(float4 Rgba)
{
//#define HistogramHslsCount	(15)
	int Index = RgbToIndex( Rgba.xyz, 15 );
	return Index == 0;
}

bool RgbaToGreen(float4 Rgba)
{
	//#define HistogramHslsCount	(15)
	int Index = RgbToIndex( Rgba.xyz, 15 );
	return Index==9;
}


void GetWhiteSample(float* SampleScores,int2 PatternSize,float2 SampleScale,float AngleDeg,__read_only image2d_t WhiteFilter,float2 BaseUv)
{
	float AngleRad = DegToRad(AngleDeg);
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	float2 Middle = (float2)(PatternSize.x,PatternSize.y) / (float2)(2,2);
	for ( int x=0;	x<PatternSize.x;	x++ )
	{
		for ( int y=0;	y<PatternSize.y;	y++ )
		{
			float2 Offset = (float2)(x,y);
			Offset -= Middle;
			Offset *= SampleScale;

			float2 uv;
			uv.x = Offset.x * cosf(AngleRad) - Offset.y * sinf(AngleRad);
			uv.y = Offset.x * sinf(AngleRad) + Offset.y * cosf(AngleRad);
			
			//	rotate sample
			uv += BaseUv;
			
			bool IsWhite = RgbaToWhite( read_imagef( WhiteFilter, Sampler, uv ) );
			float Score = IsWhite;
			SampleScores[x+(y*PatternSize.x)] = Score;
		}
	}
	
}


//	x is positive score, y is negative score
float2 GetWhitePatternScore(float2 SampleSizePx,float AngleDeg,__read_only image2d_t WhiteFilter,float2 BaseUv)
{
	//	this is the image we're trying to match
	
	 #define PatternWidth	5
	 #define PatternHeight	6
	 float PatternScores[PatternWidth*PatternHeight] =
	 {
		0,0,1,0,0,
		0,0,1,0,0,
		0,0,1,0,0,
		0,0,1,0,0,
		0,0,1,0,0,
		0,0,1,0,0,
	 };
	/*
#define PatternWidth	3
#define PatternHeight	3
	float PatternScores[PatternWidth*PatternHeight] =
	{
		0,1,0,
		0,1,0,
		0,1,0,
	};
	  */
	int2 PatternSize = (int2)(PatternWidth,PatternHeight);
	
	//	create sample to compare against the pattern
	float2 SampleScale = (float2)( SampleSizePx.x / PatternWidth, SampleSizePx.y / PatternHeight );
	float SampleScores[PatternWidth*PatternHeight];
	
	GetWhiteSample( SampleScores, PatternSize, SampleScale, AngleDeg, WhiteFilter, BaseUv );
	
	//	compare scores
	float PositiveScore = 0;
	int PositiveCount = 0;
	float NegativeScore = 0;
	int NegativeCount = 0;
	
	for ( int p=0;	p<PatternWidth*PatternHeight;	p++ )
	{
		if ( PatternScores[p] > 0.5f )
		{
			PositiveScore += (SampleScores[p]>0.5f) ? 1 : 0;
			PositiveCount++;
		}
		else
		{
			NegativeScore += PatternScores[p] == SampleScores[p];
			NegativeCount++;
		}
	}
	PositiveScore /= (float)(max(1,PositiveCount));
	NegativeScore /= (float)(max(1,NegativeCount));
	
	return (float2)(PositiveScore,NegativeScore);
}

__kernel void WhiteLineFilter(int OffsetX,int OffsetY,__read_only image2d_t WhiteFilter,__read_only image2d_t Frame,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );

	//	abort early
	//float4 InvalidColour = texture2D( Frame, uv );
	float4 InvalidColour = (float4)(0,0,0,1);
	bool BaseWhite = RgbaToWhite( texture2D( WhiteFilter, uv ) );
	if ( !BaseWhite )
	{
		write_imagef( Frag, uv, InvalidColour );
		return;
	}
	int PixelSkip = 0;
	if ( PixelSkip != 0 && ( uv.x % PixelSkip != 0 || uv.y % PixelSkip != 0 ) )
	{
		write_imagef( Frag, uv, InvalidColour );
		return;
	}
	
#define AngleCount	10
	float Angles[AngleCount] =
	{
		//-85, 5, 10, 15, 20, 25, 30,
		-90, -70, -50, -30, -15, 0, 15, 30, 50, 70
	};
#define WindowSizeCount 3
	float WindowSize[WindowSizeCount] =
	{
		6, 15, 30
	};
	
	//float2 SampleSizePx = (float2)(20,20);
	//float2 SampleSizePx = (float2)(20,20);
	//	minimum positive/negative scores
	//	float2 MinScore = (float2)(0.7f,0.7f);
	float2 MinScore = (float2)(0.8f,0.8f);
	int BestAngleIndex = -1;
	int BestScaleIndex = -1;
	float2 BestScore = 0;
	for ( int s=0;	s<WindowSizeCount;	s++ )
	{
		for ( int a=0;	a<AngleCount;	a++ )
		{
			float2 SampleSizePx = (float2)( WindowSize[s], WindowSize[s] );
			float2 Score = GetWhitePatternScore( SampleSizePx, Angles[a], WhiteFilter, (float2)(uv.x,uv.y) );
			if ( Score.x < MinScore.x || Score.y < MinScore.y )
				continue;
			if ( Score.x < BestScore.x && Score.y < BestScore.y )
				continue;
			
			//	weight the score
			float Scoref = Score.x + Score.y;
			float BestScoref = BestScore.x + BestScore.y;
			if ( Scoref < BestScoref )
				continue;
			
			BestScore = Score;
			BestAngleIndex = a;
			BestScaleIndex = s;
		}
	}
	
	float Score = (BestScore.x + BestScore.y)/2.f;
	float MinScoref = (MinScore.x + MinScore.y)/2.f;

	float ScoreNorm = Score / (1.f-MinScoref);
	
	float4 Rgba = (float4)(ScoreNorm,ScoreNorm,ScoreNorm,1);
	bool ColourByAngle = true;
	bool ColourByRainbowScore = true;
	if ( BestAngleIndex < 0 )
	{
		Rgba.xyz = 0;
	}
	else if ( ColourByAngle )
	{
		float AngleNorm = Range( Angles[BestAngleIndex], Angles[0], Angles[AngleCount-1] );
		if ( AngleNorm < 0.25f )
		{
			Rgba.xyz = Lerp3( Range( AngleNorm, 0, 0.25f ), (float3)(1,0,0), (float3)(1,1,0) );
		}
		else if ( AngleNorm < 0.50f )
		{
			Rgba.xyz = Lerp3( Range( AngleNorm, 0.25f, 0.50f ), (float3)(1,1,0), (float3)(0,1,0) );
		}
		else if ( AngleNorm < 0.75f )
		{
			Rgba.xyz = Lerp3( Range( AngleNorm, 0.50f, 0.75f ), (float3)(0,1,0), (float3)(0,1,1) );
		}
		else
		{
			Rgba.xyz = Lerp3( Range( AngleNorm, 0.75f, 1.0f ), (float3)(0,1,1), (float3)(0,0,1) );
		}
	}
	else if ( ColourByRainbowScore )
	{
		//	score red-lime
		float RedScore = MinScoref;
		float YellowScore = Lerp( 0.5f, MinScoref, 1.0f );
		if ( Score >= YellowScore )
		{
			Rgba.xyz = Lerp3( Range( Score, YellowScore, 1.0f ), (float3)(1,1,0), (float3)(0,1,0) );
		}
		else if ( Score >= RedScore )
		{
			Rgba.xyz = Lerp3( Range( Score, RedScore, YellowScore ), (float3)(1,0,0), (float3)(1,1,0) );
		}
	}
	
	write_imagef( Frag, uv, Rgba );
#undef AngleCount
#undef ScaleCount
}



__kernel void FilterWhite(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__write_only image2d_t Frag,int RenderAsRgb)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	
	float3 SourceHsl = texture2D( Hsl, uv ).xyz;
	
	
	float MatchSat = 0.5f;
	float MatchSatHigh = 0.6f;
	float MatchSatLow = 0.5f;
	float MatchLum = 0.3f;
#define HistogramHslsCount	(15)
#define LastWhiteIndex		4
	float3 HistogramHsls[HistogramHslsCount] =
	{
		(float3)( 0, 0, 0.9f ),	//	white
		(float3)( 0, 0, 0.8f ),	//	white
		(float3)( 0, 0, 0.7f ),	//	white
		(float3)( 0, 0, 0.6f ),	//	white
		(float3)( 0, 0, 0.5f ),	//	white
		
//		(float3)( 90/360.f, 0.2f, 0.5f ),	//	white
//		(float3)( 90/360.f, 0.2f, 0.6f ),	//	white
//		(float3)( 90/360.f, 0.2f, 0.7f ),	//	white

		(float3)( 0, 0, 0.1f ),	//	black
		(float3)( 0/360.f, MatchSat, MatchLum ),
		(float3)( 20/360.f, MatchSat, MatchLum ),
		(float3)( 50/360.f, MatchSat, MatchLum ),
		(float3)( 90/360.f, MatchSat, MatchLum ),
		(float3)( 150/360.f, MatchSat, MatchLum ),
		//(float3)( 180/360.f, MatchSat, MatchSatHigh ),
		(float3)( 190/360.f, MatchSat, MatchLum ),
		(float3)( 205/360.f, MatchSat, MatchLum ),
		(float3)( 290/360.f, MatchSat, MatchLum ),
		
	};
	
	int Best = 0;
	float BestDiff = 1;
	for ( int i=0;	i<HistogramHslsCount;	i++ )
	{
		float Diff = GetHslHslDifference( SourceHsl, HistogramHsls[i] );
		if ( Diff < BestDiff )
		{
			BestDiff = Diff;
			Best = i;
		}
	}
	
	float3 FragHsl = HistogramHsls[Best];
	float4 Rgba = (float4)(0,0,0,1);
	
	bool ColourToRgb = RenderAsRgb!=0;
	bool ColourToMask = false;
	bool ColourToIndex = true;
	
	if ( ColourToRgb )
	{
		Rgba.xyz = HslToRgb( FragHsl );
	}
	else if ( ColourToMask )
	{
		if ( Best <= LastWhiteIndex )
			Rgba.xyz = 1;
	}
	else
	{
		if ( Best <= LastWhiteIndex )
			Best = 0;
		Rgba.xyz = IndexToRgb( Best, HistogramHslsCount );
	}
	
	
	//	debug show what we're matching
	if ( uv.y < 100 )
	{
		int Index = uv.x / 100;
		if ( Index < HistogramHslsCount )
		{
			Rgba.xyz = HslToRgb( HistogramHsls[Index] );
		}
	}
	
	write_imagef( Frag, uv, Rgba );
}







float4 MakeLine(float2 Pos,float2 Dir)
{
	return (float4)( Pos.x, Pos.y, Pos.x + Dir.x, Pos.y + Dir.y );
}



float2 GetRayRayIntersection(float4 RayA,float4 RayB)
{
	float2 closestPointLine1;
	float2 closestPointLine2;
	float2 linePoint1 = RayA.xy;
	float2 lineVec1 = RayA.zw;
	float2 linePoint2 = RayB.xy;
	float2 lineVec2 = RayB.zw;

	float Ax = RayA.x;
	float Ay = RayA.y;
	float Bx = RayA.x + RayA.z;
	float By = RayA.y + RayA.w;
	float Cx = RayB.x;
	float Cy = RayB.y;
	float Dx = RayB.x + RayB.z;
	float Dy = RayB.y + RayB.w;

	float2 Intersection;
	
	float  distAB, theCos, theSin, newX, ABpos ;
	
	//  Fail if either line is undefined.
	//if (Ax==Bx && Ay==By || Cx==Dx && Cy==Dy) return NO;
	
	//  (1) Translate the system so that point A is on the origin.
	Bx-=Ax; By-=Ay;
	Cx-=Ax; Cy-=Ay;
	Dx-=Ax; Dy-=Ay;
	
	//  Discover the length of segment A-B.
	distAB=sqrt(Bx*Bx+By*By);
	
	//  (2) Rotate the system so that point B is on the positive X axis.
	theCos=Bx/distAB;
	theSin=By/distAB;
	newX=Cx*theCos+Cy*theSin;
	Cy  =Cy*theCos-Cx*theSin; Cx=newX;
	newX=Dx*theCos+Dy*theSin;
	Dy  =Dy*theCos-Dx*theSin; Dx=newX;
	
	//  Fail if the lines are parallel.
	//if (Cy==Dy) return NO;
	
	//  (3) Discover the position of the intersection point along line A-B.
	ABpos=Dx+(Cx-Dx)*Dy/(Dy-Cy);
	
	//  (4) Apply the discovered position to line A-B in the original coordinate system.
	Intersection.x = Ax+ABpos*theCos;
	Intersection.y = Ay+ABpos*theSin;
	return Intersection;
}
/*
float2 GetRayRayIntersection(float4 RayA,float4 RayB)
{
	float2 closestPointLine1;
	float2 closestPointLine2;
	float2 linePoint1 = RayA.xy;
	float2 lineVec1 = RayA.zw;
	float2 linePoint2 = RayB.xy;
	float2 lineVec2 = RayB.zw;
		
	float a = dot(lineVec1, lineVec1);
	float b = dot(lineVec1, lineVec2);
	float e = dot(lineVec2, lineVec2);
		
	float d = a*e - b*b;

	//	gr; assuming not parrallel, need to handle this
	//lines are not parallel
	if ( d == 0 )
	{
		return (float2)(0,0);
	}

	float2 r = linePoint1 - linePoint2;
	float c = Vector3.Dot(lineVec1, r);
	float f = Vector3.Dot(lineVec2, r);
			
	float s = (b*f - c*e) / d;
	float t = (a*f - c*b) / d;
			
	closestPointLine1 = linePoint1 + lineVec1 * s;
	closestPointLine2 = linePoint2 + lineVec2 * t;
}
*/

float GetHoughDistance(float2 Position,float2 Origin,float Angle)
{
	float x = Position.x - Origin.x;
	float y = Position.y - Origin.y;
	//	http://www.keymolen.com/2013/05/hough-transformation-c-implementation.html
	float r = x*cosf( DegToRad(Angle) ) + y*sinf( DegToRad(Angle) );
	return r;
	/*grahams silly OTT method
	
	//	https://en.wikipedia.org/wiki/Hough_transform
	//	http://docs.opencv.org/doc/tutorials/imgproc/imgtrans/hough_lines/hough_lines.html
	//	make Ray going through Position at angle Angle
	float4 Line = MakeLine( Position, GetVectorAngle( Angle, 1 ) );
	//	make perpendicular RayP from origin to Ray
	float4 LineP = MakeLine( Origin, GetVectorAngle( Angle + 90, 1 ) );
	//	find Intersection of rays to get Distance
	float2 Intersection = GetRayRayIntersection( Line, LineP );
	float Distance = distance( Origin, Intersection );
	return Distance;
	 */
}


void DrawLine(float4 Line,__write_only image2d_t Frag,float Score)
{
	int2 wh = get_image_dim(Frag);

	float2 LineDelta = Line.zw - Line.xy;
	
	//	walk until we hit the edge of the image
	for ( int i=0;	i<400;	i++ )
	{
		if ( Line.x < 0 || Line.y < 0 || Line.x >= wh.x || Line.y >= wh.y )
			continue;
		float4 Rgba = (float4)(Score,Score,1,1);
		write_imagef( Frag, (int2)(Line.x,Line.y), Rgba );

		Line.xy += LineDelta;
	}
}



void DrawLineDirect(float2 From,float2 To,__write_only image2d_t Frag,float Score)
{
	float4 Rgba = NormalToRgba( Score );
	int2 wh = get_image_dim(Frag);

	int Steps = 700;
	for ( int i=0;	i<Steps;	i++ )
	{
		float2 Point = Lerp2( i/(float)Steps, From, To );
		if ( Point.x < 0 || Point.y < 0 || Point.x >= wh.x || Point.y >= wh.y )
			continue;
		write_imagef( Frag, (int2)(Point.x,Point.y), Rgba );
	}
}

__kernel void DrawHoughLines(int OffsetAngle,int OffsetDistance,__write_only image2d_t Frag,__read_only image2d_t Frame,global int* AngleXDistances,global float* AngleDegs,global float* Distances,int AngleCount,int DistanceCount)
{
	int AngleIndex = get_global_id(0) + OffsetAngle;
	int DistanceIndex = get_global_id(1) + OffsetDistance;
	int2 wh = get_image_dim(Frag);
	
	//	origin around the middle http://www.keymolen.com/2013/05/hough-transformation-c-implementation.html
	int2 Origin = wh/2;
	float2 Originf = (float2)(Origin.x,Origin.y);

	float DistanceNorm = DistanceIndex / (float)DistanceCount;
	float Distance = Lerp( DistanceNorm, Distances[0], Distances[DistanceCount-1] );

	float Angle = AngleDegs[AngleIndex];

	float Score = AngleXDistances[ (AngleIndex * DistanceCount ) + DistanceIndex ];

	//	check local maxima and skip line if a neighbour (near distance/near angle) is better
#if defined(CHECK_MAXIMA_ANGLES)||defined(CHECK_MAXIMA_DISTANCES)	//	OR so errors if a define is missing
	for ( int x=-CHECK_MAXIMA_ANGLES;	x<=CHECK_MAXIMA_ANGLES;	x++ )
	{
		for ( int y=-CHECK_MAXIMA_DISTANCES;	y<=CHECK_MAXIMA_DISTANCES;	y++ )
		{
			int NeighbourAngleIndex = clamp( AngleIndex+x, 0, AngleCount-1 );
			int NeighbourDistanceIndex = clamp( DistanceIndex+y, 0, DistanceCount-1 );
			float NeighbourScore = AngleXDistances[ (NeighbourAngleIndex * DistanceCount ) + NeighbourDistanceIndex ];
			if ( NeighbourScore > Score )
				return;
		}
	}
#endif
	
	if ( Score < MIN_HOUGH_SCORE )
	{
		Score = 0;
		return;
	}
	if ( Score > MAX_HOUGH_SCORE )
		Score = MAX_HOUGH_SCORE;
	Score = Range( Score, MIN_HOUGH_SCORE, MAX_HOUGH_SCORE );
	
	//	render hough line; http://docs.opencv.org/master/d9/db0/tutorial_hough_lines.html#gsc.tab=0
	float rho = Distance;
	float theta = Angle;
	float2 pt1;
	float2 pt2;
	float a = cosf( DegToRad(theta) );
	float b = sinf( DegToRad(theta) );
	float x0 = a*rho;
	float y0 = b*rho;
	pt1.x = x0 + 1000*(-b);
	pt1.y = y0 + 1000*(a);
	pt2.x = x0 - 1000*(-b);
	pt2.y = y0 - 1000*(a);
	DrawLineDirect( pt1+Originf, pt2+Originf, Frag, Score );
	
	/*
	
	//	"middle" of the line for angle
	float2 LineOrigin = (float2)(Origin.x,Origin.y) + GetVectorAngle( Angle + 90, Distance );
	
	float AngleStep = 3;
	float4 LineNorth = MakeLine( LineOrigin, GetVectorAngle( Angle, AngleStep ) );
	float4 LineSouth = MakeLine( LineOrigin, GetVectorAngle( Angle, -AngleStep ) );

	//	draw line until we hit edge of image
	DrawLine( LineNorth, Frag, Score );
	DrawLine( LineSouth, Frag, Score );
	*/
}


__kernel void DrawHoughGraph(int OffsetAngle,int OffsetDistance,__write_only image2d_t Frag,__read_only image2d_t Frame,global int* AngleXDistances,global float* AngleDegs,global float* Distances,int AngleCount,int DistanceCount)
{
	int AngleIndex = get_global_id(0) + OffsetAngle;
	int DistanceIndex = get_global_id(1) + OffsetDistance;
	int2 wh = get_image_dim(Frag);

	float u = (AngleIndex/(float)AngleCount);
	float v = (DistanceIndex/(float)DistanceCount);
	int2 uv = (int2)(u*wh.x,v*wh.y);
	
	int Scorei = AngleXDistances[ (AngleIndex * DistanceCount ) + DistanceIndex ];
	float Score = Scorei;
	Score /= 1300.f;
	//if ( Score < 0.3f || Score > 0.7f )
	//	Score = 0.f;
	Score = max( 0.f, min( Score, 1.f ) );

	float4 Rgba = NormalToRgba(Score);
	
	write_imagef( Frag, uv, Rgba );
}


bool HoughIncludePixel(__read_only image2d_t WhiteFilter,int2 uv)
{
	bool BaseWhite = RgbaToWhite( texture2D( WhiteFilter, uv ) );
	if ( !BaseWhite )
		return false;
	
	int PixelSkip = 0;
	if ( PixelSkip != 0 && ( uv.x % PixelSkip != 0 || uv.y % PixelSkip != 0 ) )
		return false;
		
	return true;
}

__kernel void HoughFilter(int OffsetX,int OffsetY,int OffsetAngle,__read_only image2d_t WhiteFilter,global int* AngleXDistances,global float* AngleDegs,global float* Distances,int AngleCount,int DistanceCount)
{
	int3 uva = (int3)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY, get_global_id(2) + OffsetAngle );
	int2 uv = uva.xy;
	int2 wh = get_image_dim(WhiteFilter);
	//	origin around the middle http://www.keymolen.com/2013/05/hough-transformation-c-implementation.html
	int2 Origin = wh/2;
	float2 Originf = (float2)(Origin.x,Origin.y);
	
	//	abort early
	if ( !HoughIncludePixel( WhiteFilter, uv ) )
		return;

	int AngleIndex = uva.z;
	float Angle = AngleDegs[AngleIndex];
	
	//	for every pixel, & angle find it's hough-distance
	//	increment the count for that [angle][distance] to generate a histogram of RAYS (not storing start/ends)
	//	later; store most common rays, and find intersections
	//	later; match pitch to intersections
	float Distancef = GetHoughDistance( (float2)(uv.x,uv.y), Originf, Angle );

	//	find index
	//	gr: so slow! make this a binary chop
	int BestDistanceIndex = -1;
	int Left = 0;
	int Right = DistanceCount-1;
	while ( true )
	{
		if ( Left >= Right )
		{
			BestDistanceIndex = Left;
			break;
		}
		
		if ( Distancef <= Distances[Left] )
		{
			BestDistanceIndex = Left;
			break;
		}
		if ( Distancef >= Distances[Right] )
		{
			BestDistanceIndex = Right;
			break;
		}
		
		//	chop
		int Mid = Left + ((Right-Left)/2);
		if ( Distancef < Distances[Mid] )
		{
			Right = Mid;
			Left++;
			continue;
		}
		else
		{
			Left = Mid;
			Right--;
			continue;
		}
	}
	
	int DistanceIndex = BestDistanceIndex;
	int AngleXDistanceIndex = (AngleIndex * DistanceCount) + DistanceIndex;
	
	//	stop convergence at the ends of the distance spectrum (allows smaller distances for testing)
	if( DistanceIndex != 0 && DistanceIndex != DistanceCount-1)
		atomic_inc( &AngleXDistances[AngleXDistanceIndex] );

}


__kernel void HoughFilterPixels(int OffsetX,int OffsetY,__read_only image2d_t WhiteFilter,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	
	//	abort early
	if ( !HoughIncludePixel( WhiteFilter, uv ) )
	{
		write_imagef( Frag, uv, (float4)(0,0,0,1) );
		return;
	}
	
	write_imagef( Frag, uv, (float4)(1,1,1,1) );
}




float WhiteSample(int x,int y,__read_only image2d_t Image,int2 uv)
{
	return RgbaToWhite( texture2D( Image, uv+(int2)(x,y) ) ) ? 1:0;
}

bool GreenSample(int x,int y,__read_only image2d_t Image,int2 uv)
{
	return RgbaToGreen( texture2D( Image, uv+(int2)(x,y) ) );
}

float WhiteFilterGreenNearBy(__read_only image2d_t Image,int2 uv)
{
	int GreenCount = 0;

	GreenCount += GreenSample( -5, -5, Image, uv );
	GreenCount += GreenSample(  0, -5, Image, uv );
	GreenCount += GreenSample(  5, -5, Image, uv );
	
	GreenCount += GreenSample( -5,  0, Image, uv );
//	GreenCount += GreenSample(  0,  0, Image, uv );
	GreenCount += GreenSample(  5,  0, Image, uv );
	
	GreenCount += GreenSample( -5,  5, Image, uv );
	GreenCount += GreenSample(  0,  5, Image, uv );
	GreenCount += GreenSample(  5,  5, Image, uv );
	
	return GreenCount >= 2;
}

float WhiteFilterSobel(__read_only image2d_t Image,int2 uv)
{
	float hc =
	WhiteSample(-1,-1, Image,uv) *  1. + WhiteSample( 0,-1, Image,uv) *  2.
	+WhiteSample( 1,-1, Image,uv) *  1. + WhiteSample(-1, 1, Image,uv) * -1.
	+WhiteSample( 0, 1, Image,uv) * -2. + WhiteSample( 1, 1, Image,uv) * -1.;
	
	float vc =
	WhiteSample(-1,-1, Image,uv) *  1. + WhiteSample(-1, 0, Image,uv) *  2.
	+WhiteSample(-1, 1, Image,uv) *  1. + WhiteSample( 1,-1, Image,uv) * -1.
	+WhiteSample( 1, 0, Image,uv) * -2. + WhiteSample( 1, 1, Image,uv) * -1.;
	
	return WhiteSample(0, 0, Image,uv) * pow( (vc*vc + hc*hc), .6f);
}



__kernel void WhiteFilterEdges(int OffsetX,int OffsetY,__read_only image2d_t WhiteFilterGroup,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	
	bool BaseWhite = RgbaToWhite( texture2D( WhiteFilterGroup, uv ) );
	
	//	if it's white, only keep if there's green nearby
	if ( BaseWhite )
	{
		BaseWhite = WhiteFilterGreenNearBy( WhiteFilterGroup, uv );
	}
	
	//	if it's a white pixel, only keep if it's an edge
#if defined(EDGE_FILTER_WHITE_PIXELS)
	if ( BaseWhite )
	{
		BaseWhite = WhiteFilterSobel( WhiteFilterGroup, uv ) > 0;
	}
#endif
	
	float4 Rgba = BaseWhite ? (float4)(0,0,0,1) : (float4)(1,0,0,1);
	write_imagef( Frag, uv, Rgba );
}





