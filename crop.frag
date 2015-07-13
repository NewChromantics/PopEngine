in vec2 fTexCoord;
uniform sampler2D Frame;
in vec2 MaskTopLeft;
in vec2 MaskTopRight;
in vec2 MaskBottomRight;
in vec2 MaskBottomLeft;

void main()
{
    //gl_FragColor = vec4(fTexCoord.x,fTexCoord.y,0,1);
    gl_FragColor = texture2D(Frame,fTexCoord);
    gl_FragColor.z = 0;
}
