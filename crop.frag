in vec2 fTexCoord;
uniform sampler2D Frame;
uniform vec2 MaskTopLeft;
uniform vec2 MaskTopRight;
uniform vec2 MaskBottomRight;
uniform vec2 MaskBottomLeft;


float sign (vec2 p1, vec2 p2, vec2 p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool PointInTriangle (vec2 pt, vec2 v1, vec2 v2, vec2 v3)
{
    bool b1, b2, b3;

    b1 = sign(pt, v1, v2) < 0.0f;
    b2 = sign(pt, v2, v3) < 0.0f;
    b3 = sign(pt, v3, v1) < 0.0f;

    return ((b1 == b2) && (b2 == b3));
}

void main()
{
    gl_FragColor = texture2D(Frame,fTexCoord);

    bool InsideMask = false;
	if ( PointInTriangle( fTexCoord, MaskTopLeft, MaskTopRight, MaskBottomRight ) )
		InsideMask = true;
	if ( PointInTriangle( fTexCoord, MaskBottomRight, MaskBottomLeft, MaskTopLeft ) )
		InsideMask = true;

    if ( !InsideMask )
    {
        gl_FragColor = vec4(1,0,0,0);
    }

}
