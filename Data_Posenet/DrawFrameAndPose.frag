in vec2 uv;
const float LineWidth = 0.003;

#define FRAME_FORMAT_INVALID	0
#define FRAME_FORMAT_GREYSCALE	1
#define FRAME_FORMAT_ARGB		2
#define FRAME_FORMAT_RGBA		3

uniform sampler2D	Frame;
uniform int			FrameFormat;
uniform vec4		UnClipRect;

//	gr: this is mega slow on intel machines. speed up the distance check code!
#define LINE_COUNT	100
#if defined(LINE_COUNT)
uniform vec4		Lines[LINE_COUNT];
uniform float		LineScores[LINE_COUNT];
#endif
const float FrameRateBarWidth = 0.015;
uniform float		FrameRateNormalised;
uniform float		CameraFrameRateNormalised;
uniform float		ConcurrencyRateNormalised;

float TimeAlongLine2(vec2 Position,vec2 Start,vec2 End)
{
	vec2 Direction = End - Start;
	float DirectionLength = length(Direction);
	float Projection = dot( Position - Start, Direction) / (DirectionLength*DirectionLength);
	
	return Projection;
}

float3 NormalToRedGreen(float Normal)
{
	if ( Normal < 0 )
	{
		return float3( 0,1,1 );
	}
	else if ( Normal < 0.5 )
	{
		Normal = Normal / 0.5;
		return float3( 1, Normal, 0 );
	}
	else if ( Normal <= 1 )
	{
		Normal = (Normal-0.5) / 0.5;
		return float3( 1-Normal, 1, 0 );
	}
	else //	>1
	{
		return float3( 0,0,1 );
	}
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

float Range(float Min,float Max,float Value)
{
	return (Value-Min) / (Max-Min);
}

vec2 Range2(vec2 Min,vec2 Max,vec2 Value)
{
	return vec2( Range(Min.x,Max.x,Value.x), Range(Min.y,Max.y,Value.y) );
}

void main()
{
	//	draw frame rate
	float BarHeights[3];
	BarHeights[0] = CameraFrameRateNormalised;
	BarHeights[1] = FrameRateNormalised;
	BarHeights[2] = ConcurrencyRateNormalised;
	int BarX = int(uv.x / FrameRateBarWidth);
	if ( BarX < 3 )
	{
		float BarHeight = BarHeights[BarX];
		if ( (1-uv.y) <= BarHeight )
		{
			gl_FragColor.xyz = NormalToRedGreen( BarHeight );
			gl_FragColor.w = 1;
			return;
		}
	}

	vec2 FrameUv = uv;
	//FrameUv.xy = Range2( UnClipRect.xy, UnClipRect.xy+UnClipRect.zw, FrameUv.xy );

	float NearestDistance = 999;
	float LineScore = 0;
#if defined(LINE_COUNT)
	float Distances[LINE_COUNT];

	for ( int i=0;	i<LINE_COUNT;	i++)
	{
		vec4 Line = Lines[i];
		Distances[i] = DistanceToLine2( FrameUv, Line.xy, Line.zw );
		if ( Distances[i] < NearestDistance )
		{
			LineScore = LineScores[i];
			NearestDistance = min( NearestDistance, Distances[i] );
		}
	}
#endif
	
	if ( NearestDistance <= LineWidth )
	{
		float3 LineColour = NormalToRedGreen(LineScore);
		gl_FragColor = float4( LineColour,1);
	}
	else if ( FrameFormat != 0 )
	{
		FrameUv.xy = Range2( UnClipRect.xy, UnClipRect.xy+UnClipRect.zw, FrameUv.xy );
		
		//	uploaded as ARGB
		if ( FrameFormat == FRAME_FORMAT_GREYSCALE )
			gl_FragColor = texture( Frame, FrameUv ).xxxw;
		if ( FrameFormat == FRAME_FORMAT_ARGB )
			gl_FragColor = texture( Frame, FrameUv ).yzwx;
		if ( FrameFormat == FRAME_FORMAT_RGBA )
			gl_FragColor = texture( Frame, FrameUv ).xyzw;
	}
	else
	{
		gl_FragColor = float4( FrameUv, 0, 1 );
	}

}
