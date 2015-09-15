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


float PosterizeFloat(float f,int Levels)
{
	return round(f*Levels) / (float)Levels;
}

float3 PosterizeFloat3(float3 f,int Levels)
{
	f.x = PosterizeFloat( f.x, Levels );
	f.y = PosterizeFloat( f.y, Levels );
	f.z = PosterizeFloat( f.z, Levels );

	return f;
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

__kernel void FilterColourPatch(int OffsetX,int OffsetY,__read_only image2d_t SegmentedHsl,__read_only image2d_t undistort,__write_only image2d_t Frag)
{
	__read_only image2d_t Hsl = SegmentedHsl;

	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	int2 wh = get_image_dim(Hsl);
	
	//	walk left & right until we hit a HSL edge
	int MaxDistance = 10;
	float MaxHslDiff = 0.045f;
	
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
	int ScorePx = Left+Right;
	
	//float4 Rgba = (float4)( 0, Score, 0, 1 );
	float4 Rgba = texture2D( undistort, uv );

	//	write score in blue and green
	float ScoreLeft = (float)Left / (float)(MaxDistance+MaxDistance);
	float ScoreRight = (float)Right / (float)(MaxDistance+MaxDistance);
	Rgba.y = ScoreLeft;
	Rgba.z = ScoreRight;

	//	write hue in red
	float3 BaseHsl = texture2D( Hsl, uv ).xyz;
	Rgba.x = BaseHsl.x;
 
	
	//	erase if "this colour" has a width more than N
	if ( ScorePx > 10 )
	{
		Rgba.w = 0;
	}
	
	write_imagef( Frag, uv, Rgba );
}




__kernel void FindCentroids(int OffsetX,int OffsetY,__read_only image2d_t grassfilter,__read_only image2d_t Hsl,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	
	int MaxDistance = 10;

	float LeftScore = texture2D( grassfilter, uv ).y;
	float RightScore = texture2D( grassfilter, uv ).z;
	bool Valid = texture2D( grassfilter, uv ).w > 0.5f;
	float4 Rgba = (float4)(1,0,1,1);

	if ( Valid )
	{
		/*
		float LeftPx = LeftScore * (MaxDistance+MaxDistance);
		float RightPx = RightScore * (MaxDistance+MaxDistance);
		
		if ( fabs(LeftPx - RightPx) <= 0 )
		{
			float Hue = texture2D( grassfilter, uv ).x;
			float3 Hsl = (float3)(Hue,1,0.3f);
			Rgba = (float4)(1,1,1,1);
			Rgba.xyz = HslToRgb( Hsl );
		}
		 */
		
		//float Hue = texture2D( grassfilter, uv ).x;
		float3 SourceHsl = texture2D( Hsl, uv ).xyz;
		//float3 SourceHsl = (float3)(Hue,1,0.3f);
		Rgba = (float4)(1,1,1,1);
		Rgba.xyz = HslToRgb( SourceHsl );

	}

	write_imagef( Frag, uv, Rgba );
}

float3 NormaliseHsl(float3 Hsl)
{
	//	block hue a bit
	//Hsl.x = PosterizeFloat( Hsl.x, 5 );
	//Hsl.z = PosterizeFloat( Hsl.z, 5 );
	//Hsl.y = PosterizeFloat( Hsl.y, 5 );
	
	return Hsl;
	
	float NearWhite = 0.9f;
	float NearBlack = 0.2f;
	
	float h = Hsl.x;
	float s = Hsl.y;
	float l = Hsl.z;
	
	//	if saturation is low, let it fall to white/black more easily
	if ( s < 0.1f )
	{
		NearWhite = 0.6f;
		NearBlack = 0.4f;
	}
	
	//	look for extremes so we filter black and white
	if ( l  >= NearWhite )
	{
		s = 0;
		l = 1;
	}
	else if ( l <= NearBlack )
	{
		s = 0;
		l = 0;
	}
	else if ( s <= 0.1f )
	{
		s = 0;
		l = 0.5f;
	}
	else
	{
		s = 1;
		l = 0.5f;
	}
	
	return (float3)(h,s,l);
}


__kernel void GroupHsl(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	
	float3 SourceHsl = texture2D( Hsl, uv ).xyz;

	
	float MatchSat = 0.4f;
	float MatchSatHigh = 0.5f;
	float MatchSatLow = 0.2f;
	float MatchLum = 0.7f;
	float MatchLumHigh = 0.8f;
#define MatchHslsCount 11
	float3 MatchHsls[MatchHslsCount] =
	{
		(float3)( 0, 0, 0.1f ),	//	black
		(float3)( 0, 0, 0.9f ),	//	white
		(float3)( 0/360.f, MatchLum, MatchSat ),
		(float3)( 20/360.f, MatchLum, MatchSat ),
		(float3)( 50/360.f, MatchLum, MatchSat ),
		(float3)( 90/360.f, MatchLum, MatchSat ),
		(float3)( 150/360.f, MatchLumHigh, MatchSat ),
		(float3)( 180/360.f, MatchLumHigh, MatchSatHigh ),
		(float3)( 190/360.f, MatchLumHigh, MatchSat ),
		(float3)( 205/360.f, MatchLum, MatchSat ),
		(float3)( 290/360.f, MatchLum, MatchSat ),
	};
	
	int Best = 0;
	float BestDiff = 1;
	for ( int i=0;	i<MatchHslsCount;	i++ )
	{
		float Diff = GetHslHslDifference( SourceHsl, MatchHsls[i] );
		if ( Diff < BestDiff )
		{
			BestDiff = Diff;
			Best = i;
		}
	}
	
	float3 FragHsl = MatchHsls[Best];
	float4 Rgba = (float4)(1,1,1,1);
	Rgba.xyz = HslToRgb( FragHsl );
	//Rgba.xyz = FragHsl;
	
	//bool SourceValid = texture2D( grassfilter, uv ).w > 0;
	//if ( !SourceValid )
	//	Rgba.xyz = (float3)(0,0,0);
		
//	Rgba.xyz = (HitCount < MinHitCount) ? (float3)(0,0,0) : (float3)(1,1,1);
	
	//	debug show what we're matching
	if ( uv.y < 100 )
	{
		int Index = uv.x / 100;
		if ( Index < MatchHslsCount )
		{
			Rgba.xyz = HslToRgb( MatchHsls[Index] );
		}
	}
	
	write_imagef( Frag, uv, Rgba );
}


__kernel void BlurColourHsl(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );
	
	float3 SourceHsl = NormaliseHsl( texture2D( Hsl, uv ).xyz );
	float3 MeanHsl = (float3)(0,0,0);
	
#define SampleRadius 4
	int HitCount = 0;
	int MinHitCount = 4;
	float HslDiffMax = 0.05f;
	for ( int y=-SampleRadius;	y<=SampleRadius;	y++ )
	{
		for ( int x=-SampleRadius;	x<=SampleRadius;	x++ )
		{
			//	ignore self
			if ( y==0 && x==0 )
				continue;
			
			//bool NeighbourValid = texture2D( grassfilter, uv + (int2)(x,y) ).w > 0;
			//if ( !NeighbourValid )
			//	continue;
			
			float3 NeighbourHsl = NormaliseHsl( texture2D( Hsl, uv + (int2)(x,y) ).xyz );
			
			float NeighbourDiff = GetHslHslDifference( SourceHsl, NeighbourHsl );
			if ( NeighbourDiff > HslDiffMax )
				continue;
			HitCount++;
			MeanHsl += NeighbourHsl;
		}
	}
	
	MeanHsl /= (float)max(1,HitCount);
	//	MeanHsl.x = PosterizeFloat( MeanHsl.x, 5 );
	//	MeanHsl.z = PosterizeFloat( MeanHsl.z, 5 );
	//	MeanHsl.y = PosterizeFloat( MeanHsl.y+0.1f, 5 );
	
	float3 FragHsl = MeanHsl;
	float4 Rgba = (float4)(1,1,1,1);
	//Rgba.xyz = HslToRgb( FragHsl );
	Rgba.xyz = FragHsl;
	
	//bool SourceValid = texture2D( grassfilter, uv ).w > 0;
	//if ( !SourceValid )
	//	Rgba.xyz = (float3)(0,0,0);
	
	//	Rgba.xyz = (HitCount < MinHitCount) ? (float3)(0,0,0) : (float3)(1,1,1);
	
	write_imagef( Frag, uv, Rgba );
}



__kernel void HslSegmentation(int OffsetX,int OffsetY,__read_only image2d_t Hsl,__write_only image2d_t Frag)
{
	int2 uv = (int2)( get_global_id(0) + OffsetX, get_global_id(1) + OffsetY );

	float4 Rgba = (float4)(0,0,0,0);
	
	float3 SourceHsl = texture2D( Hsl, uv ).xyz;
	//float3 FragHsl = (float3)(Hue,1,0.3f);
	float3 FragHsl = SourceHsl;
	
	
#define SEG_NEAR_WHITE	0.9f
#define SEG_NEAR_BLACK	0.2f
#define SEG_NEAR_GREY	0.1f
	
	float NearWhite = 0.9f;
	float NearBlack = 0.2f;
	
	float h = FragHsl.x;
	float s = FragHsl.y;
	float l = FragHsl.z;
	
	//	if saturation is low, let it fall to white/black more easily
	if ( s < 0.2f )
	{
		NearWhite = 0.5f;
		NearBlack = 0.4f;
	}
	
	//	look for extremes so we filter black and white
	if ( l  >= NearWhite )
	{
		FragHsl.y = 0;
		FragHsl.z = 1;
	}
	else if ( l <= NearBlack )
	{
		FragHsl.y = 0;
		FragHsl.z = 0;
	}
	else if ( s <= SEG_NEAR_GREY )
	{
		FragHsl.y = 0;
		FragHsl.z = 0.5f;
	}
	else
	{
		FragHsl.y = 1;
		FragHsl.z = 0.5f;
	}
	
	Rgba = (float4)(1,1,1,1);
	Rgba.xyz = HslToRgb( FragHsl );
	
	write_imagef( Frag, uv, Rgba );
}

