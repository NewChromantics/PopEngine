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
uniform float BarrelPower = 0.413f;

//	http://stackoverflow.com/questions/21615298/opencv-distort-back
float2 DistortPixel(float2 point,bool Invert)
{
	float2 fragCoord;
	fragCoord.x = (point.x+1.0f)/2.0f;
	fragCoord.y = (point.y+1.0f)/2.0f;
	float2 p = fragCoord.xy;//normalized coords with some cheat
	//(assume 1:1 prop)
	float2 m = float2(0.5, 0.5);//center coords
	float2 d = p - m;//vector from center to current fragment
	float r = sqrt(dot(d, d)); // distance of pixel from center
	
	float power = ( 2.0 * 3.141592 / (2.0 * sqrt(dot(m, m))) ) * (BarrelPower - 0.5);//amount of effect
	float bind;//radius of 1:1 effect
	bind = (power > 0.0)  ? sqrt(dot(m, m)) : m.y;//stick to corners
		
	//Weird formulas
	float2 uv;
	if (power > 0.0)//fisheye
		uv = m + normalize(d) * tan(r * power) * bind / tan( bind * power);
	else if (power < 0.0)//antifisheye
		uv = m + normalize(d) * atan(r * -power * 10.0) * bind / atan(-power * bind * 10.0);
	else
		uv = p;//no effect for power = 1.0
		
	uv.x = (uv.x * 2.0f) - 1.0f;
	uv.y = (uv.y * 2.0f) - 1.0f;
	return float2(uv.x, uv.y );
	
	/*
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
	 */
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
