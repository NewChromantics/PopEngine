in vec2 uv;

uniform sampler2D	Source;
uniform vec4		ClipRect;
uniform bool		ApplyBlur;
uniform bool		OutputGreyscale;

float normpdf(in float x, in float sigma)
{
	return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
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
			final_colour += kernel[kSize+j]*kernel[kSize+i]*texture(iChannel0, (fragCoord.xy+vec2(float(i),float(j))) / iResolution.xy).rgb;
			
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
	float2 Sampleuv;
	Sampleuv.x = mix( ClipRect.x, ClipRect.x+ClipRect.z, uv.x );
	Sampleuv.y = mix( ClipRect.y, ClipRect.y+ClipRect.w, uv.y );
	
	if ( ApplyBlur )
	{
		gl_FragColor.xyz = GetBlurredSample( Source, Sampleuv );
	}
	else
	{
		gl_FragColor = texture( Source, Sampleuv );
	}
	gl_FragColor.w = 1;
	
	//	convert any greyscale (one red channel) to full greyscale
	if ( OutputGreyscale )
		gl_FragColor.xyz = gl_FragColor.xxx;
}
