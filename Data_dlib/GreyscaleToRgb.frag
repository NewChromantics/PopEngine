in vec2 uv;

uniform sampler2D	Source;
uniform vec4		ClipRect;


float Range(float Min,float Max,float Value)
{
	return (Value-Min) / (Max-Min);
}


void main()
{
	float2 Sampleuv;
	Sampleuv.x = mix( ClipRect.x, ClipRect.x+ClipRect.z, uv.x );
	Sampleuv.y = mix( ClipRect.y, ClipRect.y+ClipRect.w, uv.y );
	
	gl_FragColor = texture( Source, Sampleuv );
	gl_FragColor.xyz = gl_FragColor.xxx;
}
