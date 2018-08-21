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
var ClipToSquare = true;	//	gr: slow atm!
//var ClipToSquare = 500;	//	gr: slow atm!

var FindFaceAroundLastHeadRectScale = 1.6;	//	make this expand more width ways
var ClippedImageScale = 0.400;
var BlurLandmarkSearch = false;
var ShoulderToHeadWidthRatio = 0.8;
var HeadWidthToHeightRatio = 2.4;
var NoseHeightInHead = 0.5;

var ResizeFragShaderSource = LoadFileAsString("GreyscaleToRgb.frag");
var ResizeFragShader = null;
var DrawSmallImage = true;

var CurrentFrames = [];
var LastFrame = null;	//	completed TFrame




var DlibLandMarksdat = LoadFileAsArrayBuffer('shape_predictor_68_face_landmarks.dat');
var DlibThreadCount = 2;
var FaceProcessor = null;



function Range(Min,Max,Value)
{
	return (Value-Min) / (Max-Min);
}

function Lerp(Min,Max,Value)
{
	return Min + ( Value * (Max-Min) );
}


function Clamp(Min,Max,Value)
{
	return Math.min( Max, Math.max( Min, Value ) );
}

function Clamp01(Value)
{
	return Clamp( 0, 1, Value );
}

function ClampRect01(Rect)
{
	let l = Rect[0];
	let t = Rect[1];
	let r = l + Rect[2];
	let b = t + Rect[3];
	
	l = Clamp01(l);
	r = Clamp01(r);
	t = Clamp01(t);
	b = Clamp01(b);
	
	Rect[0] = l;
	Rect[1] = t;
	Rect[2] = r-l;
	Rect[3] = b-t;
}



function NormaliseRect(ChildRect,ParentRect)
{
	let pl = ParentRect[0];
	let pr = pl + ParentRect[2];
	let pt = ParentRect[1];
	let pb = pt + ParentRect[3];
	
	let cl = ChildRect[0];
	let cr = cl + ChildRect[2];
	let ct = ChildRect[1];
	let cb = ct + ChildRect[3];
	
	let l = Range( pl, pr, cl );
	let r = Range( pl, pr, cr );
	let t = Range( pt, pb, ct );
	let b = Range( pt, pb, cb );
	let w = r-l;
	let h = b-t;
	ChildRect[0] = l;
	ChildRect[1] = t;
	ChildRect[2] = w;
	ChildRect[3] = h;
}


function GetScaledRect(Rect,Scale)
{
	let w = Rect[2];
	let h = Rect[3];
	let cx = Rect[0] + (w/2);
	let cy = Rect[1] + (h/2);
	
	//	scale size
	w *= Scale;
	h *= Scale;
	
	let l = cx - (w/2);
	let t = cy - (h/2);
	
	l = Math.floor(l);
	t = Math.floor(t);
	w = Math.floor(w);
	h = Math.floor(h);
	
	return [l,t,w,h];
}



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


function GetRectLines(Rect,Lines,Scores,Normalise,Score)
{
	let l = Rect[0];
	let t = Rect[1];
	let r = Rect[0] + Rect[2];
	let b = Rect[1] + Rect[3];

	let lt = Normalise( l,t );
	let rb = Normalise( r,b );
	l = lt[0];
	t = lt[1];
	r = rb[0];
	b = rb[1];
	
	Lines.push( [l,t,	r,t] );
	Lines.push( [r,t,	r,b] );
	Lines.push( [r,b,	l,b] );
	Lines.push( [l,b,	l,t] );

	Scores.push( Score );
	Scores.push( Score );
	Scores.push( Score );
	Scores.push( Score );
}




var TempSharedImageData = null;

var TFrame = function(OpenglContext)
{
	this.Image = null;
	this.SmallImage = null;		//	face search image
	this.FaceFeatures = null;
	this.SkeletonPose = null;
	this.ImageData = null;
	this.OpenglContext = OpenglContext;
	
	//	rects are in Image space(px)
	this.HeadRect = [0,0,1,1];	//	head area on skeleton
	this.FaceRect = [0,0,1,1];	//	detected face
	//	gr: currently normalised!
	this.ClipRect = [0,0,1,1];	//	small image clip rect
	
	this.GetWidth = function()
	{
		return this.Image.GetWidth();
	}
	
	this.GetHeight = function()
	{
		return this.Image.GetHeight();
	}
	
	this.GetImageRect = function()
	{
		return [0,0,this.GetWidth(),this.GetHeight()];
	}
	
	this.Clear = function()
	{
		if ( this.Image )
		{
			this.Image.Clear();
			this.Image = null;
		}
		
		if ( this.SmallImage )
		{
			this.SmallImage.Clear();
			this.SmallImage = null;
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
		GetRectLines( this.HeadRect, Lines, Scores, Normalise, 0 );
		GetRectLines( this.ClipRect, Lines, Scores, function(x,y){	return[x,y];}, 1.5 );
		//GetRectLines( this.FaceRect, Lines, Scores, Normalise, 0.5 );
		GetPoseLinesAndScores( this.Pose, Lines, Scores, Normalise );
	}
	
	
	this.SetupImageData = function()
	{
		//	here, the typedarray just hangs around until garbage collection
		//	todo: make a pool & return to the pool when done
		if ( !(this.Image instanceof Image) )
			throw "Expecting frame image to be an Image";
		
		if ( TempSharedImageData == null )
		//if ( true )
		{
			TempSharedImageData = new ImageData(this.Image);
			this.ImageData = TempSharedImageData;
		}
		else
		{
			this.Image.GetRgba8(AllowBgraAsRgba,TempSharedImageData.data);
			this.ImageData = TempSharedImageData;
		}
	}
	
	this.GetSkeletonKeypoint = function(Name)
	{
		let IsMatch = function(Keypoint)
		{
			return Keypoint.part == Name;
		}
		return this.Pose.keypoints.find( IsMatch );
	}
	
	this.SetupHeadRect = function()
	{
		//	gr: ears are unreliable, get head size from shoulders
		//		which is a shame as we basically need ear to ear size
		let Nose = this.GetSkeletonKeypoint('nose');
		let Left = this.GetSkeletonKeypoint('leftShoulder');
		let Right = this.GetSkeletonKeypoint('rightShoulder');
		if ( !Nose || !Left || !Right )
			throw "No nose||Left||Right, can't make face rect";
		
		Nose = Nose.position;
		Left = Left.position;
		Right = Right.position;
	
		if ( Left.x > Right.x )
		{
			let Temp = Right;
			Right = Left;
			Left = Temp;
		}
		let Width = (Right.x - Left.x) * ShoulderToHeadWidthRatio;
		let Height = Width * HeadWidthToHeightRatio;
		let Bottom = Nose.y + (Height * NoseHeightInHead);
		let x = Nose.x - (Width/2);
		let y = Bottom - Height;
		
		x = Math.floor(x);
		y = Math.floor(y);
		Width = Math.floor(Width);
		Height = Math.floor(Height);
		
		this.HeadRect = [x,y,Width,Height];
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
				if ( DrawSmallImage )
				{
					Shader.SetUniform("Frame", LastFrame.SmallImage, 0 );
					Shader.SetUniform("UnClipRect", LastFrame.ClipRect );
				}
				else
				{
					Shader.SetUniform("Frame", LastFrame.Image, 0 );
					Shader.SetUniform("UnClipRect", [0,0,1,1] );
				}
				Shader.SetUniform("HasFrame", true );
				LastFrame.GetLinesAndScores( Lines, Scores );
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
	//	work out where to search
	Frame.SetupHeadRect();
	Frame.ClipRect = GetScaledRect( Frame.HeadRect, FindFaceAroundLastHeadRectScale );
	Debug("Frame.HeadRect=" + Frame.HeadRect);
	Debug("Frame.ClipRect[A]=" + Frame.ClipRect);
	NormaliseRect( Frame.ClipRect, Frame.GetImageRect() );
	Debug("Frame.ClipRect[B]=" + Frame.ClipRect);
	ClampRect01( Frame.ClipRect );
	Debug("Frame.ClipRect[C]=" + Frame.ClipRect);
	
	if ( Frame.ClipRect[2] <= 0 || Frame.ClipRect[3] <= 0 )
	{
		throw "Cliprect offscreen/zero width: " + Frame.ClipRect;
	}
	Debug("Frame.ClipRect[D]=" + Frame.ClipRect);
	
	//	setup the image
	let SmallImageWidth = Frame.GetWidth();
	let SmallImageHeight = Frame.GetHeight();
	let HeightRatio = SmallImageHeight / SmallImageWidth;
	SmallImageWidth = 640;
	SmallImageHeight = SmallImageWidth * HeightRatio;
		
	//	the face search looks for 80x80 size faces, scale and blur accordingly
	SmallImageWidth = (Frame.GetWidth() * ClippedImageScale) * Frame.ClipRect[2];
	SmallImageHeight = (Frame.GetHeight() * ClippedImageScale) * Frame.ClipRect[3];
	
	Debug("SmallImage="+ SmallImageWidth+"x"+SmallImageHeight+ " Frame=" + Frame.GetWidth()+"x"+Frame.GetHeight() + " ClippedImageScale=" + ClippedImageScale + " Frame.ClipRect[2]x[3]=" + Frame.ClipRect[2]+"x"+Frame.ClipRect[3]);
	
	//	return a resizing promise
	if ( Frame.OpenglContext )
	{
		let ResizeRender = function(RenderTarget,RenderTargetTexture)
		{
			try	//	these promise exceptions aren't being caught in the grand chain
			{
				Debug("ResizeRender Frame=" + Frame);

				if ( !ResizeFragShader )
				{
					ResizeFragShader = new OpenglShader( RenderTarget, VertShaderSource, ResizeFragShaderSource );
				}
					
				let SetUniforms = function(Shader)
				{
					Shader.SetUniform("ClipRect", Frame.ClipRect );
					Shader.SetUniform("Source", Frame.Image, 0 );
					Shader.SetUniform("ApplyBlur", BlurLandmarkSearch );
				}
				RenderTarget.DrawQuad( ResizeFragShader, SetUniforms );
			}
			catch(e)
			{
				Debug(e);
				throw e;
			}
		}
		
		try	//	these promise exceptions aren't being caught in the grand chain
		{
			//Debug("SmallImageWidth=" + SmallImageWidth + " SmallImageHeight=" + SmallImageHeight);
			Frame.SmallImage = new Image( [SmallImageWidth, SmallImageHeight] );
			//Debug("allocated");
			Frame.SmallImage.SetLinearFilter(true);
			//Debug("searching SmallImage.width=" + Frame.SmallImage.GetWidth() + " SmallImage.height=" + Frame.SmallImage.GetHeight() );
			let ReadBackPixels = true;
			
			//	return resizing promise
			//Debug("Frame.OpenglContext.Render");
			let ResizePromise = Frame.OpenglContext.Render( Frame.SmallImage, ResizeRender, ReadBackPixels );
			return ResizePromise;
		}
		catch(e)
		{
			Debug(e);
			throw e;
		}
	}
	else	//	CPU mode
	{
		let ResizeCpu = function(Resolve,Reject)
		{
			Debug("ResizeCpu Frame=" + Frame);

			try
			{
				Frame.SmallImage = new Image();
				Frame.SmallImage.Copy( Frame.Image );
				Frame.SmallImage.Resize( SmallImageWidth, SmallImageHeight );
				Resolve();
			}
			catch(e)
			{
				Debug(e);
				Reject();
			}
		}

		ResizePromise = new Promise( ResizeCpu );
		return ResizePromise;
	}
}


function GetFaceDetectionPromise(Frame)
{
	Debug("GetFaceDetectionPromise on " + Frame.SmallImage );
	return FaceProcessor.FindFaces( Frame.SmallImage );
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
	let OpenglContext = window.OpenglContext;
	let Frame = new TFrame(OpenglContext);
	CurrentFrames.push( Frame );
	
	Frame.Image = FrameImage;

	SetupForPoseDetection( Frame )
	.then( GetPoseDetectionPromise )
	.then( SetupForFaceDetection )
	.then( function()		{	return GetFaceDetectionPromise(Frame);		}	)
	.then( function(Face)	{	Frame.Face = Face;	return OnFrameCompleted(Frame);		}	)
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




function LoadDlib()
{
	FaceProcessor = new Dlib( DlibLandMarksdat, DlibThreadCount );
}




function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("PopTrack5",true);
	Window1.OnRender = function(){	WindowRender( Window1 );	};
	
	//	navigator global window is setup earlier
	window.OpenglContext = Window1;
	
	LoadDlib();
	LoadPosenet();
	LoadVideo();
}

//	main
Main();
