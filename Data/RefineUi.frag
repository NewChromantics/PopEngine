in vec2 uv;


#define UV_ZOOM		1.0

#define USER_POSITION_COUNT	4

uniform vec2		UserMousePos;
const vec4			UserMouseColour = vec4(1,1,1,1);
const float			UserMouseWidth = UserLineWidth * 2.0;

uniform vec2		UserPositions[USER_POSITION_COUNT];
const vec4			UserColour = vec4(0,1,0,1);
const float			UserLineWidth = 0.004;
uniform vec4x4		UserTransform;

uniform sampler2D	Background;
const vec4			BackgroundColour = (0,0,0);

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


vec4 GetLineLoopColour(vec2f FragUv,vec4 Positions[],int PositionCount,vec4 Colour,float Radius)
{
	float NearestDistance = 999;
	for ( int i=0;	i<PositionCount;	i++)
	{
		vec2 LineA = Positions[i];
		vec2 LineB = Positions[(i+1)%PositionCount];
		float Distance = DistanceToLine2( FragUv, LineA, LineB );
		if ( Distance < NearestDistance )
		{
			NearestDistance = min( NearestDistance, Distance );
		}
	}
	Colour.w *= (NearestDistance < Radius) ? 1.0 : 0.0;
	return Colour;
}

vec3 MixColours(vec3 CanvasColour,vec4 NewColour)
{
	float CanvasStrength = (1-NewColour.a);
	float NewStrength = (NewColour.a);
	vec3 rgb = CanvasColour.xyz * (CanvasStrength);
	rgb += NewColour.xyz * (NewStrength);
	return rgb;
}

void main()
{
	vec2 FrameUv = uv;
	FrameUv -= vec2(0.5,0.5);
	FrameUv /= vec2(UV_ZOOM,UV_ZOOM);
	FrameUv += vec2(0.5,0.5);


	//	get user colour
	vec4 UserColour = GetLineLoopColour( FrameUv, UserPositions, USER_POSITION_COUNT, UserColour, UserLineWidth );

	//	get mouse colour
	vec4 MouseColour = GetCircleColour( FrameUv, UserMousePos, UserMouseColour, UserMouseWidth );

	vec2 BackgroundUv = FrameUv;
	vec4 BackgroundSample = texture( Background, BackgroundUv );
	gl_FragColor = vec4( BackgroundColour, 1 );
	gl_FragColor.xyz = MixColours( gl_FragColor.xyz, BackgroundSample );

	//	merge colours
	gl_FragColor.xyz = MixColours( gl_FragColor.xyz, UserColour );
	gl_FragColor.xyz = MixColours( gl_FragColor.xyz, MouseColour );
}
