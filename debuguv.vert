attribute vec2 TexCoord;
varying vec2 oTexCoord;

void main()
{
    gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);
    //	move to view space 0..1 to -1..1
    gl_Position.xy *= vec2(2,2);
    gl_Position.xy -= vec2(1,1);
    oTexCoord = vec2(TexCoord.x,1-TexCoord.y);
}
