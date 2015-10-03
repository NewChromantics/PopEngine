#include "Common.cl"
#include "Array.cl"


DECLARE_DYNAMIC_ARRAY(float8);


//const int SampleRadius = 8;	//	range 0,9
#define SampleRadius	5	//	range 0,9
const int HitCountMin = 2;
const bool IncludeSelf = true;

const float MinLum = 0.5f;
const float Tolerance = 0.01f;
const float AngleRange = 360.0f;


//	gr: this score needs to vary for the MAX pixels that could even be on a line (a line clipped in a corner will have less pixels, but could be perfect, and smaller resolution = less pixels)
//	gr: now in config as varies on params so much
//#define MIN_HOUGH_SCORE	100
//#define MAX_HOUGH_SCORE	500


//	filter out lines if a neighbour is better
#define CHECK_MAXIMA_ANGLES		20
#define CHECK_MAXIMA_DISTANCES	30



static bool GetRingMatch(float3 BaseHsl,int2 BaseUv,float Radius,float AngleDeg,__read_only image2d_t Hsl)
{
	float2 UvOffset = GetVectorAngle( AngleDeg, Radius );
	float2 Uv = (float2)(BaseUv.x + UvOffset.x, BaseUv.y + UvOffset.y);

	float4 MatchHsl = texture2D( Hsl, (int2)(Uv.x,Uv.y) );
	
	if ( MatchHsl.z+Tolerance < BaseHsl.z )
		return false;

	return true;
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



static bool RgbaToWhite(float4 Rgba)
{
//#define HistogramHslsCount	(15)
	int Index = RgbToIndex( Rgba.xyz, 15 );
	return Index == 0;
}

static bool RgbaToGreen(float4 Rgba)
{
	//#define HistogramHslsCount	(15)
	int Index = RgbToIndex( Rgba.xyz, 15 );
	return Index==9;
}


static void GetWhiteSample(float* SampleScores,int2 PatternSize,float2 SampleScale,float AngleDeg,__read_only image2d_t WhiteFilter,float2 BaseUv)
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
static float2 GetWhitePatternScore(float2 SampleSizePx,float AngleDeg,__read_only image2d_t WhiteFilter,float2 BaseUv)
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



__kernel void FilterWhite(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__write_only image2d_t Frag,int RenderAsRgb,int DrawPalette)
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
	if ( DrawPalette && uv.y < 100 )
	{
		int Index = uv.x / 100;
		if ( Index < HistogramHslsCount )
		{
			Rgba.xyz = HslToRgb( HistogramHsls[Index] );
		}
	}
	
	write_imagef( Frag, uv, Rgba );
}



static float4 MakeLine(float2 Pos,float2 Dir)
{
	return (float4)( Pos.x, Pos.y, Pos.x + Dir.x, Pos.y + Dir.y );
}



static float GetHoughDistance(float2 Position,float2 Origin,float Angle)
{
	//	http://www.keymolen.com/2013/05/hough-transformation-c-implementation.html
	float2 xy = Position - Origin;
	float Cos;
	float Sin = sincos( DegToRad(Angle), &Cos );
	float r = Cos*xy.x + Sin*xy.y;
	return r;
}



static float4 GetHoughLine(float Distance,float Angle,float2 Originf)
{
	float rho = Distance;
	float theta = Angle;
	float Cos;
	float Sin = sincos( DegToRad(theta), &Cos );
	
	//	center of the line
	float x0 = Cos*rho + Originf.x;
	float y0 = Sin*rho + Originf.y;
	
	//	scale by an arbirtry number, but still want to be resolution-independent
	float Length = 2000;
	
	float4 Line;
	Line.xy = (float2)( x0 + Length*(-Sin), y0 + Length*(Cos) );
	Line.zw = (float2)( x0 - Length*(-Sin), y0 - Length*(Cos) );
	return Line;
}

__kernel void DrawHoughLinesDynamic(int OffsetAngle,int OffsetDistance,__write_only image2d_t Frag,__read_only image2d_t Frame,global int* AngleXDistances,global float* AngleDegs,global float* Distances,int AngleCount,int DistanceCount,int HoughScoreMin,int HoughScoreMax)
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

	
	if ( Score < HoughScoreMin )
	{
		Score = 0;
		return;
	}
	if ( Score > HoughScoreMax )
		Score = HoughScoreMax;
	Score = Range( Score, HoughScoreMin, HoughScoreMax );
	
	//	render hough line; http://docs.opencv.org/master/d9/db0/tutorial_hough_lines.html#gsc.tab=0
	float4 Line = GetHoughLine(Distance,Angle,Originf );
	DrawLineDirect( Line.xy, Line.zw, Frag, Score );
}


__kernel void DrawHoughLines(int OffsetIndex,__write_only image2d_t Frag,global float8* HoughLines,int ColourToVertical)
{
	int LineIndex = get_global_id(0) + OffsetIndex;
	float8 HoughLine = HoughLines[LineIndex];
	
	float2 LineStart = HoughLine.xy;
	float2 LineEnd = HoughLine.zw;
	float Angle = HoughLine[4];
	float Distance = HoughLine[5];
	
	//	draw vertical/non vertical
	float Score = ColourToVertical ? HoughLine[7] : HoughLine[6];
	
	DrawLineDirect( LineStart, LineEnd, Frag, Score );
}




__kernel void ExtractHoughLines(int OffsetAngle,
								int OffsetDistance,
								global int* AngleXDistances,
								global float* AngleDegs,
								global float* Distances,
								int AngleCount,
								int DistanceCount,
								global float8* Matches,
								global volatile int* MatchesCount,
								int MatchesMax,
								__read_only image2d_t WhiteFilter,
								float HoughScoreMin,
								float HoughScoreMax
								)
{
	int AngleIndex = get_global_id(0) + OffsetAngle;
	int DistanceIndex = get_global_id(1) + OffsetDistance;
	int2 wh = get_image_dim(WhiteFilter);
	
	//	origin around the middle http://www.keymolen.com/2013/05/hough-transformation-c-implementation.html
	int2 Origin = wh/2;
	float2 Originf = (float2)(Origin.x,Origin.y);
	
	float DistanceNorm = DistanceIndex / (float)DistanceCount;
	float Distance = Lerp( DistanceNorm, Distances[0], Distances[DistanceCount-1] );
	
	float Angle = AngleDegs[AngleIndex];
	
	float Score = AngleXDistances[ (AngleIndex * DistanceCount ) + DistanceIndex ];
	
	//	gr: altohugh this score hasn't been corrected, because they're neighbours, we assume the score entropy won't vary massively.
	//		if we wanted to check with corrected scores we'd have to do this as a seperate stage or calc neighbour corrected scores here which might be a bit expensive
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

	//	correct the score to be related to maximum possible length (this lets us handle mulitple resolutions)
	float4 Line = GetHoughLine( Distance, Angle, Originf );
	int Border = 0;	//	testing clip
	Line = ClipLine( Line, (float4)(Border,Border,wh.x-Border,wh.y-Border) );
	float LineLength = length( Line.xy - Line.zw );
	
	//	entirely clipped lines... should we have any at all? maybe when distance is too far out
	if ( LineLength <= 0 )
		return;

	//	gr: note: we can have more-pixels for a line depending on distance grouping
	float HoughDistanceStep = 0.5f;
	float LineMaxPixels = LineLength * HoughDistanceStep;

	Score /= LineMaxPixels;
	if ( Score < HoughScoreMin )
	{
		return;
	}

	Score = clamp( Score, (float)HoughScoreMin, (float)HoughScoreMax );
	Score = Range( Score, HoughScoreMin, HoughScoreMax );
	

	//	make output
	float8 LineAndMeta;
	LineAndMeta.xyzw = Line;
	LineAndMeta[4] = Angle;
	LineAndMeta[5] = Distance;
	LineAndMeta[6] = Score;
	LineAndMeta[7] = LineMaxPixels;
	
	TArray_float8 MatchArray = { Matches, MatchesCount, MatchesMax };
	PushArray_float8( MatchArray, &LineAndMeta );
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


static bool HoughIncludePixel(__read_only image2d_t WhiteFilter,int2 uv)
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




static float WhiteSample(int x,int y,__read_only image2d_t Image,int2 uv)
{
	return RgbaToWhite( texture2D( Image, uv+(int2)(x,y) ) ) ? 1:0;
}

static bool GreenSample(int x,int y,__read_only image2d_t Image,int2 uv)
{
	return RgbaToGreen( texture2D( Image, uv+(int2)(x,y) ) );
}

static float WhiteFilterGreenNearBy(__read_only image2d_t Image,int2 uv)
{
	int GreenCount = 0;
	int GreenRadius = 5;

	GreenCount += GreenSample( -GreenRadius, -GreenRadius, Image, uv );
	GreenCount += GreenSample(  0, -GreenRadius, Image, uv );
	GreenCount += GreenSample(  GreenRadius, -GreenRadius, Image, uv );
	
	GreenCount += GreenSample( -GreenRadius,  0, Image, uv );
//	GreenCount += GreenSample(  0,  0, Image, uv );
	GreenCount += GreenSample(  GreenRadius,  0, Image, uv );
	
	GreenCount += GreenSample( -GreenRadius,  GreenRadius, Image, uv );
	GreenCount += GreenSample(  0,  GreenRadius, Image, uv );
	GreenCount += GreenSample(  GreenRadius,  GreenRadius, Image, uv );
	
	return GreenCount >= 2;
}

static float WhiteFilterSobel(__read_only image2d_t Image,int2 uv)
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





