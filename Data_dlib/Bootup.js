
let VertShaderSource = `
	#version 410
	const vec4 Rect = vec4(0,0,1,1);
	in vec2 TexCoord;
	out vec2 uv;
	void main()
	{
		gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);
		gl_Position.xy *= Rect.zw;
		gl_Position.xy += Rect.xy;
		//	move to view space 0..1 to -1..1
		gl_Position.xy *= vec2(2,2);
		gl_Position.xy -= vec2(1,1);
		uv = vec2(TexCoord.x,TexCoord.y);
	}
`;

let FrameFragShaderSource = LoadFileAsString("Data_Posenet/DrawFrameAndPose.frag");
var FrameShader = null;
var LastFrameImage = null;


function WindowRender(RenderTarget)
{
	try
	{
		if ( !FrameShader )
		{
			FrameShader = new OpenglShader( RenderTarget, VertShaderSource, FrameFragShaderSource );
		}
		
		let SetUniforms = function(Shader)
		{
			if ( LastFrameImage != null )
				Shader.SetUniform("Frame", LastFrameImage, 0 );
			Shader.SetUniform("HasFrame", LastFrameImage!=null );
			
			const MAX_LINES = 100;
			let PoseLines = GetPoseLines(LastPose);
			PoseLines.length = Math.min( PoseLines.length, MAX_LINES );
			//Debug(PoseLines);
			Shader.SetUniform("Lines", PoseLines );
			/*
			uniform vec4		Lines[LINE_COUNT];
			uniform float		LineScores[LINE_COUNT];
			*/
		}
		
		RenderTarget.DrawQuad( FrameShader, SetUniforms );
	}
	catch(Exception)
	{
		RenderTarget.ClearColour(1,0,0);
		Debug(Exception);
	}
}


function OnNewFace(FaceLandmarks)
{
	Debug("Got facelandmarks: ");
	Debug(FaceLandmarks);
}

function OnFailedNewFace(Error)
{
	Debug("Failed to get facelandmarks: " + Error);
}


function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("dlib");
	
	let FrameImage = new Image('Data_Posenet/jazzflute.jpg');

	let FaceProcessor = new Dlib();
	//try
	{
		Debug("Running find face...");
		FaceProcessor.FindFace(FrameImage).then(OnNewFace).catch( OnFailedNewFace );
	}
	//catch(e)
	{
	//	Debug(e);
	//	OnFailedNewFace(e);
	}
}

//	main
Main();
