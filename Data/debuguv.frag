in vec2 fTexCoord;
uniform sampler2D crop;


void main()
{
	vec4 rgba = texture2D(crop,fTexCoord);
	
	if ( rgba.w > 0.5f )
	{
		gl_FragColor.xyz = rgba.www;
		gl_FragColor.w = 1;
	}
	else
	{
		gl_FragColor = vec4( fTexCoord.x, fTexCoord.y, 0, 0.5f );
	}
}
