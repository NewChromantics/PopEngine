in vec2 fTexCoord;
uniform sampler2D Tex0;

uniform float ZoomUv = 1.0f;
uniform int Invert = 0;
uniform float BarrelPower = 0.5f;	//	gr: fix these values so -1...1, not -1...0.5...1

#define PIf 3.14159265359f

//	http://stackoverflow.com/questions/21615298/opencv-distort-back
float2 DistortPixel(float2 Inputuv,bool DoInvert)
{
	//	center point around 0,0
	float2 Point = (Inputuv + float2(1,1)) / float2(2,2);
	float2 LensCenter = float2( 0.5f, 0.5f );
	
	
	float2 Delta = Point - LensCenter;
	float2 DeltaNorm = normalize(Delta);
	float Radius = length(Delta);
	float LensCenterLength = length(LensCenter);
	
	float UseBarrelPower = DoInvert ? 0.5f+(0.5f-BarrelPower) : BarrelPower;
	//	-N ... N
	UseBarrelPower = UseBarrelPower - 0.5f;
	
	float power = ( PIf / LensCenterLength ) * (UseBarrelPower);
	
	//	stick to corners
	//float bind = (power > 0.0)  ? LensCenterLength : LensCenter.y;
	float bind = LensCenter.y;

	//	gr: arbirtry scalar, remove this
	float Mult = 10.0f;
	
	//	Weird formulas, don't think fisheye scales right
	float2 uv = Point;
	if ( power > 0.0 )
	{
		//fisheye
		float Scalar = bind / tan( bind * power * Mult );
		uv = DeltaNorm * tan( Radius * power * Mult ) * Scalar;
		uv += LensCenter;
	}
	else if (power < 0.0)
	{
		//	make power positive
		power = -power;
		//antifisheye
		float Scalar = bind / atan( bind * power * Mult );
		uv = DeltaNorm * atan( Radius * power * Mult ) * Scalar;
		uv += LensCenter;
	}
	
	//	if y=tan(x) && x=atan(y)
	//	y = atan(x*mult)
	//	x*mult = tan(y)
	//	x = tan(y)/mult
	
	//	back to uv's
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
