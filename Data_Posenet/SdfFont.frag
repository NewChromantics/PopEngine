in vec2 uv;

uniform sampler2D SdfTexture;
uniform vec4 SdfRect;

uniform float InnerDistance = 0.9;
uniform float OuterDistance = 0.55;
uniform float NullDistance = 0.29;


float Range(float Min,float Max,float Value)
{
	return (Value-Min) / (Max-Min);
}

float Clamp01(float Value)
{
	return clamp( Value, 0.0, 1.0 );
}

void main()
{
	gl_FragColor = float4(0,0,1,0);
	vec2 Sdfuv = uv;
	
	Sdfuv.x = mix( SdfRect[0], SdfRect[0]+SdfRect[2], Sdfuv.x );
	Sdfuv.y = mix( SdfRect[1], SdfRect[1]+SdfRect[3], Sdfuv.y );
	
	float SdfDistance = texture(SdfTexture,Sdfuv).x;

	//	quick anti-alias
	float AlphaDelta = Range(NullDistance,OuterDistance,SdfDistance);
	float InnerDelta = Range(OuterDistance,InnerDistance,SdfDistance);
	float Alpha = Clamp01( smoothstep( 0.0, 1.0, AlphaDelta ) );
	float Inner = Clamp01( smoothstep( 0.0, 1.0, InnerDelta ) );
	vec3 Colour = vec3(Inner,Inner,Inner);
	gl_FragColor = vec4(Colour,Alpha);
}
