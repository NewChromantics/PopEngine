in vec2 uv;
const float LineWidth = 0.001;

#define UV_ZOOM		0.6
//#define ENABLE_SCORES
#define ENABLE_BACKGROUND



#if defined(ENABLE_BACKGROUND)
 uniform sampler2D	Background;
#endif

#define LINE_COUNT	100
uniform vec4		Lines[LINE_COUNT];
#if defined(ENABLE_SCORES)
uniform float		LineScores[LINE_COUNT];
#else
const vec4			LineColour = vec4(1,0,1,1);
#endif


#define endofheader

float TimeAlongLine2(vec2 Position,vec2 Start,vec2 End)
{
	vec2 Direction = End - Start;
	float DirectionLength = length(Direction);
	float Projection = dot( Position - Start, Direction) / (DirectionLength*DirectionLength);
	
	return Projection;
}


vec2 NearestToLine2(vec2 Position,vec2 Start,vec2 End)
{
	float Projection = TimeAlongLine2( Position, Start, End );
	
	//	past start
	Projection = max( 0, Projection );
	//	past end
	Projection = min( 1, Projection );
	
	//	is using lerp faster than
	//	Near = Start + (Direction * Projection);
	float2 Near = mix( Start, End, Projection );
	return Near;
}

float DistanceToLine2(vec2 Position,vec2 Start,vec2 End)
{
	vec2 Near = NearestToLine2( Position, Start, End );
	return length( Near - Position );
}


void main()
{
	vec2 FrameUv = uv;
	FrameUv -= vec2(0.5,0.5);
	FrameUv /= vec2(UV_ZOOM,UV_ZOOM);
	FrameUv += vec2(0.5,0.5);
	
	float Distances[LINE_COUNT];

	float NearestDistance = 999;
	float LineScore = 0;
	for ( int i=0;	i<LINE_COUNT;	i++)
	{
		vec4 Line = Lines[i];
		Distances[i] = DistanceToLine2( FrameUv, Line.xy, Line.zw );
		if ( Distances[i] < NearestDistance )
		{
#if defined(ENABLE_SCORES)
			LineScore = max( LineScore, LineScores[i] );
#endif
			NearestDistance = min( NearestDistance, Distances[i] );
		}
	}

	if ( NearestDistance <= LineWidth )
	{
#if defined(ENABLE_SCORES)
		gl_FragColor = float4( LineScore,LineScore,LineScore,1);
#else
		gl_FragColor = LineColour;
#endif
	}
	else
	{
#if defined(ENABLE_BACKGROUND)
		gl_FragColor = texture( Background, FrameUv );
#else
		gl_FragColor = vec4(0,0,0,1);
#endif
		if ( FrameUv.x < 0 || FrameUv.x > 1 || FrameUv.y < 0 || FrameUv.y > 1 )
			gl_FragColor = vec4(0,0,1,1);
	}
}
