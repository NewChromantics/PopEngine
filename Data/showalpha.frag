in vec2 fTexCoord;
uniform sampler2D crop;

void main()
{
    vec4 rgba = texture2D(crop,fTexCoord);
	gl_FragColor.xyzw = rgba.wwww;
}
