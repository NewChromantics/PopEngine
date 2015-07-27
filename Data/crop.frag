in vec2 fTexCoord;
uniform sampler2D undistort;
#define Frame undistort
uniform vec2 MaskTopLeft = vec2(0,0);
uniform vec2 MaskTopRight = vec2(0,1);
uniform vec2 MaskBottomRight = vec2(1,1);
uniform vec2 MaskBottomLeft = vec2(0,1);

const bool Zoom = false;

float max(float a,float b,float c,float d)
{
	return max( a, max( b, max(c,d) ) );
}

float min(float a,float b,float c,float d)
{
	return min( a, min( b, min(c,d) ) );
}


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

float Range(float Value,float Start,float End)
{
	return (Value-Start) / (End-Start);
}

#define lerp(value,start,end)	mix(start,end,value)

float2 GetMaskMin()
{
	float minx = min( MaskTopLeft.x, MaskTopRight.x, MaskBottomRight.x, MaskBottomLeft.x );
	float miny = min( MaskTopLeft.y, MaskTopRight.y, MaskBottomRight.y, MaskBottomLeft.y );
	return float2(minx,miny);
}

float2 GetMaskMax()
{
	float maxx = max( MaskTopLeft.x, MaskTopRight.x, MaskBottomRight.x, MaskBottomLeft.x );
	float maxy = max( MaskTopLeft.y, MaskTopRight.y, MaskBottomRight.y, MaskBottomLeft.y );
	return float2(maxx,maxy);
}

float2 ZoomTexCoord(float2 TexCoord)
{
	if ( !Zoom )
		return TexCoord;
	
	float2 MaskMin = GetMaskMin();
	float2 MaskMax = GetMaskMax();
	
	float2 zoom = TexCoord;
	zoom.y = lerp( TexCoord.x, MaskMin.x, MaskMax.x );
	zoom.y = lerp( TexCoord.y, MaskMin.y, MaskMax.y );

	return zoom;
}

void main()
{
	float2 zTexCoord = ZoomTexCoord(fTexCoord);
    gl_FragColor = texture2D(Frame, zTexCoord );
/*
	float2 MaskMin = GetMaskMin();
	float2 MaskMax = GetMaskMax();
	if ( zTexCoord.x < MaskMin.x )	gl_FragColor = vec4(1,0,0,1);
	if ( zTexCoord.x > MaskMax.x )	gl_FragColor = vec4(0,1,0,1);
	if ( zTexCoord.y < MaskMin.y )	gl_FragColor = vec4(0,0,1,1);
	if ( zTexCoord.y > MaskMax.y )	gl_FragColor = vec4(1,1,0,1);
*/
    bool InsideMask = false;
	//	if ( PointInTriangle( zTexCoord, ZoomTexCoord(MaskTopLeft), ZoomTexCoord(MaskTopRight), ZoomTexCoord(MaskBottomRight) ) )
	if ( PointInTriangle( zTexCoord, MaskTopLeft, MaskTopRight, MaskBottomRight ) )
		InsideMask = true;
//	if ( PointInTriangle( zTexCoord, ZoomTexCoord(MaskBottomRight), ZoomTexCoord(MaskBottomLeft), ZoomTexCoord(MaskTopLeft) ) )
	if ( PointInTriangle( zTexCoord, MaskBottomRight, MaskBottomLeft, MaskTopLeft ) )
		InsideMask = true;

    if ( !InsideMask )
    {
        gl_FragColor = vec4(1,0,1,0);
    }

}
