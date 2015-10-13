in vec2 fTexCoord;
uniform sampler2D Tex0;

void main()
{
	vec4 Hsla = texture2D(Tex0, fTexCoord );
	
	//if ( Hsla.z < 0.5f )
	if ( Hsla.x < 0.20f || Hsla.x > 0.5f )
		Hsla.w = 0;
	
	gl_FragColor = Hsla;
}
