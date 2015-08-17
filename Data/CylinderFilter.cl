#define const	__constant

const float MaxRadiusPx = 30;
const float MinInnerHitCountPercent = 0.99;



static float Range(float Value,float Start,float End)
{
	return (Value-Start) / (End-Start);
}

static float Lerp(float Value,float Start,float End)
{
	return Start + (Value * (End-Start));
}


static float4 texture2D(__read_only image2d_t Image,float2 uv)
{
	sampler_t Sampler = CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
	
	return read_imagef( Image, Sampler, uv );
}

static bool GetHit(__read_only image2d_t Image,float2 uv)
{
	float4 Sample = texture2D( Image, uv );
	
	return (Sample.w > 0.5f) ? 1 : 0;
}


//	BHT is bigdistance hitcount totaltests
#define TTestOutput float3
/*
 int3
 {
 float	BigDistance;
 int		HitCount;
 int		TotalTests;
 };
 */

static float lengthsquared(float2 xy)
{
	return (xy.x*xy.x) + (xy.y*xy.y);
}

static TTestOutput DoTest(__read_only image2d_t Image,float2 xy,float2 offset,TTestOutput TestOutput)
{
	float4 Sample = texture2D( Image, xy+offset );
	bool MatchValid = (Sample.w > 0.5f);
	
	TestOutput.z ++;//= MatchValid ? 1 :0;
	
	if ( MatchValid )
	{
		TestOutput.y++;
		float Distance = length(offset);
		TestOutput.x = max( TestOutput.x, Distance );
	}
	
	return TestOutput;
}


/*
 //	make a grid spiraling out from bottom right, and apply for 4 corners
 00	aa	dd	hh	mm	ss	a	i
 bb	cc	ee	ii	nn	tt	b	j
 ff	gg	jj	oo	uu	c	k
 ll	kk	pp	vv	d	l
 rr	qq  ww	e	m
 xx	yy	f	n
 g	h	o
 p	q
 
 */
//	giant list of coords to check, spiraling outwards
#define RadiusOffsetCount	43
const float2 RadiusOffsets[RadiusOffsetCount] =
{
	//0				a				b				c				d 			e 			f	 		g
	(float2)(0,0),	(float2)(1,0),	(float2)(0,1),	(float2)(1,1),	(float2)(2,0),	(float2)(2,1),	float2(0,2),	float2(1,2),
	//h			i			j			k			l
	(float2)(3,0),	(float2)(3,1),	(float2)(2,2),	(float2)(1,3),	(float2)(0,3),
	//m			n			o			p			q			r
	(float2)(4,0),	(float2)(4,1),	(float2)(3,2),	(float2)(2,3),	(float2)(1,4),	(float2)(0,4),
	//s			t			u			v			w			x			y
	(float2)(5,0),	(float2)(5,1),	(float2)(4,2),	(float2)(3,3),	(float2)(2,4),	(float2)(0,5),	float2(1,5),
	
	//	a		b			c			d			e			f			g			h
	(float2)(6,0),	(float2)(6,1),	(float2)(5,2),	(float2)(4,3),	(float2)(3,4),	(float2)(4,5),	float2(1,6),	float2(0,6),
	
	//	i		j			k			l			m			n			o			p			q
	(float2)(7,0),	(float2)(7,1),	(float2)(6,2),	(float2)(5,3),	(float2)(4,4),	(float2)(3,5),	float2(2,6),	float2(1,7),	float2(0,7),
};
#define MaxRadiusDistance	length((float2)(7,1))	//	if a const, dx sees 0

//	to stop noisy blobs, check at waves
#define RadiusOffsetWaveStartCount	7
const int RadiusOffsetWaveStarts[RadiusOffsetWaveStartCount] =
{
	7,	//	gg
	12,	//	ll
	18,	//	rr
	25,	//	yy
	33,	//	h
	42,	//	q
	9999,
};

static float2 GetRadiusOffset(int Index)
{
	return RadiusOffsets[Index];
}

static float GetRadius(__read_only image2d_t Image,float2 BaseCoord)
{
	float4 Sample = texture2D( Image, BaseCoord );
	bool BaseValid = (Sample.w > 0.5f);
	if ( !BaseValid )
		return 0;
	
	//	find largest match
	TTestOutput Output;
	//Output.BigDistance = 50;
	//Output.HitCount = 0;
	//Output.TotalTests = 0;
	Output.x = 0;	//	gr: this was initialised to 10?
	Output.y = 0;
	Output.z = 0;
	
	//	do an inner test to find solid blobs
	float Scale = MaxRadiusPx / MaxRadiusDistance;
	
	int WaveCount = 0;
	for ( int c=0;	c<RadiusOffsetCount;	c++ )
	{
		float2 radoffset = GetRadiusOffset( c );
		Output = DoTest( Image, BaseCoord, radoffset*float2( 1, 1 ) * Scale, Output );
		Output = DoTest( Image, BaseCoord, radoffset*float2(-1, 1 ) * Scale, Output );
		Output = DoTest( Image, BaseCoord, radoffset*float2( 1,-1 ) * Scale, Output );
		Output = DoTest( Image, BaseCoord, radoffset*float2(-1,-1 ) * Scale, Output );
		
		
		//	break out at points if a quality isn't kept
		if ( c == RadiusOffsetWaveStarts[WaveCount] )
		{
			float HitCountPercent = (float)Output.y / (float)Output.z;
			if ( HitCountPercent < MinInnerHitCountPercent )
			{
				//	return 0;
				return Output.x;
			}
			WaveCount++;
		}
	}
	
	return Output.x;
}

__kernel void CircleFilter(int OffsetX,int OffsetY,__read_only image2d_t grassfilled,__write_only image2d_t Frag)
{
	float tx = get_global_id(0) + OffsetX;
	float ty = get_global_id(1) + OffsetY;
	int2 wh = get_image_dim(Frag);
	float w = wh.x;
	float h = wh.y;
	float2 TexCoord = (float2)( tx, ty );
	
	float Radius = GetRadius( grassfilled, TexCoord );
	float RadiusScore = Radius / MaxRadiusPx;
	
	float4 Output = (float4)( RadiusScore, RadiusScore, RadiusScore, 1 );
	if ( RadiusScore > 1 )
	{
		Output = (float4)(1,0,0,1);
	}
	
	write_imagef( Frag, (int2)(TexCoord.x,TexCoord.y), Output );
}


__kernel void CylinderFilter(int OffsetX,int OffsetY,__read_only image2d_t grassfilled,__write_only image2d_t Frag)
{
	float tx = get_global_id(0) + OffsetX;
	float ty = get_global_id(1) + OffsetY;
	int2 wh = get_image_dim(Frag);
	float w = wh.x;
	float h = wh.y;
	float2 TexCoord = (float2)( tx, ty );

	int2 SamplesUp = (int2)(6,10);
	int2 SamplesDown = (int2)(4,4);
	float2 RectUpMin = (float2)( -6,-30);
	float2 RectUpMax = (float2)(  6, 0);
	float2 RectDownMin = (float2)( -3,1);
	float2 RectDownMax = (float2)( 3,4);
	
	int PositiveTestCount = 0;
	int PositiveCount = 0;
	
	for ( int y=0;	y<=SamplesUp.y;	y++ )
	{
		for ( int x=0;	x<=SamplesUp.x;	x++ )
		{
			float2 xyNorm = (float2)( Range( x, 0, SamplesUp.x ), Range( y, 0, SamplesUp.y ) );
			float2 Offset = (float2)( Lerp( xyNorm.x, RectUpMin.x, RectUpMax.x ), Lerp( xyNorm.y, RectUpMin.y, RectUpMax.y ) );
			PositiveCount += GetHit( grassfilled, TexCoord + Offset );
			PositiveTestCount++;
		}
	}

	
//	int NegativeTestCount = 1;
//	int NegativeCount = GetHit( grassfilled, TexCoord + (float2)(0,1) );
	
	int NegativeTestCount = 0;
	int NegativeCount = 0;
	for ( int y=0;	y<=SamplesDown.y;	y++ )
	{
		for ( int x=0;	x<=SamplesDown.x;	x++ )
		{
			float2 xyNorm = (float2)( Range( x, 0, SamplesDown.x ), Range( y, 0, SamplesDown.y ) );
			float2 Offset = (float2)( Lerp( xyNorm.x, RectDownMin.x, RectDownMax.x ), Lerp( xyNorm.y, RectDownMin.y, RectDownMax.y ) );
			NegativeCount += GetHit( grassfilled, TexCoord + Offset ) ? 0 : 1;
			NegativeTestCount++;
		}
	}

	
	float NegativeScore = (float)NegativeCount / (float)NegativeTestCount;
	float PositiveScore = (float)PositiveCount / (float)PositiveTestCount;
	
	float NegativeMin = 0.9f;
	float PositiveMin = 0.9f;
	
	float4 Output;
	bool HitUp = (PositiveScore >= PositiveMin);
	bool HitDown = (NegativeScore >= NegativeMin);
	if ( HitUp && HitDown )
	{
		Output = (float4)(1,1,1,1);
	}
	else
	{
		Output = (float4)(0,0,0,1);
	}
	
	/*
	float2 ScaleUp = (float2)( 4, 2 );
	float2 ScaleDown = (float2)( 4, 2 );
	
	//	look UP for positives
	int PositiveCount = 0;
	int PositiveTestCount = 0;
	int HeightUp = 10;
	int WidthUp = 5;
	for ( int y=-HeightUp;	y<=0;	y++ )
	{
		for ( int x=-WidthUp;	x<=WidthUp;	x++ )
		{
			float2 xyNorm = (float2)( Range( x, -WidthUp, WidthUp ), Range( y, -HeightUp, 0 ) );
			float2 Offset = ScaleUp * xyNorm;
			bool Hit = GetHit( grassfilled, TexCoord + Offset );
			PositiveCount += Hit ? 1 : 0;
			PositiveTestCount++;
		}
	}
	
	//	look DOWN for negatives
	int NegativeCount = 0;
	int NegativeTestCount = 0;
	int HeightDown = 4;
	int WidthDown = 4;
	for ( int y=0;	y<HeightDown;	y++ )
	{
		for ( int x=-WidthDown;	x<=WidthDown;	x++ )
		{
			float2 xyNorm = (float2)( Range( x, -WidthDown, WidthDown ), Range( y, 0, HeightDown ) );
			float2 Offset = ScaleDown * xyNorm;
			bool Hit = GetHit( grassfilled, TexCoord + Offset );
			NegativeCount += Hit ? 0 : 1;
			NegativeTestCount++;
		}
	}
	
	float NegativeScore = (float)NegativeCount / (float)NegativeTestCount;
	float PositiveScore = (float)PositiveCount / (float)PositiveTestCount;

	float NegativeMin = 0.9f;
	float PositiveMin = 0.9f;
	
	float4 Output = (float4)( PositiveScore, NegativeScore, 0, 1 );

	if ( NegativeScore < NegativeMin )
		Output.w = 0;
	//if ( PositiveScore < PositiveMin )
	//	Output.w = 0;
	*/
	write_imagef( Frag, (int2)(TexCoord.x,TexCoord.y), Output );
}
