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

__kernel void CornerFilter(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__write_only image2d_t Frag)
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
