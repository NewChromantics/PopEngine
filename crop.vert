in vec2 TexCoord;
out vec2 fTexCoord;

void main()
{
    gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);
    //	move to view space 0..1 to -1..1
    gl_Position.xy *= vec2(2,2);
    gl_Position.xy -= vec2(1,1);

    fTexCoord = TexCoord;
}
x
