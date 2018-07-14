in vec2 uv;
const float LineWidth = 0.004;

#define UV_ZOOM		1.0


#define COLOUR_TO_SCORES
//#define COLOUR_TO_ANGLES

uniform sampler2D	Background;

#define LINE_COUNT	200
uniform vec4		Lines[LINE_COUNT];
uniform float		LineScores[LINE_COUNT];
uniform float		LineAngles[LINE_COUNT];
uniform bool		ShowIndexes;

#define endofheader

float TimeAlongLine2(vec2 Position,vec2 Start,vec2 End)
{
	vec2 Direction = End - Start;
	float DirectionLength = length(Direction);
	float Projection = dot( Position - Start, Direction) / (DirectionLength*DirectionLength);
	
	return Projection;
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

float3 GetAngleColour(float Angle)
{
	//	0 and 180 need to be kinda the same colour
	float AngleNorm;
	if ( Angle <= 90 )
		AngleNorm = Angle / 90.0f;
	else
		AngleNorm = 1 - ((Angle-90.0f) / 90.0f);

	return NormalToRedGreen(AngleNorm);
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
	float LineAngle = 0;
	float LineIndexNorm = 0;
	for ( int i=0;	i<LINE_COUNT;	i++)
	{
		vec4 Line = Lines[i];
		Distances[i] = DistanceToLine2( FrameUv, Line.xy, Line.zw );
		if ( Distances[i] < NearestDistance )
		{
			LineIndexNorm = i / float(LINE_COUNT);
			LineScore = LineScores[i];
			LineAngle = LineAngles[i];
			NearestDistance = min( NearestDistance, Distances[i] );
		}
	}

	if ( NearestDistance <= LineWidth )
	{
#if defined(COLOUR_TO_SCORES)
		float3 LineColour = NormalToRedGreen(LineScore);
#elif defined(COLOUR_TO_ANGLES)
		float3 LineColour = GetAngleColour(LineAngle);
#else
		#error no colour mode defined
#endif
		
		if ( ShowIndexes )
		{
			LineColour = NormalToRedGreen(LineIndexNorm);
		}
		
		gl_FragColor = float4( LineColour,1);
	}
	else
	{
		gl_FragColor = texture( Background, FrameUv );
		if ( FrameUv.x < 0 || FrameUv.x > 1 || FrameUv.y < 0 || FrameUv.y > 1 )
			gl_FragColor = vec4(0,0,1,1);
	}
}
