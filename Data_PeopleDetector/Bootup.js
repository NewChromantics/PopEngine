let Vert_Source =
`
	#version 410
	uniform vec4 VertexRect = vec4(0,0,1,1);
	in vec2 TexCoord;
	out vec2 uv;
	void main()
	{
		gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);
		
		float l = VertexRect[0];
		float t = VertexRect[1];
		float r = l+VertexRect[2];
		float b = t+VertexRect[3];
		
		l = mix( -1, 1, l );
		r = mix( -1, 1, r );
		t = mix( 1, -1, t );
		b = mix( 1, -1, b );
		
		gl_Position.x = mix( l, r, TexCoord.x );
		gl_Position.y = mix( t, b, TexCoord.y );
		
		uv = vec2( TexCoord.x, TexCoord.y );
	}
`;

let Frag_Debug_Source =
`
in vec2 uv;
uniform sampler2D Image;
const int RectCount = 30;
uniform float4 Rects[RectCount];
uniform float RectScores[RectCount];


float3 NormalToRedGreen(float Normal)
{
	if ( Normal < 0.5 )
	{
		Normal = Normal / 0.5;
		return float3( 1, Normal, 0 );
	}
	else if ( Normal <= 1 )
	{
		Normal = (Normal-0.5) / 0.5;
		return float3( 1-Normal, 1, 0 );
	}
	
	//	>1
	return float3( 0,0,1 );
}


bool InsideRect(float2 uv,float4 Rect)
{
	Rect.z += Rect.x;
	Rect.w += Rect.y;
	if ( uv.x >= Rect.x && uv.y >= Rect.y && uv.x <= Rect.z && uv.y <= Rect.w )
		return true;
	return false;
}

float3 BlendColour(float3 a,float3 b,float Alpha)
{
	float3 rgb;
	rgb += a * (1.0-Alpha);
	rgb += b.xyz * (Alpha);
	return rgb;
}

void main()
{
	float4 Sample = texture( Image, uv );
	gl_FragColor = float4(Sample.xyz,1);
	gl_FragColor.yz = gl_FragColor.xx;
	
	float Overlap = 0;
	for ( int RectIndex=0;	RectIndex<RectCount;	RectIndex++ )
	{
		if ( !InsideRect( uv, Rects[RectIndex] ) )
			continue;

		float BlendAlpha = 0.5 * RectScores[RectIndex];
		float3 ScoreRgb = NormalToRedGreen( RectScores[RectIndex] );
		gl_FragColor.xyz = BlendColour( gl_FragColor.xyz, ScoreRgb, BlendAlpha );
	}
}
`;


var FrameRects = [[0,0,0.1,0.1]];
var FrameRectScores = [0];
var FrameImage = null;
var FrameShader = null;
function RenderWindow(RenderTarget)
{
	if ( !FrameShader )
	{
		FrameShader = new OpenglShader( RenderTarget, Vert_Source, Frag_Debug_Source );
	}
	
	let SetUniforms = function(Shader)
	{
		Shader.SetUniform("Image", FrameImage, 0 );
		Shader.SetUniform("Rects", FrameRects );
		Shader.SetUniform("RectScores", FrameRectScores );
	}
	
	RenderTarget.DrawQuad( FrameShader, SetUniforms );
	
}

//	startup
let Window1 = new OpenglWindow("meow learning",true);
Window1.OnRender = function(){	RenderWindow( Window1 );	};
Window1.OnMouseMove = function(){};

FrameImage = new Image("1cats.png");
FrameImage = new Image("6cats.jpg");
FrameImage = new Image("Motd_baseline.png");
FrameImage = new Image("Motd_baseline_big.png");
FrameImage.Resize(416,416);

async function RunDetection(InputImage)
{
	try
	{
		var PeopleDetector = new CoreMlMobileNet();
		const DetectedPeople = await PeopleDetector.DetectObjects(FrameImage);
		Debug("detected x"+DetectedPeople.length);
		FrameRects = [];
		FrameRectScores = [];
		let PushRect = function(Object)
		{
			if ( Object.Label != "person" || Object.Score < 0.10 )
			{
				Debug("Skipped " + Object.Label + " at " + ((Object.Score*100).toFixed(2)) + "%");
				return;
			}
			Debug(Object.Label + " at " + ((Object.Score*100).toFixed(2)) + "%");
			
			Object.Score /= 0.50;
			Object.Score = Math.min( 1, Object.Score );
			
			let w = 416;
			let h = 416;
			let Rect = [Object.x/w,Object.y/h,Object.w/w,Object.h/h];
			let Score = Object.Score;
			//Debug(Rect);
			//Debug(Score);
			FrameRects.push( Rect );
			FrameRectScores.push( Score );
		}
		DetectedPeople.forEach(PushRect);
	}
	catch(e)
	{
		Debug(">>>> Exception: " + e);
	}
}
RunDetection( FrameImage );
