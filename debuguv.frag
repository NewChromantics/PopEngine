in vec2 fTexCoord;
uniform sampler2D Frame;

void main()
{
    gl_FragColor = vec4(fTexCoord.x,fTexCoord.y,0,1);
    //gl_FragColor = texture2D(Frame,fTexCoord);
}
