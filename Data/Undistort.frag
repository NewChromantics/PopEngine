in vec2 fTexCoord;
uniform sampler2D Tex0;

uniform float RadialDistortionX = -.0900000036;
uniform float RadialDistortionY = .00999999978;
uniform float TangentialDistortionX = .0599999987;
uniform float TangentialDistortionY = 0;
uniform float K5Distortion = 0;
uniform float LensOffsetX = .0299999993;
uniform float LensOffsetY = .0500000007;
uniform float ZoomUv = 1.0f;
const bool Invert = false;

//	http://stackoverflow.com/questions/21615298/opencv-distort-back
float2 DistortPixel(float2 point,bool Invert)
{
	float Inverse = Invert?-1:1;
	float cx = LensOffsetX;
	float cy = LensOffsetY;
	float k1 = RadialDistortionX * Inverse;
	float k2 = RadialDistortionY * Inverse;
	float p1 = TangentialDistortionX * Inverse;
	float p2 = TangentialDistortionY * Inverse;
	float k3 = K5Distortion * Inverse;
	
	
	float x = point.x - cx;
	float y = point.y - cy;
	float r2 = x*x + y*y;
	
	// Radial distorsion
	float xDistort = x * (1 + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2);
	float yDistort = y * (1 + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2);
	
	// Tangential distorsion
	xDistort = xDistort + (2 * p1 * x * y + p2 * (r2 + 2 * x * x));
	yDistort = yDistort + (p1 * (r2 + 2 * y * y) + 2 * p2 * x * y);
	
	// Back to absolute coordinates.
	xDistort = xDistort + cx;
	yDistort = yDistort + cy;
	
	return float2( xDistort, yDistort);
}

//	0..1 to -1..1
float2 CenterUv(float2 uv)
{
	//	gr: distort maths is for 0=bottom... so flip here for now
	uv.y = 1 - uv.y;

	uv = uv*float2(2,2) - float2(1,1);
	return uv;
}

float2 UncenterUv(float2 uv)
{
	uv = (uv+float2(1,1)) / float2(2,2);

	//	gr: distort maths is for 0=bottom... so flip here for now
	uv.y = 1 - uv.y;
	return uv;
}




void main()
{
	float2 uv = fTexCoord;
	
	uv = CenterUv(uv);
	uv *= 1.0f / ZoomUv;
	uv = DistortPixel( uv, Invert );
	uv = UncenterUv(uv);
	
	if ( uv.x > 1 )
	{
		gl_FragColor = float4(1,0,0,1);
		return;
	}
	if ( uv.y > 1 )
	{
		gl_FragColor = float4(0,1,0,1);
		return;
	}
	if ( uv.x < 0 )
	{
		gl_FragColor = float4(0,0,1,1);
		return;
	}
	if ( uv.y < 0 )
	{
		gl_FragColor = float4(1,1,0,1);
		return;
	}
	
	uv = CenterUv(uv);
	uv *= 1.0f / ZoomUv;
	uv = UncenterUv(uv);
	
	
	float4 Sample = texture2D( Tex0, uv );

	gl_FragColor = Sample;
}
