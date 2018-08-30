in vec2 uv;

uniform sampler2D SdfTexture;
uniform vec4 SdfRect;

void main()
{
	gl_FragColor = float4(0,0,0,0);
	vec2 Sdfuv = uv;
	Sdfuv.y = 1.0-Sdfuv.y;
	Sdfuv.x = mix( SdfRect[0], SdfRect[0]+SdfRect[2], Sdfuv.x );
	Sdfuv.y = mix( SdfRect[1]+SdfRect[3], SdfRect[1], Sdfuv.y );
	float SdfDistance = texture(SdfTexture,Sdfuv).x;
	
	if ( SdfDistance > 0.8 )
		gl_FragColor = float4(1,1,1,1);
}
