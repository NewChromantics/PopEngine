in vec2 fTexCoord;
uniform sampler2D crop;


void main()
{
	vec4 rgba = texture2D(crop,fTexCoord);
	gl_FragColor = vec4( fTexCoord.x, fTexCoord.y, 0, 1 );
}
