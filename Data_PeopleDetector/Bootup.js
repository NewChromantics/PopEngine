//	gr: include is not a generic thing (or a wrapper yet) so we can change
//	LoadFileAsString to a file-handle to detect file changes to auto reload things
function include(Filename)
{
	let Source = LoadFileAsString(Filename);
	return CompileAndRun( Source );
}
include("../Data_Holosports/PopDomJs/PopDomJs.js");

Math.clamp = function(min, max,Value)
{
	return Math.min( Math.max(Value, min), max);
}
Math.range = function(Min,Max,Value)
{
	return (Value-Min) / (Max-Min);
}
Math.rangeClamped = function(Min,Max,Value)
{
	return Math.clamp( 0, 1, Math.range( Min, Max, Value ) );
}


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
const int RectCount = 150;
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
	
	
	float BestScore = 0;
	for ( int RectIndex=0;	RectIndex<RectCount;	RectIndex++ )
	{
		if ( !InsideRect( uv, Rects[RectIndex] ) )
			continue;

		BestScore = max( BestScore, RectScores[RectIndex] );
	}

	float BlendAlpha = (BestScore > 0.0) ? 0.6 : 0.0;
	float3 ScoreRgb = NormalToRedGreen( BestScore );
	gl_FragColor.xyz = BlendColour( gl_FragColor.xyz, ScoreRgb, BlendAlpha );
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

var PersonImages = [];

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
				let PersonImage = PersonImages[RectIndex];
				
				Shader.SetUniform("VertexRect", Rect );
				//Shader.SetUniform("Image", FrameImage, 0 );
				//Shader.SetUniform("Rect", FrameRects[RectIndex] );
				Shader.SetUniform("Image", PersonImage, 0 );
				//Shader.SetUniform("Rect", [0,0,1,1] );
				
				Shader.SetUniform("Rects", PersonImage.Rects );
				Shader.SetUniform("RectScores", PersonImage.RectScores );
			}
			try
			{
				//RenderTarget.DrawQuad( SingleRectShader, SetUniforms );
				RenderTarget.DrawQuad( AllRectsShader, SetUniforms );
			}
			catch(e)
			{}
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

let ch = FrameImage.GetHeight();
let cy = FrameImage.GetHeight() - ch;
let cw = ch;
let cx = FrameImage.GetWidth() - cw;
FrameImage.Clip( [cx, cy, cw, ch] );

function MakeRectSquareCentered(Rect)
{
	//	don't modify original rect
	Rect = Rect.slice();
	
	let PadWidth = 0;
	let PadHeight = 0;
	let w = Rect[2];
	let h = Rect[3];
	if ( w==h )
		return Rect;
	
	if ( w > h )
		PadHeight = w - h;
	else
		PadWidth = h - w;
	
	Rect[0] -= PadWidth/2;
	Rect[1] -= PadHeight/2;
	Rect[2] += PadWidth;
	Rect[3] += PadHeight;
	return Rect;
}

function GrowRect(Rect,Scale)
{
	//	don't modify original rect
	Rect = Rect.slice();

	let LeftChange = Rect[2] * Scale;
	let TopChange = Rect[3] * Scale;
	Rect[0] -= LeftChange/2;
	Rect[1] -= TopChange/2;
	Rect[2] += LeftChange;
	Rect[3] += TopChange;
	return Rect;
}

async function RunDetection(InputImage)
{
	let CompareObject = function(a,b)
	{
		if ( a.Score > b.Score )	return -1;
		if ( a.Score < b.Score )	return 1;
		return 0;
	}
	
	let Detector = new CoreMl();
	try
	{
		let YoloFrameImage = new Image([1,1]);
		YoloFrameImage.Copy( FrameImage );
		
		YoloFrameImage.Resize(300,300);
		const DetectedPeople = await Detector.SsdMobileNet(YoloFrameImage);
		//YoloFrameImage.Resize(416,416);
		//const DetectedPeople = await Detector.Yolo(YoloFrameImage);
		
		DetectedPeople.sort(CompareObject);
		Debug("detected x"+DetectedPeople.length);
		FrameRects = [];
		FrameRectScores = [];
		let PersonMinScore = 0.10;
		let MaxMatches = 100;
		let PushRect = function(Object)
		{
			//	limit
			if ( FrameRects.length >= MaxMatches )
				return;
			if ( Object.Label != "person" || Object.Score < PersonMinScore )
			{
				Debug("Skipped " + Object.Label + " at " + ((Object.Score*100).toFixed(2)) + "%");
				return;
			}
			Debug(Object.Label + " at " + ((Object.Score*100).toFixed(2)) + "%");
			
			Object.Score /= 0.50;
			Object.Score = Math.min( 1, Object.Score );
			//Object.x = 0;
			//Object.y = 0;
			let Rect = [Object.x,Object.y,Object.w,Object.h];
			let Score = Object.Score;
			//Debug(Rect);
			//Debug(Score);
			
			//	extract an image to do more processing on
			let Person = new Image([1,1]);
			Person.Copy( InputImage );
			
			//	use normalised coords
			//	gr: clip to square for later processing
			let ClipRect = Rect;
			ClipRect = GrowRect( ClipRect, 1.05 );
			ClipRect = MakeRectSquareCentered( ClipRect );
			ClipRect[0] *= Person.GetWidth();
			ClipRect[1] *= Person.GetHeight();
			ClipRect[2] *= Person.GetWidth();
			ClipRect[3] *= Person.GetHeight();
			
			Debug("Clip: " +  ClipRect );
			Person.Clip( ClipRect );
			FrameRects.push( Rect );
			FrameRectScores.push( Score );
			PersonImages.push( Person );
		}
		//DetectedPeople.forEach(PushRect);
		for ( let i=0;	i<DetectedPeople.length;	i++ )
		{
			try
			{
				PushRect(DetectedPeople[i],i);
			}
			catch(e)
			{
				Debug("Failed to extract person: " + e);
			}
		}
		
		//	now run body-detection on each person
		let RunPersonDetection = async function(PersonImage,PersonIndex)
		{
			//	resize to fit model requirement
			PersonImage.Resize(368,368);
			//PersonImage.Resize(192,192);
			//const DetectedLimbs = await Detector.Hourglass(PersonImage);
			//const DetectedLimbs = await Detector.Cpm(PersonImage);
			const DetectedLimbs = await Detector.OpenPose(PersonImage);
			Debug("detected limbs x"+DetectedLimbs.length);

			//	make rects on each player image to render
			PersonImage.Rects = [];
			PersonImage.RectScores = [];
			let AppendRect = function(Object)
			{
				//if ( ["Head"].indexOf(Object.Label) == -1 )
				//if ( ["Neck","Top"/*,"RightShoulder","LeftShoulder"*/].indexOf(Object.Label) == -1 )
				//if ( ["Head","LeftAnkle","RightAnkle"/*,"RightShoulder","LeftShoulder"*/].indexOf(Object.Label) == -1 )
				//if ( ["LeftAnkle","RightAnkle","LeftKnee","RightKnee"].indexOf(Object.Label) == -1 )
				if ( Object.Label == "Background" )
				{
					return;
				}
				if ( PersonImage.Rects.length > 100 )
					return;
				
				let MinScore = 0.0;
				let MaxScore = 0.5;
				let Rect = [Object.x,Object.y,Object.w,Object.h];
				let Score = Object.Score;
				if ( Score < MinScore )
					return;
				/*
				if ( Object.Label == "Neck" )
					Score = 0.1;
				else Score = 1;
				 */
				Score = Math.rangeClamped( MinScore, MaxScore, Score );
				//Score = 1;
				PersonImage.Rects.push( Rect );
				PersonImage.RectScores.push( Score );
			};
			
			DetectedLimbs.sort( CompareObject );
			DetectedLimbs.forEach( AppendRect );
		}
		for ( let PersonIndex=0;	PersonIndex<PersonImages.length;	PersonIndex++)
		{
			const PersonResult = await RunPersonDetection( PersonImages[PersonIndex], PersonIndex );
			
		}
	}
	catch(e)
	{
		Debug(">>>> Exception: " + e);
	}
}
RunDetection( FrameImage );
