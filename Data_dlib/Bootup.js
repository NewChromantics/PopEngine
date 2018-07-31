
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
var LastFrameFeatures = null;

function GetFeatureLines(Features)
{
	let Lines = [];
	let LineOffset = 1 / 400;
	let PushFeatureLine = function(fx,fy)
	{
		let x0 = fx-LineOffset;
		let x1 = fx+LineOffset;
		let y0 = fy-LineOffset;
		let y1 = fy+LineOffset;
		Lines.push( [x0,y0,x1,y1] );
	}
	for ( let i=0;	Features!=null && i<Features.length;	i+=2 )
	{
		PushFeatureLine( Features[i+0], Features[i+1] );
	}
	return Lines;
}

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
			let PoseLines = GetFeatureLines(LastFrameFeatures);
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
	LastFrameFeatures = FaceLandmarks;
	
	Debug("Got facelandmarks: x" + 	FaceLandmarks.length );
	Debug(FaceLandmarks);
}

function OnFailedNewFace(Error)
{
	Debug("Failed to get facelandmarks: " + Error);
}


var DlibLandMarksdat = LoadFileAsArrayBuffer('Data_Dlib/shape_predictor_68_face_landmarks.dat');


function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("dlib");
	Window1.OnRender = function(){	WindowRender(Window1);	};
	
	let FrameImage = new Image('Data_Posenet/jazzflute.jpg');
	LastFrameImage = FrameImage;
	
	let FaceProcessor = new Dlib(DlibLandMarksdat);
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
