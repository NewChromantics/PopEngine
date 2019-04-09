precision highp float;
varying vec2 uv;

uniform sampler2D Texture;
uniform float TextureWidth = 1;
#define LumaTexture	Texture


const float ChromaVRed = 1.5958;
const float ChromaUGreen = -0.39173;
const float ChromaVGreen = -0.81290;
const float ChromaUBlue = 2.017;

uniform bool OutputChromaxy = false;

float3 LumaChromaToRgb(float Luma,float2 Chroma)
{
	//	0..1 to -0.5..0.5
	Luma = mix( 16.0/255.0, 253.0/255.0, Luma );
	Chroma -= 0.5;
	
	float3 Rgb;
	
	Rgb.x = Luma + (ChromaVRed * Chroma.y);
	Rgb.y = Luma + (ChromaUGreen * Chroma.x) + (ChromaVGreen * Chroma.y);
	Rgb.z = Luma + (ChromaUBlue * Chroma.x);
	
	Rgb = max( float3(0,0,0), Rgb );
	Rgb = min( float3(1,1,1), Rgb );
	
	return Rgb;
}



void main()
{
	float TextureStep = 1.0 / TextureWidth;
	//	https://gitlab.com/NewChromantics/PopCameraDevice/blob/master/Unity/PopCameraDevice/Assets/Yuv.shader
	//	data is
	//	LumaX+0, ChromaU+0, LumaX+1, ChromaV+0
	//	2 lumas for each chroma
	float x = mod(uv.x * TextureWidth, 2.0);
	float uRemainder = x * TextureStep;
	
	//	uv0 = left pixel of pair
	float2 uv0 = uv;
	uv0.x -= uRemainder;
	//	uv1 = right pixel of pair
	float2 uv1 = uv0;
	uv1.x += TextureStep;
	
	//	just in case, sample from middle of texel!
	uv0.x += TextureStep * 0.5;
	uv1.x += TextureStep * 0.5;
	
	float ChromaU = texture(LumaTexture, uv0).x;
	float ChromaV = texture(LumaTexture, uv1).x;
	float Luma = texture(LumaTexture, uv).y;
	float2 ChromaUV = float2(ChromaU, ChromaV);

	float3 Rgb = LumaChromaToRgb( Luma, ChromaUV );
	
	//gl_FragColor = float4(Luma,Luma,Luma,1);
	gl_FragColor.xyz = Rgb;
	if ( OutputChromaxy )
		gl_FragColor.xy = ChromaUV;
		
	gl_FragColor.w = 1;
}


