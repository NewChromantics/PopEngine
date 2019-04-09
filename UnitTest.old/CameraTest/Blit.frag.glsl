precision highp float;
varying vec2 uv;

uniform sampler2D Texture;
uniform bool Mirror = false;

void main()
{
	float2 Sampleuv = uv;
	if ( Mirror )
		Sampleuv.x = 1.0 - Sampleuv.x;
	
	float4 Sample = texture2D( Texture, Sampleuv );
	gl_FragColor = Sample;
}


