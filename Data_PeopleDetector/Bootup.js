//	gr: include is not a generic thing (or a wrapper yet) so we can change
//	LoadFileAsString to a file-handle to detect file changes to auto reload things
function include(Filename)
{
	let Source = LoadFileAsString(Filename);
	return CompileAndRun( Source );
}
include("../Data_Holosports/PopDomJs/PopDomJs.js");


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

let Frag_ShowAllRects_Source =
`
in vec2 uv;
uniform sampler2D Image;
const int RectCount = 40;
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


let Frag_SingleRect_Source =
`
in vec2 uv;
uniform sampler2D Image;
uniform float4 Rect;

void main()
{
	float2 SampleUv = mix( Rect.xy, Rect.xy+Rect.zw, uv );
	float4 Sample = texture( Image, SampleUv );
	gl_FragColor = float4(Sample.xyz,1);
}
`;

var FrameRects = [[0,0,0.1,0.1]];
var FrameRectScores = [0];
var FrameImage = null;
var AllRectsShader = null;
var SingleRectShader = null;
function RenderWindow(RenderTarget)
{
	if ( !AllRectsShader )
	{
		AllRectsShader = new OpenglShader( RenderTarget, Vert_Source, Frag_ShowAllRects_Source );
	}
	if ( !SingleRectShader )
	{
		SingleRectShader = new OpenglShader( RenderTarget, Vert_Source, Frag_SingleRect_Source );
	}
	
	
	
	let BoxesWide = Math.max( 1, Math.ceil( Math.sqrt( FrameRects.length + 1 ) ) );
	let BoxesHigh = BoxesWide;
	let BoxRects = [];
	
	let Lerp = function(Min,Max,Value)
	{
		return Min + ( (Max-Min) * Value );
	}
	for ( let bx=0;	bx<BoxesWide;	bx++ )
	{
		for ( let by=0;	by<BoxesHigh;	by++ )
		{
			let bx0 = (bx+0) / BoxesWide;
			let bw = (1) / BoxesWide;
			let by0 = (by+0) / BoxesHigh;
			let bh = (1) / BoxesHigh;
			let Rect = [bx0,by0,bw,bh];
			BoxRects.push( Rect );
		}
	}
	
	let DrawRect = function(Rect,RectIndex)
	{
		if ( RectIndex == 0 )
		{
			let SetUniforms = function(Shader)
			{
				Shader.SetUniform("VertexRect", Rect );
				Shader.SetUniform("Image", FrameImage, 0 );
				Shader.SetUniform("Rects", FrameRects );
				Shader.SetUniform("RectScores", FrameRectScores );
			}
			RenderTarget.DrawQuad( AllRectsShader, SetUniforms );
		}
		else
		{
			RectIndex--;
			if ( RectIndex >= FrameRects.length )
				return;
			let SetUniforms = function(Shader)
			{
				Shader.SetUniform("VertexRect", Rect );
				Shader.SetUniform("Image", FrameImage, 0 );
				Shader.SetUniform("Rect", FrameRects[RectIndex] );
			}
			RenderTarget.DrawQuad( SingleRectShader, SetUniforms );
		}
	}
	BoxRects.forEach( DrawRect );
}

//	startup
let Window1 = new OpenglWindow("meow learning",true);
Window1.OnRender = function(){	RenderWindow( Window1 );	};
Window1.OnMouseMove = function(){};

FrameImage = new Image("1cats.png");
FrameImage = new Image("6cats.jpg");
FrameImage = new Image("Motd_baseline.png");
FrameImage = new Image("Motd_baseline_big.png");
//FrameImage.Resize(416,416);

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
			if ( Object.Label != "person" || Object.Score < 0.05 )
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
