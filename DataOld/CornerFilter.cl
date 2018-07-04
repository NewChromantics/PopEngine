#define const	__constant

//const int SampleRadius = 8;	//	range 0,9
#define SampleRadius	5	//	range 0,9
const int HitCountMin = 2;
const bool IncludeSelf = true;


const float MinLum = 0.5f;
const float Tolerance = 0.01f;
const float AngleRange = 360.0f;


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



ulong GetRingMatches(float3 BaseHsl,int2 BaseUv,float Radius,int AnglesToTest,__read_only image2d_t Hsl)
{
	ulong Result = 0;
	for ( int i=0;	i<AnglesToTest;	i++ )
	{
		float AngleStep = AngleRange / (float)(AnglesToTest);
		float Angle = (float)i * AngleStep;
		bool Match = GetRingMatch( BaseHsl, BaseUv, Radius, Angle, Hsl );
		
		if ( Match )
			Result |= 1<<i;
	}

	return Result;
}


float GetAngleFromRing(ulong Ring,int Angles)
{
	int FirstAngle = -1;
	for ( int i=0;	i<Angles;	i++ )
	{
		bool Match = ( Ring & (1<<i))!=0;
		if ( !Match )
			continue;
		
		return (float)i * ( AngleRange / (float)Angles );
	}
	return -1;
}

float2 GetVectorFromRing(ulong Ring,int Angles)
{
	float Angle = GetAngleFromRing( Ring, Angles );
	if ( Angle < 0 )
		return (float2)(0,0);
	
	return GetVectorAngle( Angle, 1 );
}

int CountBits(ulong x)
{
	int Count = 0;
	for ( int i=0;	i<64/8;	i++ )
	{
		bool Bit = (x & (1<<i))!=0;
		Count += Bit;
	}
	return Count;
}

__kernel void CornerFilterAngles(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__write_only image2d_t Frag)
{
	int x = get_global_id(0) + OffsetX;
	int y = get_global_id(1) + OffsetY;
	int2 uv = (int2)( x, y );

	float3 BaseHsl = texture2D( Hsl, uv ).xyz;

	//	mark all angles that are NOT darker than me
	ulong Rings[3] = {0,0,0};
	float RingRadius[3] = {1,2,3 };
	int Angles = 10;
	if ( BaseHsl.z >= MinLum )
	{
		Rings[0] = GetRingMatches( BaseHsl.xyz, uv, RingRadius[0], Angles, Hsl );
		Rings[1] = GetRingMatches( BaseHsl.xyz, uv, RingRadius[1], Angles, Hsl );
		Rings[2] = GetRingMatches( BaseHsl.xyz, uv, RingRadius[2], Angles, Hsl );
	}

	//	eliminate non lines
	ulong Ring = Rings[0] & Rings[1] & Rings[2];
	float4 Rgba;
	int BitCount = CountBits( Ring );
	if ( BitCount >= Angles )
	{
		Rgba = (float4)(0,0,1,1);
	}
	else if ( BitCount < 2 )
	{
		Rgba = (float4)(0,0,0,1);
	}
	else
	{
		float2 RingAngleVector = GetVectorFromRing( Ring, Angles );
		Rgba = (float4)( RingAngleVector.x, RingAngleVector.y, 0, 1 );
		Rgba = (float4)(0,1,0,1);
	}
	/*
	Rgba.xyz = (float)BitCount / (float)Angles;
	if ( BitCount >= Angles )
	{
		Rgba = (float4)(0,0,1,1);
	}
	*/
	write_imagef( Frag, uv, Rgba );
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

int GetWalk(int2 xy,int2 WalkStep,__read_only image2d_t Image,int MaxSteps,float MaxDiff,bool* HitEdge)
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

__kernel void CornerFilter(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__read_only image2d_t Frame,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	int2 wh = get_image_dim(Hsl);
	
	//	walk left & right until we hit a HSL edge
	int MaxDistance = 20;
	float MaxHslDiff = 0.03f;
	
	bool HitRightEdge = false;
	bool HitLeftEdge = false;
	int Right = GetWalk( uv, (int2)(1,0), Hsl, MaxDistance, MaxHslDiff, &HitRightEdge );

	HitRightEdge = (Right < MaxDistance);
	
	//	if we hit an image edge going left or right, then let the other direction go further
	int Left = GetWalk( uv, (int2)(-1,0), Hsl, MaxDistance + (HitRightEdge?MaxDistance-Right:0), MaxHslDiff, &HitLeftEdge );
	HitLeftEdge = (Left < MaxDistance);

	if ( HitLeftEdge )
	{
		//	re-calc right if we hit the left edge
		Right = GetWalk( uv, (int2)(1,0), Hsl, MaxDistance + (HitLeftEdge?MaxDistance-Left:0), MaxHslDiff, &HitRightEdge );
	}
	
	float Score = (float)(Left+Right) / (float)(MaxDistance+MaxDistance);
	
	//float4 Rgba = (float4)( 0, Score, 0, 1 );
	float4 Rgba = texture2D( Frame, uv );
	if ( Score > 0.2f )
		Rgba = (float4)(0,0,0,1);
	
	write_imagef( Frag, uv, Rgba );
}
