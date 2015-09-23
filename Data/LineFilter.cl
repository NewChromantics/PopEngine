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
//	if ( Degrees < 0 )		Degrees += 360;
//	if ( Degrees > 360 )	Degrees -= 360;
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



float hue2rgb(float p,float q,float t)
{
	if(t < 0) t += 1.f;
	if(t > 1) t -= 1.f;
	if(t < 1.f/6.f) return p + (q - p) * 6.f * t;
	if(t < 1.f/2.f) return q;
	if(t < 2.f/3.f) return p + (q - p) * (2.f/3.f - t) * 6.f;
	return p;
}

float3 HslToRgb(float3 Hsl)
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



void GetSample(float* SampleScores,int2 PatternSize,float2 SampleScale,float AngleDeg,__read_only image2d_t Hsl,float3 BaseHsl,float HslDiffMax,float2 BaseUv)
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
float2 GetPatternScore(float2 SampleSizePx,float AngleDeg,__read_only image2d_t Hsl,float HslDiffMax,float2 BaseUv)
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
}



bool RgbaToWhite(float4 Rgba)
{
	return (Rgba.x+Rgba.y+Rgba.z) == 0;
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
}


float3 IndexToRgb(int Index,int IndexCount)
{
	if ( Index == 0 )
		return (float3)(0,0,0);
	
	float Hue = Index / (float)IndexCount;
	float3 Hsl = (float3)( Hue, 1.0f, 0.5f );

	return HslToRgb( Hsl );
}


__kernel void FilterWhite(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__write_only image2d_t Frag)
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
	
	bool ColourToReal = false;
	bool ColourToMask = false;
	bool ColourToIndex = true;
	
	if ( ColourToReal )
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

