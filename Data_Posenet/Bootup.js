//	gr: include is not a generic thing (or a wrapper yet) so we can change
//	LoadFileAsString to a file-handle to detect file changes to auto reload things
function include(Filename)
{
	let Source = LoadFileAsString(Filename);
	return CompileAndRun( Source );
}

include('XMLHttpRequest.js');
var OnOpenglImageCreated = undefined;
include('Webgl.js');
include('FrameCounter.js');


var RGBAFromCamera = true;
var FlipCameraInput = false;
//	tries to find these in order, then grabs any
var VideoDeviceNames = ["c920","isight","facetime"];

var WebServer = null;
var WebServerPort = 8000;


var AllowBgraAsRgba = true;
var PoseNetScale = 0.20;
var PoseNetOutputStride = 32;
var PoseNetMirror = false;
//var outputStride = 32;
//var ClipToSquare = false;
//var ClipToSquare = true;	//	gr: slow atm!
var ClipToSquare = 500;	//	gr: slow atm!



var CurrentFrames = [];
var LastFrame = null;	//	completed TFrame








function GetXLinesAndScores(Lines,Scores)
{
	Lines.push( [0,0,1,1] );
	Scores.push( [1] );
	Lines.push( [1,0,0,1] );
	Scores.push( [0] );
}

function GetPoseLinesAndScores(Pose,Lines,Scores,Normalise)
{
	if ( !Pose )
		return;
	
	let PushLine = function(Keypointa,Keypointb)
	{
		if ( Keypointa === undefined || Keypointb === undefined )
			return;
		
		let Score = (Keypointa.score + Keypointb.score)/2;
		let Start = Normalise( Keypointa.position.x, Keypointa.position.y );
		let End = Normalise( Keypointb.position.x, Keypointb.position.y );
		let Line = [ Start[0], Start[1], End[0], End[1] ];
		Lines.push( Line );
		Scores.push( Score );
	}
	
	let GetKeypoint = function(Name)
	{
		let IsMatch = function(Keypoint)
		{
			return Keypoint.part == Name;
		}
		return Pose.keypoints.find( IsMatch );
	}
	
	let PushBone = function(BonePair)
	{
		let kpa = GetKeypoint(BonePair[0]);
		let kpb = GetKeypoint(BonePair[1]);
		PushLine( kpa, kpb );
	}
	
	let Bones = [["nose", "leftEye"], ["leftEye", "leftEar"], ["nose", "rightEye"], ["rightEye", "rightEar"], ["nose", "leftShoulder"], ["leftShoulder", "leftElbow"], ["leftElbow", "leftWrist"], ["leftShoulder", "leftHip"], ["leftHip", "leftKnee"], ["leftKnee", "leftAnkle"], ["nose", "rightShoulder"], ["rightShoulder", "rightElbow"], ["rightElbow", "rightWrist"], ["rightShoulder", "rightHip"], ["rightHip", "rightKnee"], ["rightKnee", "rightAnkle"]];
	Bones.forEach( PushBone );
}

var TFrame = function()
{
	this.Image = null;
	this.FaceFeatures = null;
	this.SkeletonPose = null;
	this.ImageData = null;
	
	this.Clear = function()
	{
		if ( this.Image )
		{
			this.Image.Clear();
			this.Image = null;
		}
		
		this.ImageData = null;
		//GarbageCollect();
	}
	
	this.GetLinesAndScores = function(Lines,Scores)
	{
		let w = this.ImageData.width;
		let h = this.ImageData.height;
		let Normalise = function(x,y)
		{
			return [ x/w, y/h ];
		}
		GetPoseLinesAndScores( this.Pose, Lines, Scores, Normalise );
	}
	
	
	this.SetupImageData = function()
	{
		//	here, the typedarray just hangs around until garbage collection
		//	todo: make a pool & return to the pool when done
		if ( !(this.Image instanceof Image) )
			throw "Expecting frame image to be an Image";
		
		this.ImageData = new ImageData(this.Image);
		//NewImage.GetRgba8(AllowBgraAsRgba,CachedImageData.data);
	}
}




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

let FrameFragShaderSource = LoadFileAsString("DrawFrameAndPose.frag");
var FrameShader = null;

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
			let Lines = [];
			let Scores = [];
			
			if ( LastFrame == null )
			{
				Shader.SetUniform("HasFrame", false );
				GetXLinesAndScores( Lines, Scores );
				Shader.SetUniform("UnClipRect", [0,0,1,1] );
			}
			else
			{
				Shader.SetUniform("Frame", LastFrame.Image, 0 );
				Shader.SetUniform("HasFrame", true );
				LastFrame.GetLinesAndScores( Lines, Scores );
				Shader.SetUniform("UnClipRect", [0,0,1,1] );
			}
			
			const MAX_LINES = 100;
			Lines.length = Math.min( Lines.length, MAX_LINES );
			Scores.length = Math.min( Scores.length, MAX_LINES );
			Shader.SetUniform("Lines", Lines );
			Shader.SetUniform("LineScores", Scores );
		}
		
		RenderTarget.DrawQuad( FrameShader, SetUniforms );
	}
	catch(Exception)
	{
		RenderTarget.ClearColour(1,0,0);
		Debug(Exception);
	}
}




function IsReady()
{
	if ( PoseNet == null )
		return false;
	
	return true;
}

function IsIdle()
{
	if ( !IsReady() )
		return false;
	
	if ( CurrentFrames.length > 0 )
		return false;
	return true;
}

function OnFrameCompleted(Frame)
{
	UpdateFrameCounter('FrameCompleted');
	
	CurrentFrames = CurrentFrames.filter( function(el)	{	return el!=Frame;	} );
	
	if ( LastFrame != null )
		LastFrame.Clear();
	LastFrame = Frame;
	
	Debug("Got a pose? " + Frame.Pose );
}

function OnFrameError(Frame,Error)
{
	Debug("OnFrameError(" + Error + ")");
	CurrentFrames = CurrentFrames.filter( function(el)	{	return el!=Frame;	} );

	Frame.Clear();
}










//	valid when posenet is loaded
var PoseNet = null;

function OnPoseNetLoaded(pn)
{
	PoseNet = pn;
}

function OnPoseNetFailed(Error)
{
	throw "Posenet Failed to load " + Error;
}

function LoadPosenet()
{
	//	make a context, then let tensorflow grab the bindings
	include('tfjs.0.11.7.js');
	include('posenet.0.1.2.js');
	
	//	load posenet
	Debug("Loading posenet...");
	posenet.load().then( OnPoseNetLoaded ).catch( OnPoseNetFailed );
}



function SetupForPoseDetection(Frame)
{
	let Runner = function(Resolve,Reject)
	{
		//	gr: we may resize & filter here ourselves before putting into the tensorflow resize
		/*
		//	clip image to square
		if ( ClipToSquare )
		{
			let Width = Frame.Image.GetWidth();
			let Height = Frame.Image.GetHeight();
			if ( typeof ClipToSquare == "number" )
				Width = ClipToSquare;
			
			Width = Math.min( Width, Height );
			Height = Math.min( Width, Height );
			
			Frame.Image.Clip( [0,0,Width,Height] );
		}
		*/
		Frame.SetupImageData();
		
		Resolve(Frame);
	}
	
	return new Promise(Runner);
}


function GetPoseDetectionPromise(Frame)
{
	let Runner = function(Resolve,Reject)
	{
		let OnPose = function(Pose)
		{
			Frame.Pose = Pose;
			Resolve(Frame);
		}
		
		let OnPoseError = function(Error)
		{
			Reject(Error);
		}
		
		let EstimatePromise = PoseNet.estimateSinglePose( Frame.ImageData, PoseNetScale, PoseNetMirror, PoseNetOutputStride );
		EstimatePromise.then( OnPose )
		.catch( OnPoseError );
	}
	
	return new Promise(Runner);
}


function SetupForFaceDetection(Frame)
{
	let Runner = function(Resolve,Reject)
	{
		Debug("Do SetupForFaceDetection");
		Resolve(Frame);
	}
	
	return new Promise(Runner);
}


function GetFaceDetectionPromise(Frame)
{
	let Runner = function(Resolve,Reject)
	{
		Debug("Do GetFaceDetectionPromise");
		Resolve(Frame);
	}
	
	return new Promise(Runner);
}





function OnNewVideoFrameFilter()
{
	UpdateFrameCounter('Webcam');
	//	filter if busy here
	return IsIdle();
}

function OnNewVideoFrame(FrameImage)
{
	if ( !IsIdle() )
	{
		FrameImage.Clear();
		Debug("Skipped webcam image");
		return;
	}

	//	make a new frame
	let NewFrame = new TFrame();
	CurrentFrames.push( NewFrame );
	
	NewFrame.Image = FrameImage;

	SetupForPoseDetection( NewFrame )
	.then( GetPoseDetectionPromise )
	.then( SetupForFaceDetection )
	.then( GetFaceDetectionPromise )
	.then( OnFrameCompleted )
	.catch( function(Error)	{	OnFrameError(NewFrame,Error);	}	);
}







function LoadVideo()
{
	let GetDeviceNameMatch = function(DeviceNames,MatchName)
	{
		let MatchDeviceName = function(DeviceName)
		{
			//	case insensitive match
			let MatchIndex = DeviceName.search(new RegExp(MatchName, "i"));
			return (MatchIndex==-1) ? false : true;
		}
		let Match = DeviceNames.find( MatchDeviceName );
		return Match;
	}

	let LoadDevice = function(DeviceNames)
	{
		try
		{
			//	find best match name
			Debug("Got devices: x" + DeviceNames.length);
			Debug(DeviceNames);
			
			//	find device in list
			let VideoDeviceName = VideoDeviceNames.length ? VideoDeviceNames[0] : null;
			for ( let i=0;	i<VideoDeviceNames.length;	i++ )
			{
				let MatchedName = GetDeviceNameMatch(DeviceNames,VideoDeviceNames[i]);
				if ( !MatchedName )
					continue;
				VideoDeviceName = MatchedName;
				break;
			}
			Debug("Loading device: " + VideoDeviceName);
			
			let VideoCapture = new MediaSource(VideoDeviceName,RGBAFromCamera,OnNewVideoFrameFilter);
			VideoCapture.OnNewFrame = OnNewVideoFrame;
		}
		catch(e)
		{
			Debug(e);
		}
	}
	
		
	//	load webcam
	let MediaDevices = new Media();
	MediaDevices.EnumDevices().then( LoadDevice );
}


function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("PopTrack5",true);
	Window1.OnRender = function(){	WindowRender( Window1 );	};
	
	//	navigator global window is setup earlier
	window.OpenglContext = Window1;
	
	LoadPosenet();
	LoadVideo();
}

//	main
Main();
