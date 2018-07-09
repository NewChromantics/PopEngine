in vec2 uv;
const float LineWidth = 0.01;

#define UV_ZOOM		0.8
#define ENABLE_BACKGROUND



#if defined(ENABLE_BACKGROUND)
uniform sampler2D	Background;
#endif

#define CORNER_COUNT	100
uniform vec3		CornerAndScores[CORNER_COUNT];


#define endofheader


//	returns score and distance
float2 DistanceToCorner(vec2 Position,int CornerIndex)
{
	float2 Corner2 = CornerAndScores[CornerIndex].xy;
	float CornerScore = CornerAndScores[CornerIndex].z;
	float Distance = length( Position - Corner2 );
	return float2( Distance, CornerScore );
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
	
	
	float NearestDistance = 999;
	float NearestScore = 0;
	for ( int i=0;	i<CORNER_COUNT;	i++)
	{
		float2 DistanceAndScore = DistanceToCorner( FrameUv, i );
		if ( DistanceAndScore.x < LineWidth )
		{
			NearestDistance = min( NearestDistance, DistanceAndScore.x );
			NearestScore = max( NearestScore, DistanceAndScore.y );
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
