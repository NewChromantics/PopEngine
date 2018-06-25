let VertShaderSource = `
	#version 410
	const vec4 Rect = vec4(0,0,1,1);
	in vec2 TexCoord;
	out vec2 uv;
	out float Blue_Frag;
	uniform float Blue;
	void main()
	{
		gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);
		gl_Position.xy *= Rect.zw;
		gl_Position.xy += Rect.xy;
		//	move to view space 0..1 to -1..1
		gl_Position.xy *= vec2(2,2);
		gl_Position.xy -= vec2(1,1);
		uv = vec2(TexCoord.x,1-TexCoord.y);
		Blue_Frag = Blue;
	}
`;

let DebugFragShaderSource = `
	#version 410
	in vec2 uv;
	in float Blue_Frag;
	//out vec4 FragColor;
	void main()
	{
		gl_FragColor = vec4(uv.x,uv.y,Blue_Frag,1);
	}
`;

let ImageFragShaderSource = `
	#version 410
	in vec2 uv;
	uniform sampler2D Image;
	void main()
	{
		gl_FragColor = texture( Image, uv );
		gl_FragColor *= vec4(uv.x,uv.y,0,1);
	}
`;

var DrawImageShader = null;
var DebugShader = null;
var LastProcessedImage = null;

function ReturnSomeString()
{
	return "Hello world";
}


function ProcessFrame(RenderTarget,Frame)
{
	if ( !DebugShader )
	{
		DebugShader = new OpenglShader( RenderTarget, VertShaderSource, DebugFragShaderSource );
	}
	
	let SetUniforms = function(Shader)
	{
		Shader.SetUniform("Image", Frame, 0 );
	}
	
	RenderTarget.DrawQuad( DebugShader, SetUniforms );
}

function StartProcessFrame(Frame,OpenglContext)
{
	log( "Frame size: " + Frame.GetWidth() + "x" + Frame.GetHeight() );
	let FrameEdges = new Image( [Frame.GetWidth(),Frame.GetHeight() ] );
	//LastProcessedImage = FrameEdges;
	
	//	blit into render target
	let OnBlit = function(RenderTarget)
	{
		ProcessFrame( RenderTarget, Frame );
	};
	//OpenglContext.Blit( FrameEdges, OnBlit );
}


function WindowRender(RenderTarget)
{
	try
	{
		if ( LastProcessedImage == null )
		{
			RenderTarget.ClearColour(0,1,1);
			return;
		}
		
		if ( !DrawImageShader )
		{
			DrawImageShader = new OpenglShader( RenderTarget, VertShaderSource, ImageFragShaderSource );
		}
		
		let Shader = DrawImageShader;
		
		let SetUniforms = function(Shader)
		{
			//Blue = (Blue==0) ? 1 : 0;
			//log("On bind: " + Blue);
			//Shader.SetUniform("Blue", Blue );
			//log( typeof(Pitch) );
			Shader.SetUniform("Image", LastProcessedImage, 0 );
		}
		
		RenderTarget.ClearColour(0,1,0);
		RenderTarget.DrawQuad( Shader, SetUniforms );
	}
	catch(Exception)
	{
		RenderTarget.ClearColour(1,0,0);
		log(Exception);
	}
}

function Main()
{
	//log("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Hello!");
	
	Window1.OnRender = function(){	WindowRender( Window1 );	};
	
	let Pitch = new Image("Data/FootballPitch_Rotated90.png");
	//LastProcessedImage = Pitch;
	
	let OpenglContext = Window1;
	StartProcessFrame( Pitch, OpenglContext );
}

//	main
Main();
