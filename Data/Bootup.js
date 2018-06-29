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
		uv = vec2(TexCoord.x,TexCoord.y);
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
		vec2 Flippeduv = vec2( uv.x, 1-uv.y );
		gl_FragColor = texture( Image, Flippeduv );
		//gl_FragColor *= vec4(uv.x,uv.y,0,1);
	}
`;


let EdgeFragShaderSource = `
	#version 410
	in vec2 uv;
	uniform sampler2D Image;

	float GetLum(vec3 rgb)
	{
		float lum = max( rgb.x, max( rgb.y, rgb.z ) );
		return lum;
	}

	float GetLumSample(vec2 uvoffset)
	{
		vec3 rgb = texture( Image, uv+uvoffset ).xyz;
		return GetLum(rgb);
	}

	void main()
	{
		vec2 ImageSize = vec2( 1280, 720 );
		vec2 uvstep2 = 1.0 / ImageSize;
		#define NeighbourCount	(3*3)
		float NeighbourLums[NeighbourCount];
		vec2 NeighbourSteps[NeighbourCount] =
		vec2[](
			vec2(-1,-1),	vec2(0,-1),	vec2(1,-1),
			vec2(-1,0),	vec2(0,0),	vec2(1,-1),
			vec2(-1,1),	vec2(0,1),	vec2(1,1)
		);
		
		for ( int n=0;	n<NeighbourCount;	n++ )
		{
			NeighbourLums[n] = GetLumSample( NeighbourSteps[n] * uvstep2 );
		}
		
		float BiggestDiff = 0;
		float ThisLum = NeighbourLums[4];
		for ( int n=0;	n<NeighbourCount;	n++ )
		{
			float Diff = abs( ThisLum - NeighbourLums[n] );
			BiggestDiff = max( Diff, BiggestDiff );
		}
		
		if ( BiggestDiff > 0.1 )
			gl_FragColor = vec4(1,1,1,1);
		else
			gl_FragColor = vec4(0,0,0,1);
	}
`;


var DrawImageShader = null;
var DebugShader = null;
var EdgeShader = null;
var LastProcessedImage = null;

function ReturnSomeString()
{
	return "Hello world";
}


function RenderHsl(RenderTarget)
{
	RenderTarget.ClearColour(1,0,0);
	return;
	
	if ( !EdgeShader )
	{
		EdgeShader = new OpenglShader( RenderTarget, VertShaderSource, EdgeFragShaderSource );
	}
	
	let SetUniforms = function(Shader)
	{
		Shader.SetUniform("Image", Frame, 0 );
	}
	
	RenderTarget.ClearColour(1,0,0);
	RenderTarget.DrawQuad( EdgeShader, SetUniforms );
}


function RenderGreenMask(RenderTarget)
{
	RenderTarget.ClearColour(0,1,0);
	return;

	if ( !EdgeShader )
	{
		EdgeShader = new OpenglShader( RenderTarget, VertShaderSource, EdgeFragShaderSource );
	}
	
	let SetUniforms = function(Shader)
	{
		Shader.SetUniform("Image", Frame, 0 );
	}
	
	RenderTarget.ClearColour(1,0,0);
	RenderTarget.DrawQuad( EdgeShader, SetUniforms );
}


function RenderLineMask(RenderTarget)
{
	RenderTarget.ClearColour(0,0,1);
	return;
	
	if ( !EdgeShader )
	{
		EdgeShader = new OpenglShader( RenderTarget, VertShaderSource, EdgeFragShaderSource );
	}
	
	let SetUniforms = function(Shader)
	{
		Shader.SetUniform("Image", Frame, 0 );
	}
	
	RenderTarget.ClearColour(1,0,0);
	RenderTarget.DrawQuad( EdgeShader, SetUniforms );
}


function StartProcessFrame(Frame,OpenglContext)
{
	log( "Frame size: " + Frame.GetWidth() + "x" + Frame.GetHeight() );
	let FrameHsl = new Image( [Frame.GetWidth(),Frame.GetHeight() ] );
	let FrameGreenMask = new Image( [Frame.GetWidth(),Frame.GetHeight() ] );
	let FrameLineMask = new Image( [Frame.GetWidth(),Frame.GetHeight() ] );

	//	blit into render target
	let MakeHsl = function(RenderTarget,RenderTargetTexture)
	{
		RenderHsl( RenderTarget );
		log("MakeHsl:" + RenderTarget );
		LastProcessedImage = RenderTargetTexture;
	};
	
	let MakeGreenMask = function(RenderTarget,RenderTargetTexture)
	{
		RenderGreenMask( RenderTarget );
		log("MakeGreenMask:" + RenderTarget );
		LastProcessedImage = RenderTargetTexture;
	};
	
	let MakeLineMask = function(RenderTarget,RenderTargetTexture)
	{
		RenderLineMask( RenderTarget );
		log("MakeLineMask:" + RenderTarget );
		LastProcessedImage = RenderTargetTexture;
	};
	
	let OnError = function(Error)
	{
		log(Error);
	};
	let Part1 = function()	{	return OpenglContext.Render( FrameHsl, MakeHsl );	}
	let Part2 = function()	{	return OpenglContext.Render( FrameGreenMask, MakeGreenMask );	}
	let Part3 = function()	{	return OpenglContext.Render( FrameLineMask, MakeLineMask );	}
	let Finish = function()
	{
		//LastProcessedImage = FrameHsl;
		log("Done!");
	};
	Part1().then( Part2 ).then( Part3 ).then( Finish ).catch( OnError );
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
	
	let Pitch = new Image("Data/ArgentinaVsCroatia.png");
	//let Pitch = new Image("Data/Cat.jpg");
	
	let OpenglContext = Window1;
	StartProcessFrame( Pitch, OpenglContext );
	
}

//	main
Main();
