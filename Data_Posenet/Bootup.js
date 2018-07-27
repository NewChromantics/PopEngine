//	gr: include is not a generic thing (or a wrapper yet) so we can change
//	LoadFileAsString to a file-handle to detect file changes to auto reload things
function include(Filename)
{
	let Source = LoadFileAsString(Filename);
	CompileAndRun( Source );
}

include('Data_Posenet/XMLHttpRequest.js');
include('Data_Posenet/Webgl.js');


//	to allow tensorflow to TRY and read video, (and walk past the code), we at least need a constructor for instanceof HTMLVideoElement
function HTMLVideoElement()
{
	
}






include('Data_Posenet/tfjs.0.11.7.js');
include('Data_Posenet/posenet.0.1.2.js');
//include("Data_Posenet/Hello.js");


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

let DebugFragShaderSource = `
#version 410
in vec2 uv;
void main()
{
	gl_FragColor = vec4(uv.x,uv.y,0,1);
}
`;

var DebugFrameShader = null;

function WindowRender(RenderTarget)
{
	try
	{
		if ( !DebugFrameShader )
		{
			Debug("Creating window render shader");
			DebugFrameShader = new OpenglShader( RenderTarget, VertShaderSource, DebugFragShaderSource );
		}
		
		let SetUniforms = function()
		{
			
		}
		
		RenderTarget.ClearColour(0,1,0);
		RenderTarget.DrawQuad( DebugFrameShader, SetUniforms );
	}
	catch(Exception)
	{
		RenderTarget.ClearColour(1,0,0);
		Debug(Exception);
	}
}


function RunPoseDetection(PoseNet,NewImage,OnPoseFound)
{
	var imageScaleFactor = 0.20;
	var outputStride = 16;
	var flipHorizontal = false;
	
	//console.log("Processing...");
	//console.log(NewImage);
	//let StartTime = performance.now();
	
	let OnNewPose = function(NewPose)
	{
		//let EndTime = performance.now();
		//let ProcessingTime = EndTime - StartTime;
		//NewPose.ProcessingTimeMs = ProcessingTime;
		
		let ImageWidth = NewImage.width;
		let ImageHeight = NewImage.height;
		//console.log(ImageWidth);
		
		//	put coords in uv space
		let RescaleCoords = function(keypoint)
		{
			keypoint.position.x /= ImageWidth;
			keypoint.position.y /= ImageHeight;
			keypoint.position.y = 1-keypoint.position.y;
		};
		NewPose.keypoints.forEach( RescaleCoords );
		
		OnPoseFound(NewPose);
	}
	
	let OnEstimateFailed = function(e)
	{
		Debug("estimateSinglePose failed");
		Debug(e);
	}
	
	let EstimatePromise = PoseNet.estimateSinglePose(NewImage, imageScaleFactor, flipHorizontal, outputStride);
	EstimatePromise.then( OnNewPose ).catch( OnEstimateFailed );
}

function StartPoseDetection(PoseNet)
{
	Debug("Posenet loaded!");
	
	let OnFoundPose = function(Pose)
	{
		try
		{
			//SendNewPose(Pose);
		}
		catch(e)
		{
			console.log(e);
		}
		Debug("Found pose");
		//console.log("Found pose in " + Pose.ProcessingTimeMs + "ms: ");
		console.log(Pose);
	}

	let FrameImage = new Image('Data_Posenet/jazzflute.jpg');
	FrameImage.width = FrameImage.GetWidth();
	FrameImage.height = FrameImage.GetHeight();
	
	try
	{
		RunPoseDetection( PoseNet, FrameImage, OnFoundPose );
	}
	catch(e)
	{
		//OnFoundPose(null, 0);
		console.log(e);
	}

}

function PosenetFailed(Arg1)
{
	Debug("Posenet failed to load");
	Debug(Arg1);
}

function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Posenet");
	Window1.OnRender = function(){	WindowRender( Window1 );	};

	window.OpenglContext = Window1;

	
	//	load posenet
	Debug("Loading posenet...");
	posenet.load().then( StartPoseDetection ).catch( PosenetFailed );
	
}

//	main
Main();
