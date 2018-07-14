in vec2 uv;
const float LineWidth = 0.01;

#define UV_ZOOM		1.0



uniform sampler2D	Background;

#define CORNER_COUNT	500
uniform mat4		Transform;
uniform vec2		Corners[CORNER_COUNT];

#define endofheader


//	returns score and distance
float DistanceToCorner(vec2 Position,int CornerIndex)
{
	float2 Corner2 = Corners[CornerIndex];
	float Distance = length( Position - Corner2 );
	return Distance;
}

float3 NormalToRedGreen(float Normal)
{
	if ( Normal < 0.5 )
	{
		Normal = Normal / 0.5;
		return float3( 1, Normal, 0 );
	}
	else if ( Normal <= 1 )
	{
		Normal = (Normal-0.5) / 0.5;
		return float3( 1-Normal, 1, 0 );
	}
	
	//	>1
	return float3( 0,0,1 );
}

void main()
{
	vec2 FrameUv = uv;
	FrameUv -= vec2(0.5,0.5);
	FrameUv /= vec2(UV_ZOOM,UV_ZOOM);
	FrameUv += vec2(0.5,0.5);
	
	vec4 FrameUv4 = Transform * float4( FrameUv, 0, 1 );
	FrameUv = FrameUv4.xy;
	
	float NearestDistance = 999;
	float NearestScore = 999;
	for ( int i=0;	i<CORNER_COUNT;	i++)
	{
		float Distance = DistanceToCorner( FrameUv, i );
		if ( Distance < LineWidth )
		{
			NearestDistance = min( NearestDistance, Distance );
		}
	}

	if ( NearestDistance <= LineWidth )
	{
		gl_FragColor = float4( NormalToRedGreen(NearestScore),1);
	}
	else
	{
		gl_FragColor = texture( Background, FrameUv );
	}
}
