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
uniform float4 Rect;

void main()
{
	float4 Sample = texture( Image, uv );
	gl_FragColor = float4(Sample.xyz,1);
	
	if ( uv.x >= Rect.x && uv.y >= Rect.y && uv.x <= Rect.z && uv.y <= Rect.w )
		gl_FragColor.yz = float2(0,0);
	
}
`;


var FrameRect = [0,0,0.1,0.1];
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
		Shader.SetUniform("Rect", FrameRect );
	}
	
	RenderTarget.DrawQuad( FrameShader, SetUniforms );
	
}

//	startup
let Window1 = new OpenglWindow("CoreMl",true);
Window1.OnRender = function(){	RenderWindow( Window1 );	};
Window1.OnMouseMove = function(){};

FrameImage = new Image("jazzflute.jpg");

async function RunDetection(InputImage)
{
	try
	{
		var PeopleDetector = new CoreMl_MobileNet();
		const DetectedPeople = await PeopleDetector.Detect(FrameImage);
		FrameRect = DetectedPeople[0];
	}
	catch(e)
	{
		Debug(e);
	}
}
RunDetection( FrameImage );
