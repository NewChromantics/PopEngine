in vec2 fTexCoord;
uniform sampler2D grassfilled;
uniform float2 grassfilled_TexelWidthHeight;

uniform float CylinderPixelWidth = 10;	//	Range(1,20)
uniform float CylinderPixelHeight = 19;	//	range(1,40)
uniform float MinHitRate = 0.73;	//	range(0,1)

float2 GetCylinderViewSize()
{
	return float2( CylinderPixelWidth*grassfilled_TexelWidthHeight.x, CylinderPixelHeight*grassfilled_TexelWidthHeight.y );
}

//	gr: modifications;
//	(float)x <-- cast doesn't work. must be float(x)
//	lastelement, <--- cannot have a trailing , in an initialiser list
//	gr: done this way as in future we'll replace this table (maybe multiple) with a texture full of coords
#define SampleUvCount		(SampleCountX*SampleCountY)
#define SampleCountX		11
#define SampleCountY		10
#define SampleStepX			(1.0f/float(SampleCountX))
#define SampleStepY			(1.0f/float(SampleCountY))
#define SampleStep2(x,y)	float2( x*SampleStepX, y*SampleStepY )
#define ss(x,y)				SampleStep2(x,y)
#define SampleRow(y)		ss(-5,y), ss(-4,y), ss(-3,y), ss(-2,y), ss(-1,y), ss(0,y), ss(1,y), ss(2,y), ss(3,y), ss(4,y), ss(5,y)

uniform float2 SampleUvs[SampleUvCount] = float2[](
	SampleRow(-9),
	SampleRow(-8),
	SampleRow(-7),
	SampleRow(-6),
	SampleRow(-5),
	SampleRow(-4),
	SampleRow(-3),
	SampleRow(-2),
	SampleRow(-1),
	SampleRow(0)
);

//	todo: match colour of base sample
int GetHit(float2 BaseUv,float2 ViewSize,int Index)
{
	/*
	BaseUv.y -= 6 * grassfilled_TexelWidthHeight.y;
	BaseUv.x += 0 * grassfilled_TexelWidthHeight.x;
	*/
	//	calc uv
	//	gr: -1 y as textures are upside down
	float2 SampleOffset = ViewSize*float2(1,-1)*SampleUvs[Index];
	float2 uv = BaseUv + SampleOffset;
	float4 Sample = texture2D( grassfilled, uv );
	return (Sample.w > 0.5f) ? 1 : 0;
}

void main()
{
	//	get this point in world space
	//	look N metres up and get a 2D height
	//	use aspect ratio to get a 2D width
	//	todo: skew for when view is not orthagonal or billboard and make a test mask?

	//	gr: first version, in view space
	float2 ViewSize = GetCylinderViewSize();
	int HitCount = 0;
	for ( int i=0;	i<SampleUvCount;	i++ )
	{
		HitCount += GetHit( fTexCoord, ViewSize, i );
	}


	float HitRate = float(HitCount) / float(SampleUvCount);

	float Success = (HitRate >= MinHitRate ) ? 1 : 0;
	gl_FragColor = float4( HitRate, fTexCoord.y, 0, Success );
}
