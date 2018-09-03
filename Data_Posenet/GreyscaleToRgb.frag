in vec2 uv;

uniform sampler2D	Source;
uniform vec4		ClipRect;
uniform bool		ApplyBlur;
uniform bool		OutputGreyscale;
uniform int			SourceFormat;

#define FRAME_FORMAT_INVALID	0
#define FRAME_FORMAT_GREYSCALE	1
#define FRAME_FORMAT_ARGB		2
#define FRAME_FORMAT_RGBA		3

float normpdf(in float x, in float sigma)
{
	return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
}

vec3 GetSample(vec2 uv)
{
	if ( SourceFormat == FRAME_FORMAT_GREYSCALE )
		return texture( Source, uv ).xxx;
	
	if ( SourceFormat == FRAME_FORMAT_ARGB )
		return texture( Source, uv ).yzw;
	
	if ( SourceFormat == FRAME_FORMAT_RGBA )
		return texture( Source, uv ).xyz;
	
	return vec3(uv,0);
}

vec3 GetBlurredSample(sampler2D iChannel0, vec2 fragCoord)
{
	const vec2 iResolution = vec2(1000,1000);
	fragCoord *= iResolution;
	
	//declare stuff
	const int mSize = 11;
	const int kSize = (mSize-1)/2;
	float kernel[mSize];
	vec3 final_colour = vec3(0.0);
	
	//create the 1-D kernel
	float sigma = 7.0;
	float Z = 0.0;
	for (int j = 0; j <= kSize; ++j)
	{
		kernel[kSize+j] = kernel[kSize-j] = normpdf(float(j), sigma);
	}
	
	//get the normalization factor (as the gaussian has been clamped)
	for (int j = 0; j < mSize; ++j)
	{
		Z += kernel[j];
	}
	
	
	//read out the texels
	for (int i=-kSize; i <= kSize; ++i)
	{
		for (int j=-kSize; j <= kSize; ++j)
		{
			vec3 Sample = GetSample( (fragCoord.xy+vec2(float(i),float(j))) / iResolution.xy);
			final_colour += kernel[kSize+j] * kernel[kSize+i] * Sample;
		}
	}
	
	
	return final_colour/(Z*Z);
}

float Range(float Min,float Max,float Value)
{
	return (Value-Min) / (Max-Min);
}


void main()
{
	//	our camera data is upside down
	//	and we render VertShaderSource upside down for opengl geometry
	//	so uv=0,0 is top left
	//	and when we send the data to posenet its upside down again
	float2 Sampleuv = uv;
	Sampleuv.y = 1-Sampleuv.y;
	Sampleuv.x = mix( ClipRect.x, ClipRect.x+ClipRect.z, Sampleuv.x );
	Sampleuv.y = mix( ClipRect.y, ClipRect.y+ClipRect.w, Sampleuv.y );
	
	if ( ApplyBlur )
	{
		gl_FragColor.xyz = GetBlurredSample( Source, Sampleuv );
	}
	else
	{
		gl_FragColor.xyz = GetSample( Sampleuv );
	}
	gl_FragColor.w = 1;
	
	//	convert any greyscale (one red channel) to full greyscale
	if ( OutputGreyscale )
		gl_FragColor.xyz = gl_FragColor.xxx;
	//else
	//	gl_FragColor.xyz = gl_FragColor.zyx;
}
