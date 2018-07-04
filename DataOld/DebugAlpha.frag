in vec2 fTexCoord;
uniform sampler2D crop;

void main()
{
    vec4 rgba = texture2D(crop,fTexCoord);
	gl_FragColor.xyz = rgba.www;
	gl_FragColor.w = 1;
}
