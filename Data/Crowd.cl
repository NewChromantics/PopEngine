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
							   int MaxSteps
							   )
{
	int x = get_global_id(0) + OffsetX;
	int y = get_global_id(1) + OffsetY;
	int2 xy = (int2)( x, y );
	int2 wh = get_image_dim(Image);

	bool HitLeftEdge = false;
	bool HitRightEdge = false;

	int2 Left = (int2)(-1,0);
	int2 Right = (int2)(1,0);
	int2 Up = (int2)(0,-1);
	int2 Down = (int2)(0,1);
//	int2 Left = (int2)(-1,-1);
//	int2 Right = (int2)(1,1);
//	int2 Up = (int2)(1,-1);
//	int2 Down = (int2)(-1,1);
	
	
	int WalkLeft = GetWalkHsl( xy, Left, Image, MaxSteps, MaxDiff, &HitLeftEdge );
	int WalkRight = GetWalkHsl( xy, Right, Image, MaxSteps, MaxDiff, &HitRightEdge );
	int WalkUp = GetWalkHsl( xy, Up, Image, MaxSteps, MaxDiff, &HitLeftEdge );
	int WalkDown = GetWalkHsl( xy, Down, Image, MaxSteps, MaxDiff, &HitRightEdge );
	
	bool MaxedWidth = ( WalkLeft + WalkRight >= MaxSteps+MaxSteps );
	bool MaxedHeight = ( WalkUp + WalkDown >= MaxSteps+MaxSteps );

	float Alignmentx = WalkLeft / (float)(WalkLeft+WalkRight);
	float Alignmenty = WalkUp / (float)(WalkUp+WalkDown);
	
	//	noise
	float4 Rgba = (float4)( 0, 0, 0, 1 );
	if ( WalkLeft + WalkRight + WalkUp + WalkDown < 4 )
	{
		Rgba = (float4)(1,0,0,1);
	}
	else if ( MaxedWidth || MaxedHeight )
	{
		Rgba = (float4)(0,0,1,0);
	}
	else
	{
		//	change 0..1 to score 0..1 from center
		Alignmentx = 1.f - (fabsf( Alignmentx - 0.5f ) * 2.f);
		Alignmenty = 1.f - (fabsf( Alignmenty - 0.5f ) * 2.f);

		//	edges
		if ( Alignmentx < MinAlignment || Alignmenty < MinAlignment )
		{
			Rgba = (float4)(0,0,1,0);
		}
		else
		{
			//	colour width
			float Radius = min( WalkLeft+WalkRight, WalkUp+WalkDown ) / (float)MaxSteps;
			Rgba.x = Radius;
			Rgba.y = Alignmentx * Alignmenty;
			Rgba.z = 0;
		}
	}
	
	if ( Rgba.w > 0 )
		write_imagef( Frag, xy, Rgba );
}

