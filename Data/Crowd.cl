#include "Common.cl"


/*



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
*/
int GetWalkHsl(int2 xy,int2 WalkStep,__read_only image2d_t Image,int MaxSteps,float MaxDiff,bool* HitEdge)
{
	float3 BaseHsl = texture2D( Image, xy ).xyz;
	int2 wh = get_image_dim(Image);
	int2 Min = (int2)(0,0);
	int2 Max = (int2)(wh.x-1,wh.y-1);
	
	int Step = 0;
	for ( Step=0;	Step<=MaxSteps;	Step++ )
	{
		int2 Offset = WalkStep * (Step+1);
		int2 Matchxy = xy + Offset;
		if ( Matchxy.x < Min.x || Matchxy.y < Min.y || Matchxy.x > Max.x || Matchxy.y > Max.y )
		{
			*HitEdge = true;
			break;
		}
		
		float3 MatchHsl = texture2D( Image, Matchxy ).xyz;
		float Diff = GetHslHslDifference( BaseHsl, MatchHsl );
		if ( Diff > MaxDiff )
			break;
	}
	return Step;
}




__kernel void CalcHslScanlines(int OffsetX,int OffsetY,__write_only image2d_t Frag,__read_only image2d_t Image,
							   float MinAlignment,
							   float MaxDiff,
							   int MaxSteps,
							   int AllowAlpha	//	on when displaying over an image for visualisation
							   )
{
	int x = get_global_id(0) + OffsetX;
	int y = get_global_id(1) + OffsetY;
	int2 xy = (int2)( x, y );
	int2 wh = get_image_dim(Image);

	bool HitLeftEdge = false;
	bool HitRightEdge = false;

	
	int MinWidth = 10;
	MinAlignment = 0.9f;
	MaxSteps = 10;
	int StepStep = 2;
	int2 Left = (int2)(-StepStep,0);
	int2 Right = (int2)(StepStep,0);
	int2 Up = (int2)(0,-StepStep);
	int2 Down = (int2)(0,StepStep);
//	int2 Left = (int2)(-StepStep,-StepStep);
//	int2 Right = (int2)(StepStep,StepStep);
//	int2 Up = (int2)(StepStep,-StepStep);
//	int2 Down = (int2)(-StepStep,StepStep);
	
	
	int WalkLeft = GetWalkHsl( xy, Left, Image, MaxSteps, MaxDiff, &HitLeftEdge );
	int WalkRight = GetWalkHsl( xy, Right, Image, MaxSteps, MaxDiff, &HitRightEdge );
	int WalkUp = GetWalkHsl( xy, Up, Image, MaxSteps, MaxDiff, &HitLeftEdge );
	int WalkDown = GetWalkHsl( xy, Down, Image, MaxSteps, MaxDiff, &HitRightEdge );
	
	bool MaxedWidth = ( WalkLeft + WalkRight >= MaxSteps+MaxSteps );
	bool MaxedHeight = ( WalkUp + WalkDown >= MaxSteps+MaxSteps );

	float Alignmentx = WalkLeft / (float)(WalkLeft+WalkRight);
	float Alignmenty = WalkUp / (float)(WalkUp+WalkDown);
	
	bool ValidOutput = false;
	
	//	noise
	float4 Rgba = (float4)( 0, 0, 0, 1 );
	if ( WalkLeft + WalkRight + WalkUp + WalkDown < MinWidth )
	{
		bool ShowNoise = false;
		Rgba = (float4)(1,0,0,ShowNoise?1:0);
		ValidOutput = false;
	}
	else if ( MaxedWidth || MaxedHeight )
	{
		bool ShowBackground = false;
		Rgba = (float4)(0,1,0,ShowBackground?1:0);
		ValidOutput = false;
	}
	else
	{
		//	change 0..1 to score 0..1 from center
		Alignmentx = 1.f - (fabsf( Alignmentx - 0.5f ) * 2.f);
		Alignmenty = 1.f - (fabsf( Alignmenty - 0.5f ) * 2.f);

		//	allow edges to come through
		
		
		//	filter out middle only
		if ( Alignmentx < MinAlignment || Alignmenty < MinAlignment )
		{
			Rgba = (float4)(0,0,0,0);
			ValidOutput = false;
		}
		else
		{
			//	colour width
			float Radius = min( WalkLeft+WalkRight, WalkUp+WalkDown ) / (float)MaxSteps;
			Rgba.x = Radius;
			Rgba.y = Alignmentx * Alignmenty;
			ValidOutput = true;
		}
	}
	
	Rgba.z = ValidOutput ? 0 : 1;
	
	if ( !AllowAlpha )
		Rgba.w = 1;
	
	if ( Rgba.w > 0 )
		write_imagef( Frag, xy, Rgba );
}




__kernel void ShowPrevNextDiff(int OffsetX,int OffsetY,__write_only image2d_t Frag,__read_only image2d_t Next,__read_only image2d_t Prev)
{
	int x = get_global_id(0) + OffsetX;
	int y = get_global_id(1) + OffsetY;
	int2 xy = (int2)( x, y );
	int2 wh = get_image_dim(Frag);
	
	float3 HslSample = texture2D( Next, xy ).xyz;
	float3 PrevHslSample = texture2D( Prev, xy ).xyz;
	
	float3 Diff = HslSample - PrevHslSample;
	Diff = fabs( Diff );
	
	float4 Rgba = (float4)( Diff.x, Diff.y, Diff.z, 1 );
	write_imagef( Frag, xy, Rgba );
}

static bool IsValidScanlineSample(float3 Scanline)
{
	return (Scanline.z == 0);
}



static float GetNearestNeighbourScore(float3 ScanlineA,float3 HslA,float3 ScanlineB,float3 HslB)
{
	float MinAlignmentScore = 0.95f;
	float AlignmentA = ScanlineA.y;
	float AlignmentB = ScanlineB.y;
	float AlignmentScore = 1.f - fabs( AlignmentA - AlignmentB );
	if ( AlignmentScore < MinAlignmentScore )
		AlignmentScore = 0;

	float ValidOutputA = IsValidScanlineSample(ScanlineA) ? 1 : 0;
	float ValidOutputB = IsValidScanlineSample(ScanlineB) ? 1 : 0;
	
	
	float MaxHslDiff = 0.2f;
	float ColourScore = GetHslHslDifference( HslA, HslB );
	if ( ColourScore > MaxHslDiff )
		ColourScore = 0;
	else
		ColourScore = 1 - (ColourScore/MaxHslDiff);
	
	return ValidOutputA * ValidOutputB * AlignmentScore * ColourScore;
}


static void DrawBox(int2 Center,__write_only image2d_t Image,int Radius,float4 Colour)
{
	Radius -= 1;
	int2 wh = get_image_dim(Image);
	int2 Min,Max;
	Min.x = max( 0, Center.x - Radius );
	Min.y = max( 0, Center.y - Radius );
	Max.x = min( wh.x-1, Center.x + Radius );
	Max.y = min( wh.y-1, Center.y + Radius );

	for ( int x=Min.x;	x<=Max.x;	x++ )
	{
		for ( int y=Min.y;	y<=Max.y;	y++ )
		{
			write_imagef( Image, (int2)(x,y), Colour );
		}
	}
}

__kernel void FindNearestNeighbour(int OffsetX,int OffsetY,__write_only image2d_t Frag,__read_only image2d_t ScanlineWidths,__read_only image2d_t PrevScanlineWidths,__read_only image2d_t Hsl,__read_only image2d_t PrevHsl)
{
	int x = get_global_id(0) + OffsetX;
	int y = get_global_id(1) + OffsetY;
	int2 xy = (int2)( x, y );
	int2 wh = get_image_dim(Frag);
	
	//	less noise by skipping
//	if ( x % 3 != 0 )	return;
//	if ( y % 3 != 0 )	return;
	
	float3 ScanlineSample = texture2D( ScanlineWidths, xy ).xyz;
	float3 HslSample = texture2D( Hsl, xy ).xyz;
	
	if ( !IsValidScanlineSample( ScanlineSample ) )
	{
		return;
	}
	

	int WindowRadius = 5;
	float WindowStepX = 2.f;
	float WindowStepY = 2.f;
	float2 Bestxyf;
	float BestxyScore = -1.f;
	float MinScore = 0.01f;
	float MaxDistance = length( (float2)(WindowRadius,WindowRadius) );
	
	//	search window
	for ( int Matchx=-WindowRadius;	Matchx<=WindowRadius;	Matchx++ )
	{
		for ( int Matchy=-WindowRadius;	Matchy<=WindowRadius;	Matchy++ )
		{
			float2 Matchxyf = (float2)( (float)Matchx*WindowStepX, (float)Matchy*WindowStepY );
			int2 Matchxy = (int2)( Matchxyf.x, Matchxyf.y );
			float3 MatchHsl = texture2D( PrevHsl, xy+Matchxy ).xyz;
			float3 MatchScanline = texture2D( PrevScanlineWidths, xy+Matchxy ).xyz;
			float MatchScore = GetNearestNeighbourScore( ScanlineSample, HslSample, MatchScanline, MatchHsl );
			/*
			//	todo; if == ... or a tolerance... then favour nearest to 0,0
			float ScoreDiff = fabs( MatchScore - BestxyScore );
			if ( ScoreDiff < 0.01f )
			{
				//	further away
				if ( length(Matchxyf) > length(Bestxyf) )
					continue;
			}
			else*/
			{
				if ( MatchScore <= BestxyScore )
					continue;
			}
			
			Bestxyf = Matchxyf;
			BestxyScore = MatchScore;
		}
	}
	
	float4 Rgba = (float4)(0,0,0,1);
	bool RenderBestScore = true;
	bool RenderDeltaLine = true;
	bool RenderDeltaXy = true;
	
	//	no match
	if ( BestxyScore < MinScore )
	{
		Rgba = (float4)(0,0,1,0);
	}
	else if ( RenderBestScore )
	{
		Rgba.xyz = NormalToRgb( BestxyScore );
	}
	else if ( RenderDeltaLine )
	{
		float2 Delta = Bestxyf;
		float2 xyf = (float2)(xy.x,xy.y);
		
		float Distance = length(Bestxyf) / MaxDistance;
		//	for "no movement" draw a white box
		if ( Distance < 0.001f )
		{
			DrawBox( xy, Frag, 1, (float4)(1,1,1,1) );
			return;
		}
		
		float Angle = atan2( Delta.x, Delta.y );
		Angle = RadToDeg( Angle );
		if ( Angle < 360.f )	Angle += 360.f;
		if ( Angle >= 360.f )	Angle -= 360.f;
		Angle = Range( Angle, 0, 360.f );
		
		DrawLineDirect( xyf, xyf+Delta, Frag, Angle );
		return;
	}
	else if ( RenderDeltaXy )
	{
		float2 Delta = Bestxyf;
		Rgba.x = (Delta.x / (WindowRadius*2.f)) + 0.5f;
		Rgba.y = (Delta.y / (WindowRadius*2.f)) + 0.5f;
		Rgba.z = 0;
		Rgba.w = 1;
	}
	else
	{
		//	output delta to neighbour
		//Rgba = (float4)( Distance, Distance, Distance, 1 );
		float2 Delta = Bestxyf;
		float Distance = length(Bestxyf) / MaxDistance;
		
		if ( Distance < 0.001f )
		{
			Rgba = (float4)( 1, 1, 1, 1 );
		}
		else
		{
			//	distance as score (bright green = closer)
			//Rgba = (float4)( 0, 1-Distance, 0, 1 );
			
			/*
			 //	delta as 2d normal
			 Delta = normalise( Delta );
			 Rgba.x = (Delta.x / WindowRadius);
			 */
			
			//	delta as coloured angle
			
			float Angle = atan2( Delta.x, Delta.y );
			Angle = RadToDeg( Angle );
			if ( Angle < 360.f )	Angle += 360.f;
			if ( Angle >= 360.f )	Angle -= 360.f;
			Angle = Range( Angle, 0, 360.f );
			float3 Rgb = NormalToRgb( Angle );
			Rgba = (float4)( Rgb.x, Rgb.y, Rgb.z, 1 );
			
		}
	}
	
	if ( Rgba.w > 0 )
	{
		DrawBox( xy, Frag, 2, Rgba );
	}
}


__kernel void HighlightTargets(int OffsetX,int OffsetY,__write_only image2d_t Frag,__read_only image2d_t ScanlineWidths,__read_only image2d_t PrevScanlineWidths,__read_only image2d_t Hsl,__read_only image2d_t PrevHsl)
{
	int x = get_global_id(0) + OffsetX;
	int y = get_global_id(1) + OffsetY;
	int2 xy = (int2)( x, y );
	int2 wh = get_image_dim(Frag);
	
	float3 ScanlineSample = texture2D( ScanlineWidths, xy ).xyz;
	float3 PrevScanlineSample = texture2D( PrevScanlineWidths, xy ).xyz;

	float4 Rgba = 0;
	
	if ( IsValidScanlineSample( PrevScanlineSample ) )
	{
		DrawBox( xy, Frag, 1, (float4)(1,0,0,1) );
	}
	if ( IsValidScanlineSample( ScanlineSample ) )
	{
		DrawBox( xy, Frag, 1, (float4)(0,1,0,1) );
	}
	
	if ( Rgba.w > 0 )
		write_imagef( Frag, xy, Rgba );
}

