in vec2 uv;

uniform float		Value;
const float3		ExactColour = float3(1,1,1);
uniform float		Alpha = 0.6;

float3 NormalToRedGreen(float Normal)
{
	if ( Normal < 0 )
	{
		return float3( 0,1,1 );
	}
	else if ( Normal < 0.5 )
	{
		Normal = Normal / 0.5;
		return float3( 1, Normal, 0 );
	}
	else if ( Normal <= 1 )
	{
		Normal = (Normal-0.5) / 0.5;
		return float3( 1-Normal, 1, 0 );
	}
	else //	>1
	{
		return float3( 0,0,1 );
	}
}


void main()
{
	gl_FragColor = float4(0,0,0,Alpha);
	
	if ( uv.x < Value )
	{
		gl_FragColor.xyz = NormalToRedGreen(Value);
	}
	else if ( uv.x == Value )
	{
		gl_FragColor.xyz = ExactColour;
	}
}
