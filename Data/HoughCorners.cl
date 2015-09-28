#define const	__constant

#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable

#define DECLARE_DYNAMIC_ARRAY(TYPE)		\
typedef struct							\
{										\
__global TYPE*			mData;		\
volatile __global int*	mOffset;	\
int				mMax;		\
} TArray_ ## TYPE;						\
\
static bool PushArrayGetIndex_ ## TYPE(TArray_ ## TYPE Array,const TYPE* Value,int* pNewIndex)	{		\
int NewIndex = atomic_inc( Array.mOffset );	/*get next unused index*/	\
bool Success = (NewIndex < Array.mMax);		/*out of space*/			\
NewIndex = min( NewIndex, Array.mMax-1 );	/* dont go out of bounds */	\
Array.mData[NewIndex] = *Value;			\
*pNewIndex = NewIndex;				\
return Success;		\
}						\
static bool PushArray_ ## TYPE(TArray_ ## TYPE Array,TYPE* Value)	{	\
int NewIndex = atomic_inc( Array.mOffset );	/*get next unused index*/	\
bool Success = (NewIndex < Array.mMax);		/*out of space*/			\
NewIndex = min( NewIndex, Array.mMax-1 );	/* dont go out of bounds */	\
Array.mData[NewIndex] = *Value;			\
return Success;		\
}						\

DECLARE_DYNAMIC_ARRAY(float4);


//	z is 0 if bad lines
float3 GetRayRayIntersection(float4 RayA,float4 RayB)
{
	float2 closestPointLine1;
	float2 closestPointLine2;
	float2 linePoint1 = RayA.xy;
	float2 lineVec1 = RayA.zw;
	float2 linePoint2 = RayB.xy;
	float2 lineVec2 = RayB.zw;
	
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



__kernel void ExtractHoughCorners(int OffsetHoughLineAIndex,
								  int OffsetHoughLineBIndex,
								  global float8* HoughLines,
								  int HoughLineCount,
								  global float4* HoughCorners
								  )
{
	int HoughLineAIndex = get_global_id(0) + OffsetHoughLineAIndex;
	int HoughLineBIndex = get_global_id(1) + OffsetHoughLineBIndex;
	float8 HoughLineA = HoughLines[HoughLineAIndex];
	float8 HoughLineB = HoughLines[HoughLineBIndex];
	int CornerIndex = (HoughLineAIndex*HoughLineCount) + HoughLineBIndex;
	
	float2 Intersection = 0;
	float Score = 0;
	float w = 0;

	//	same-index will always intersect, (or be parallel?) score zero
	//	and we only need to compare lines once. so B must be >A
	if ( HoughLineAIndex < HoughLineBIndex )
	{
		float3 Intersection3 = GetLineLineInfiniteIntersection( HoughLineA.xyzw, HoughLineB.xyzw );
		Intersection = Intersection3.xy;
	
		//	just for neat output
		Intersection.x = round(Intersection.x);
		Intersection.y = round(Intersection.y);

		//	invalidate score if intersection was bad
		Score = HoughLineA[6] * HoughLineB[6];
		Score *= Intersection3.z;
		
		//	invalidate score if cross is very far away
		float FarCoord = 10000;
		if ( fabsf(Intersection.x) > FarCoord || fabsf(Intersection.y) > FarCoord )
			Score = -1;
	}
	
	HoughCorners[CornerIndex] = (float4)( Intersection, Score, w );
}


