#define const	__constant

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

__kernel void CylinderFilter(int OffsetX,int OffsetY,__read_only image2d_t grassfilled,__write_only image2d_t Frag)
{
	float tx = get_global_id(0) + OffsetX;
	float ty = get_global_id(1) + OffsetY;
	int2 wh = get_image_dim(Frag);
	float w = wh.x;
	float h = wh.y;
	float2 TexCoord = (float2)( tx, ty );

	int2 SamplesUp = (int2)(4,4);
	int2 SamplesDown = (int2)(4,4);
	float2 RectUpMin = (float2)(-2,-4);
	float2 RectUpMax = (float2)( 2, 0);
	
	int PositiveTestCount = 0;
	int PositiveCount = 0;
	
	for ( int y=0;	y<SamplesUp.y;	y++ )
	{
		float2 xyNorm = (float2)( 1, y / SamplesUp.y );
		float2 Offset = (float2)( 0, -4 * xyNorm.y );
		PositiveCount += GetHit( grassfilled, TexCoord + Offset );
		PositiveTestCount++;
	}
	/*
	int PositiveCount = 0;
	int PositiveTestCount = 0;
	float2 RectUpMin = (float2)(-2,-4);
	float2 RectUpMax = (float2)( 2, 0);
	for ( int y=0;	y<SamplesUp.y;	y++ )
	{
		for ( int x=0;	x<SamplesUp.x;	x++ )
		{
			float2 xyNorm = (float2)( x / SamplesUp.x, y / SamplesUp.y );
			float2 SampleOffset;
			SampleOffset.x = Lerp( xyNorm.x, RectUpMin.x, RectUpMax.x );
			SampleOffset.y = Lerp( xyNorm.y, RectUpMin.y, RectUpMax.y );
			PositiveCount += GetHit( grassfilled, TexCoord + SampleOffset );
			PositiveTestCount++;
		}
	}
	*/
	
	int NegativeTestCount = 1;
	int NegativeCount = GetHit( grassfilled, TexCoord + (float2)(0,1) );
	
	float NegativeScore = (float)NegativeCount / (float)NegativeTestCount;
	float PositiveScore = (float)PositiveCount / (float)PositiveTestCount;
	
	float NegativeMin = 0.9f;
	float PositiveMin = 0.9f;
	
	float4 Output;
	bool HitUp = (PositiveScore >= PositiveMin);
	bool HitDown = (NegativeScore >= NegativeMin);
	if ( HitUp && !HitDown )
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
