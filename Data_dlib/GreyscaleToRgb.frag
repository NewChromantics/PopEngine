in vec2 uv;

uniform sampler2D	Source;

void main()
{
	gl_FragColor = texture( Source, uv );
	gl_FragColor.xyz = gl_FragColor.xxx;
}
