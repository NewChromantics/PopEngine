varying vec2 oTexCoord;
uniform sampler2D Texture0;

void main()
{
    gl_FragColor = vec4(oTexCoord.x,oTexCoord.y,0,1);
    //gl_FragColor = texture2D(Texture0,oTexCoord);
}
