in vec2 fTexCoord;
uniform sampler2D crop;


float max(float a,float b,float c)
{
	return max( a, max( b,c ) );
}

float min(float a,float b,float c)
{
	return min( a, min( b,c ) );
}


vec3 RgbToHsl(vec3 rgb)
{
	float r = rgb.x;
	float g = rgb.y;
	float b = rgb.z;

	float Max = max( r, g, b );
	float Min = min( r, g, b );

	float h = 0;
	float s = 0;
	float l = ( Max + Min ) / 2.f;

	if ( Max == Min )
	{
		//	achromatic/grey
        h = s = 0;
    }
	else
	{
        float d = Max - Min;
        s = l > 0.5f ? d / (2 - Max - Min) : d / (Max + Min);
        if ( Max == r )
		{
            h = (g - b) / d + (g < b ? 6 : 0);
		}
		else if ( Max == g )
		{
            h = (b - r) / d + 2;
        }
		else //if ( Max == b )
		{
			h = (r - g) / d + 4;
		}

        h /= 6;
    }

	return vec3( h, s, l );
}


void main()
{
    vec4 rgba = texture2D(crop,fTexCoord);
    gl_FragColor.xyz = RgbToHsl( rgba.xyz );
    gl_FragColor.w = rgba.w;
}
