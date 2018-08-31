in vec2 uv;

uniform sampler2D SdfTexture;
uniform vec4 SdfRect;

const float gamma = 0.5;
const float scale = 128.0;
const float u_gamma = gamma * 1.4142 / scale;

void main()
{
	gl_FragColor = float4(0,0,1,0);
	vec2 Sdfuv = uv;
	
	Sdfuv.x = mix( SdfRect[0], SdfRect[0]+SdfRect[2], Sdfuv.x );
	Sdfuv.y = mix( SdfRect[1], SdfRect[1]+SdfRect[3], Sdfuv.y );
	
	float SdfDistance = texture(SdfTexture,Sdfuv).x;

	const float InnerDistance = 0.8;
	const float OuterDistance = 0.4;

	float InnerAlpha = smoothstep(InnerDistance - u_gamma, InnerDistance + u_gamma, SdfDistance);
	float OuterAlpha = smoothstep(OuterDistance - u_gamma, OuterDistance + u_gamma, SdfDistance);
	
	float Colourf = mix( 0,1,InnerAlpha);
	float Alpha = mix( OuterAlpha,InnerAlpha,InnerAlpha);
	//if ( SdfDistance > 0.5 )
	//	gl_FragColor = float4(0,0,0,alpha);
	//if ( SdfDistance > 0.80 )
		gl_FragColor = float4(Colourf,Colourf,Colourf,Alpha);
}
