in vec2 fTexCoord;
uniform sampler2D Tex0;

uniform float ZoomUv = 1.0f;
uniform int Invert = 0;
uniform float BarrelPower = 0.5f;	//	gr: fix these values so -1...1, not -1...0.5...1
uniform float InverseBarrelPower = 0.5f;	//	gr: fix these values so -1...1, not -1...0.5...1


//	http://stackoverflow.com/questions/21615298/opencv-distort-back
float2 DistortPixel(float2 point,bool DoInvert)
{
	float2 fragCoord;
	fragCoord.x = (point.x+1.0f)/2.0f;
	fragCoord.y = (point.y+1.0f)/2.0f;
	float2 p = fragCoord.xy;//normalized coords with some cheat
	//(assume 1:1 prop)
	float2 m = float2(0.5, 0.5);//center coords
	float2 d = p - m;//vector from center to current fragment
	float r = sqrt(dot(d, d)); // distance of pixel from center
	
	float UseBarrelPower = DoInvert ? InverseBarrelPower : BarrelPower;
	
	float power = ( 2.0 * 3.141592 / (2.0 * sqrt(dot(m, m))) ) * (UseBarrelPower - 0.5);//amount of effect
	float bind;//radius of 1:1 effect
	//bind = (power > 0.0)  ? sqrt(dot(m, m)) : m.y;//stick to corners
	bind = m.y;//stick to corners

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
	uv = DistortPixel( uv, Invert!=0 );
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
