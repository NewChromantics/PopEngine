#define const	__constant

#define DRAW_LINE_WIDTH			2




static float4 ClipLine(float4 Line,float4 Rect)
{
	//	LiangBarsky
	float edgeLeft = Rect.x;
	float edgeRight = Rect.z;
	float edgeBottom = Rect.w;
	float edgeTop = Rect.y;
	
	float x0src = Line.x;
	float y0src = Line.y;
	float x1src = Line.z;
	float y1src = Line.w;
	float t0 = 0.0;
	float t1 = 1.0;
	float xdelta = x1src-x0src;
	float ydelta = y1src-y0src;
	
	float2 pq[4] =
	{
		(float2)(	-xdelta,	-(edgeLeft-x0src)	),
		(float2)(	xdelta,		(edgeRight-x0src)	),
		(float2)(	ydelta,		(edgeBottom-y0src)	),
		(float2)(	-ydelta,	- (edgeTop-y0src)	)
	 };
	
	for(int edge=0; edge<4; edge++)
	{
		float p = pq[edge].x;
		float q = pq[edge].y;
		float r = q/p;
		
		//	parallel line outside
		if ( p==0 && q<0 )
			return 0;

		if(p<0)
		{
			if(r>t1)
				return 0;

			t0 = max(t0,r);
		}
		else
		{
			if(r<t0)
				return 0;
			
			t1 = min( t1,r );
		}
	}
	
	float4 ClippedLine;
	
	ClippedLine.xy = Line.xy + t0 * (float2)(xdelta,ydelta);
	ClippedLine.zw = Line.xy + t1 * (float2)(xdelta,ydelta);
	
	return ClippedLine;
}



//	z is 0 if bad lines
static float3 GetRayRayIntersection(float4 RayA,float4 RayB)
{
	float Ax = RayA.x;
	float Ay = RayA.y;
	float Bx = RayA.x + RayA.z;
	float By = RayA.y + RayA.w;
	float Cx = RayB.x;
	float Cy = RayB.y;
	float Dx = RayB.x + RayB.z;
	float Dy = RayB.y + RayB.w;
	
	
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
	float Score = (Cy==Dy) ? 0 : 1;
	
	//  (3) Discover the position of the intersection point along line A-B.
	ABpos=Dx+(Cx-Dx)*Dy/(Dy-Cy);
	
	//  (4) Apply the discovered position to line A-B in the original coordinate system.
	float2 Intersection;
	Intersection.x = Ax + ABpos * theCos;
	Intersection.y = Ay + ABpos * theSin;
	return (float3)(Intersection.x,Intersection.y,Score);
}

static float4 LineToRay(float4 Line)
{
	return (float4)( Line.x, Line.y, Line.z-Line.x, Line.w-Line.y );
}

//	z is 0 if bad lines
static float3 GetLineLineInfiniteIntersection(float4 LineA,float4 LineB)
{
	float4 RayA = LineToRay(LineA);
	float4 RayB = LineToRay(LineB);
	return GetRayRayIntersection( RayA, RayB );
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



static void DrawLineDirect(float2 From,float2 To,__write_only image2d_t Frag,float Score)
{
	float4 Rgba = NormalToRgba( Score );
	int2 wh = get_image_dim(Frag);
	
	float4 Line4;
	Line4.xy = From;
	Line4.zw = To;
	float Border = 0;
	Line4 = ClipLine( Line4, (float4)(Border,Border,wh.x-Border,wh.y-Border) );
	From = Line4.xy;
	To = Line4.zw;
	
	int Steps = 900;
	for ( int i=0;	i<Steps;	i++ )
	{
		float2 Point = Lerp2( i/(float)Steps, From, To );
		
		int Rad = DRAW_LINE_WIDTH-1;

		//	gr: not needed with clipping, but doesnt seem to impact performance (whilst sync is > 30ms)
		if ( Point.x < Rad || Point.y < Rad || Point.x >= wh.x-Rad || Point.y >= wh.y-Rad )
			continue;

		if ( DRAW_LINE_WIDTH>1 )
		{
			//	thicken line
			write_imagef( Frag, (int2)(Point.x-Rad,	Point.y-Rad), Rgba );
			write_imagef( Frag, (int2)(Point.x-0,	Point.y-Rad), Rgba );
			write_imagef( Frag, (int2)(Point.x+Rad,	Point.y-Rad), Rgba );
			write_imagef( Frag, (int2)(Point.x-Rad,	Point.y-0), Rgba );
			write_imagef( Frag, (int2)(Point.x-0,	Point.y-0), Rgba );
			write_imagef( Frag, (int2)(Point.x+Rad,	Point.y-0), Rgba );
			write_imagef( Frag, (int2)(Point.x-Rad,	Point.y+Rad), Rgba );
			write_imagef( Frag, (int2)(Point.x-0,	Point.y+Rad), Rgba );
			write_imagef( Frag, (int2)(Point.x+Rad,	Point.y+Rad), Rgba );
		}
		else
		{
			write_imagef( Frag, (int2)(Point.x,Point.y), Rgba );
		}
	}
}
