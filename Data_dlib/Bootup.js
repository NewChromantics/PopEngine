//	gr: include is not a generic thing (or a wrapper yet) so we can change
//	LoadFileAsString to a file-handle to detect file changes to auto reload things
function include(Filename)
{
	let Source = LoadFileAsString(Filename);
	CompileAndRun( Source );
}

//Debug = function(){};

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

let ResizeFragShaderSource = LoadFileAsString("GreyscaleToRgb.frag");
var ResizeFragShader = null;


//	skeleton + face
var OutputSkeleton = null;
var OutputImage = null;
var OutputFilename = "../../../../SkeletonOutputFrames.json";
var OutputSkipFirstFrames = 20;

//	last simple skeleton we recieved
var LastSkeleton = null;


var ServerSkeletonReciever = null;
var ServerSkeletonRecieverPort = 8008;
var ServerSkeletonSender = null;
var ServerSkeletonSenderPort = 8007;
var BroadcastServer = null;
var BroadcastServerPort = 8009;
var WebServer = null;
var WebServerPort = 8000;

//	tries to find these in order, then grabs any
var VideoDeviceNames = ["c920","isight","facetime"];

var FlipOutputSkeleton = true;
var FlipInputSkeleton = true;
var RenderLastFrame = true;
var RenderRects = true;

var AlwaysFindFaceRect = true;
var FindFaceAroundLastFaceRectScale = 1.6;	//	make this expand more width ways
var FindFaceAroundLastFaceRect = true;
var ClippedImageScale = 0.300;

var EnableKalmanFilter = true;
var AlwaysScanSameFrame = false;
var FilterAroundNose = true;

var DlibThreadCount = 2;
var ShoulderToHeadWidthRatio = 0.45;
var HeadWidthToHeightRatio = 2.1;
var NoseHeightInHead = 0.5;




var LastFrameRateTimelapse = Date.now();
var FrameCounters = {};


function Range(Min,Max,Value)
{
	return (Value-Min) / (Max-Min);
}

function Lerp(Min,Max,Value)
{
	return Min + ( Value * (Max-Min) );
}


function ClampRect01(Rect)
{
	if ( Rect[0] < 0 )
	{
		Rect[2] -= Rect[0];
		Rect[0] = 0;
	}
	if ( Rect[1] < 0 )
	{
		Rect[3] -= Rect[1];
		Rect[1] = 0;
	}
	if ( Rect[0]+Rect[2] > 1 )
	{
		Rect[2] -= (Rect[0]+Rect[2])-1;
	}
	if ( Rect[1]+Rect[3] > 1 )
	{
		Rect[3] -= (Rect[1]+Rect[3])-1;
	}
}

function ScaleRect(Rect,Scale)
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
	return [l,t,w,h];
}

function CheckFrameRateLapse()
{
	let Now = Date.now();
	if ( Now - LastFrameRateTimelapse < 1000 )
		return;
	//	1 sec has lapsed
	//	ideally timelapsed = 1
	let TimeLapsed = (Now - LastFrameRateTimelapse) / 1000;
	
	let UpdateCounter = function(CounterName)
	{
		let Count = FrameCounters[CounterName];
		FrameCounters[CounterName] = 0;
		let Fps = Count / TimeLapsed;
		Debug( CounterName + " " + Fps.toFixed(2) + "fps");
	}
	let CounterNames = Object.keys(FrameCounters);
	CounterNames.forEach( UpdateCounter );
	LastFrameRateTimelapse = Now;
}

function UpdateFrameCounter(CounterName)
{
	if ( FrameCounters[CounterName] === undefined )
		FrameCounters[CounterName] = 0;
	FrameCounters[CounterName]++;
	CheckFrameRateLapse();
}



function GetRectLines(Rect)
{
	let Lines = [];
	
	let l = Rect[0];
	let t = Rect[1];
	let r = Rect[0] + Rect[2];
	let b = Rect[1] + Rect[3];
	
	Lines.push( [l,t,	r,t] );
	Lines.push( [r,t,	r,b] );
	Lines.push( [r,b,	l,b] );
	Lines.push( [l,b,	l,t] );
	
	return Lines;
}



function GetSkeletonLinesAndScores(Skeleton,GetRects)
{
	let Lines = [];
	let Scores = [];

	if ( Skeleton == null )
	{
		//	X
		Lines.push( [0,0,1,1] );
		Lines.push( [1,0,0,1] );
		Scores.push( 0 );
		Scores.push( 1 );
		return Lines;
	}

	if ( GetRects )
	{
		//	make rect lines blue
		Lines = Lines.concat( GetRectLines(Skeleton.FaceRect) );
		Scores = Scores.concat( [9,9,9,9] );
		
		if ( Skeleton.ClipRect )
		{
			Lines = Lines.concat( GetRectLines(Skeleton.ClipRect) );
			Scores = Scores.concat( [0.5,0.5,0.5,0.5] );
		}
	}
	
	let PushLine = function(namea,nameb,Score)
	{
		Score = Score || 99;
		let Posa = Skeleton[namea];
		let Posb = Skeleton[nameb];
		if ( Posa == undefined || Posb == undefined )
			return;
		
		Lines.push( [Posa.x,Posa.y,Posb.x,Posb.y] );
		Scores.push(Score);
	}

	let PushPoint = function(Name,Score)
	{
		let Pos = Skeleton[Name];
		if ( Pos == undefined )
			return;
		let LineOffset = 1 / 600;
		
		let fx = Pos.x;
		let fy = Pos.y;
		let x0 = fx-LineOffset;
		let x1 = fx+LineOffset;
		let y0 = fy-LineOffset;
		let y1 = fy+LineOffset;
		Lines.push( [x0,y0,x1,y1] );
		Scores.push(Score);
	}
	
	PushLine('nose','leftEye');
	PushLine('nose','rightEye');
	PushLine('leftEar','leftEye');
	PushLine('rightEar','rightEye');

	PushLine('nose','leftShoulder');
	PushLine('nose','rightShoulder');

	PushLine('leftShoulder','rightShoulder');
	PushLine('leftHip','rightHip');

	PushLine('leftShoulder','leftElbow');
	PushLine('leftElbow','leftWrist');
	PushLine('leftShoulder','leftHip');
	PushLine('leftHip','leftKnee');
	PushLine('leftKnee','leftAnkle');

	PushLine('rightShoulder','rightElbow');
	PushLine('rightElbow','rightWrist');
	PushLine('rightShoulder','rightHip');
	PushLine('rightHip','rightKnee');
	PushLine('rightKnee','rightAnkle');
	
	for ( let i=0;	i<FaceLandMarkNames.length;	i++)
	{
		let Name = FaceLandMarkNames[i];
		let Score = i / FaceLandMarkNames.length;
		if ( i >= 65 )
			Score= 9;
		PushPoint( Name, Score);
	}
	
	return [Lines,Scores];
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
			if ( OutputImage != null )
				Shader.SetUniform("Frame", OutputImage, 0 );
			Shader.SetUniform("HasFrame", RenderLastFrame && OutputImage!=null );
			
			if ( OutputSkeleton && OutputSkeleton.ClipRect )
			{
				Shader.SetUniform("UnClipRect", OutputSkeleton.ClipRect );
			}
			else
			{
				Shader.SetUniform("UnClipRect", [0,0,1,1] );
			}
			
			let LinesAndScores = GetSkeletonLinesAndScores( OutputSkeleton, RenderRects );
			let Lines = LinesAndScores[0];
			let Scores = LinesAndScores[1];

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



//	if it ends with !, we don't bother sending it out
let FaceLandMarkNames =
[
 	//	right is actor-right, not image-right
	//	17 outline features
	"RightEarTop",
	"FaceOutline1!",
	"FaceOutline2!",
	"FaceOutline3!",
	"FaceOutline4!",
	"FaceOutline5!",
	"FaceOutline6!",
	"Chin",
	"FaceOutline8!",
	"FaceOutline9!",
	"FaceOutline10!",
	"FaceOutline11!",
	"FaceOutline12!",
	"FaceOutline13!",
	"FaceOutline14!",
	"FaceOutline15!",
	"LeftEarTop",

	//
	"RightEyebrowOuter",
	"RightEyebrow1!",
	"RightEyebrow2",
	"RightEyebrow3!",
	"RightEyebrowInner",

	"LeftEyebrowInner",
	"LeftEyebrow3!",
	"LeftEyebrow2",
	"LeftEyebrow1!",
	"LeftEyebrowOuter",

	"NoseTop",
	"Nose1!",
	"Nose2!",
	"Nose3!",
	"NoseRight",
	"NoseMidRight!",
	"Nose",
	"NoseMidLeft!",
	"NoseLeft",

	"EyeRight_Outer",
	"EyeRight_TopOuter",
	"EyeRight_TopInner",
	"EyeRight_Inner",
	"EyeRight_BottomInner",
	"EyeRight_BottomOuter",

	"EyeLeft_Inner",
	"EyeLeft_TopInner",
	"EyeLeft_TopOuter",
	"EyeLeft_Outer",
	"EyeLeft_BottomOuter",
	"EyeLeft_BottomInner",

	"MouthRight",
	"Mouth1!",
	"Mouth2!",
	"MouthTop",
	"Mouth4!",
	"Mouth5!",
	"MouthLeft",

	"Mouth7!",
	"Mouth8!",
	"Mouth9!",
	"MouthBottom",
	"Mouth11!",
	"Mouth12!",
	"Mouth13!",

	"TeethTopRight!",
	"TeethTopMiddle!",
	"TeethTopLeft!",
	"TeethBottomRight!",
	"TeethBottomMiddle!",
	"TeethBottomLeft!",
 
];
if ( FaceLandMarkNames.length != 68 )
	throw "FaceLandMarkNames should have 68 entries, not " + FaceLandMarkNames.length;



//	get json, but in the original keypoint format, so that unity can still process skeleton from posenet
function GetSkeletonJson(Skeleton,Pretty,KeypointCountArray)
{
	let KeypointSkeleton = {};
	
	if ( Skeleton == null )
	{
		KeypointSkeleton.score = 0;
	}
	else
	{
		//	gr: todo: copy other keys automatically
		//KeypointSkeleton.score = 0.45789;
		KeypointSkeleton.Time = Skeleton.Time;
		KeypointSkeleton.FaceRect = Skeleton.FaceRect;
		KeypointSkeleton.keypoints = [];
		
		//	convert any position to a keypoint
		let PushKeypoint = function(Name)
		{
			if ( Name.includes("!") || Name == "FaceRect" )
				return;
			let Pos = Skeleton[Name];
			if ( !Pos || Pos.x === undefined )
				return;
			
			let Score = KeypointSkeleton.score;

			if ( FlipOutputSkeleton )
			{
				//	gotta copy, not modify orig
				let NewPosY = 1-Pos.y;
				Pos = { x:Pos.x, y:NewPosY };
			}
			let Keypoint = { part:Name, position:Pos, score:Score };
			
			KeypointSkeleton.keypoints.push(Keypoint);
			KeypointCountArray[0]++;
		}
		let Keys = Object.keys(Skeleton);
		Keys.forEach( PushKeypoint );
	}
	
	let Json = Pretty ? JSON.stringify( KeypointSkeleton, null, '\t' ) : JSON.stringify( KeypointSkeleton );
	return Json;
}


function OnOutputSkeleton(Skeleton,Image)
{
	//	try and free unused memory manually
	if ( OutputImage != null && OutputImage != Image )
		OutputImage.Clear();
	OutputImage = Image;
	OutputSkeleton = Skeleton;
	
	if ( OutputSkeleton == null )
		return;
	
	if ( OutputSkipFirstFrames-- > 0 )
		return;
	
	//	nowhere to output
	let SaveFilename = OutputFilename;
	if ( !SaveFilename && !ServerSkeletonSender )
		return;
	
	OutputSkeleton.Time = /*Image.Time || */Date.now();
	
	let Pretty = false;
	let KeypointCount = [0];
	let Json = GetSkeletonJson(OutputSkeleton,Pretty,KeypointCount) + "\n";

	if ( KeypointCount[0] == 0 )
	{
		Debug("No keypoints");
		return;
	}
	
	if ( SaveFilename )
	{
		try
		{
			WriteStringToFile( SaveFilename, Json, true );
		}
		catch(e)
		{
			Debug("Failed to write to file " + e);
		}
	}
	
	if ( ServerSkeletonSender )
	{
		try
		{
			let Peers = ServerSkeletonSender.GetPeers();
			
			if ( Peers.length > 0 )
			{
				//Debug("Sending FaceJson to x" + Peers.length + " peers on socket " + ServerSkeletonSender.GetAddress() );
			}
			
			let SendToPeer = function(Peer)
			{
				try
				{
					ServerSkeletonSender.Send( Peer, Json );
				}
				catch(e)
				{
					Debug("Failed to send to "+Peer+": " + e);
				}
			}
			Peers.forEach( SendToPeer );
		}
		catch(e)
		{
			Debug("Failed to write to stream out " + e);
		}
	}
}


function GetDefaultSkeleton(FaceRect)
{
	let Skeleton = {};
	Skeleton.FaceRect = FaceRect;
	return Skeleton;
}


if ( EnableKalmanFilter )
{
	include('KalmanFilter.js');
	var KalmanFilters = {};
}

function UpdateKalmanFilter(Name,NewValue,TightNoise)
{
	if ( !EnableKalmanFilter )
		return NewValue;
	
	TightNoise = TightNoise === true;
	let Noise = TightNoise ? [0.10,0.99] : [0.20,0.20];
	
	if ( KalmanFilters[Name] === undefined )
	{
		KalmanFilters[Name] = new KalmanFilter( NewValue, Noise[0], Noise[1] );
	}
	
	let Filter = KalmanFilters[Name];
	Filter.Push( NewValue );
	let v = NewValue;
	NewValue = Filter.GetEstimatedPosition(0);
	//Debug( Name + ": " + v + " -> " + NewValue );
	return NewValue;
}


function OnNewFace(FaceLandmarks,FaceRect,ClipRect,Image,Skeleton)
{
	UpdateFrameCounter('NewFace');
	
	//	handle no-face
	if ( FaceLandmarks == null )
	{
		OnOutputSkeleton( Skeleton, Image );
		return;
	}

	//	if no skeleton, make one
	if ( !Skeleton )
	{
		Debug("Making default skeleton");
		Skeleton = GetDefaultSkeleton(FaceRect);
		
		if ( !LastSkeleton )
		{
			Debug("Default skeleton is now LastSkeleton for face rect base");
			LastSkeleton = Skeleton;
		}
	}
	
	//	update rect
	Skeleton.FaceRect = FaceRect;
	Skeleton.ClipRect = ClipRect;

	let PushFeature;
	
	if ( FilterAroundNose )
	{
		//	filter nose
		let NoseIndex = FaceLandMarkNames.indexOf("Nose");
		let Nosex = FaceLandmarks[ (NoseIndex*2)+0 ];
		let Nosey = FaceLandmarks[ (NoseIndex*2)+1 ];
		Nosex = UpdateKalmanFilter( "BaseNoseX", Nosex, true );
		Nosey = UpdateKalmanFilter( "BaseNoseY", Nosey, true );
		
		PushFeature = function(Name,fx,fy)
		{
			fx -= Nosex;
			fy -= Nosey;
			fx = UpdateKalmanFilter( Name+"_x", fx, false );
			fy = UpdateKalmanFilter( Name+"_y", fy, false );
			fx += Nosex;
			fy += Nosey;
			Skeleton[Name] = { x:fx, y:fy };
		}
	}
	else
	{
		PushFeature = function(Name,fx,fy)
		{
			fx = UpdateKalmanFilter( Name+"_x", fx );
			fy = UpdateKalmanFilter( Name+"_y", fy );
			Skeleton[Name] = { x:fx, y:fy };
		}
	}
	
	for ( let i=0;	i<FaceLandmarks.length;	i+=2 )
	{
		let FeatureName = FaceLandMarkNames[i/2];
		PushFeature( FeatureName, FaceLandmarks[i+0], FaceLandmarks[i+1] );
	}
	
	OnOutputSkeleton( Skeleton, Image );
}

function EnumDevices(DeviceNames)
{
	let EnumDevice = function(DeviceName)
	{
		Debug(DeviceName);
	}
	DeviceNames.forEach( EnumDevice );
}

var DlibLandMarksdat = LoadFileAsArrayBuffer('shape_predictor_68_face_landmarks.dat');
var FaceProcessor = null;
var CurrentProcessingImageCount = 0;


function GetSkeletonFaceRect(Skeleton)
{
	if ( !Skeleton )
		return null;
	
	//	gr: ears are unreliable, get head size from shoulders
	//		which is a shame as we basically need ear to ear size
	let Nose = Skeleton.nose;
	let Left = Skeleton.leftShoulder;
	let Right = Skeleton.rightShoulder;
	if ( !Nose || !Left || !Right )
		return null;
	
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
	
	let Rect = [x,y,Width,Height];
	return Rect;
}

var AlwaysThisFrame = null;

function OnNewFrame(NewFrameImage,FindFaceIfNoSkeleton,Skeleton,OpenglContext)
{
	UpdateFrameCounter('CameraFrameRate');

	if ( AlwaysScanSameFrame === true )
	{
		if ( AlwaysThisFrame == null && OutputImage != null )
		{
			AlwaysThisFrame = new Image();
			AlwaysThisFrame.Copy(OutputImage);
		}
	}
	
	if( AlwaysThisFrame != null )
	{
		NewFrameImage.Copy(AlwaysThisFrame);
	}
	
	if ( OutputImage == null )
		OutputImage = NewFrameImage;
	
	NewFrameImage.Timestamp = Date.now();
	
	//	temp work throttler
	if ( CurrentProcessingImageCount > DlibThreadCount )
	{
		NewFrameImage.Clear();
		return;
	}
	//Debug("Now processing image " + NewFrameImage.GetWidth() + "x" + NewFrameImage.GetHeight() );
	
	let OnFaceError = function(Error)
	{
		Debug("Failed to get facelandmarks: " + Error);
		CurrentProcessingImageCount--;
		OnNewFace(null,null,null,NewFrameImage,Skeleton);
	}

	let OnFace = function(Face,Image,ClipRect)
	{
		//Debug("ClipRect=" + ClipRect);
		//	get the inverse rect
		let Unnormalise = function(x,y)
		{
			let cx = x;
			let cy = y;
			let ix = Lerp( ClipRect[0], ClipRect[0]+ClipRect[2], cx );
			let iy = Lerp( ClipRect[1], ClipRect[1]+ClipRect[3], cy );
			return [ix,iy];
		}
		
		let UnnormaliseRect = function(Rect)
		{
			let tl = Unnormalise( Rect[0], Rect[1] );
			let br = Unnormalise( Rect[0]+Rect[2], Rect[1]+Rect[3] );
			Rect[0] = tl[0];
			Rect[1] = tl[1];
			Rect[2] = br[0] - tl[0];
			Rect[3] = br[1] - tl[1];
		}
		
		
		CurrentProcessingImageCount--;
		//Debug("OnFace: " + typeof Face );
		if ( Face.length == 0 )
			Face = null;
		
		let FaceRect = null;
		
		//	need to put uv's back to image space
		if ( Face != null )
		{
			FaceRect = [ Face.shift(), Face.shift(), Face.shift(), Face.shift() ];
			UnnormaliseRect( FaceRect );
			for ( let i=0;	i<Face.length;	i+=2 )
			{
				let xy = Unnormalise( Face[i+0], Face[i+1] );
				Face[i+0] = xy[0];
				Face[i+1] = xy[1];
			}
		}
		OnNewFace(Face,FaceRect,ClipRect,Image,Skeleton);
	}
	

	
	//	load on first use
	if ( FaceProcessor == null )
		FaceProcessor = new Dlib( DlibLandMarksdat, DlibThreadCount );

	try
	{
		let FaceRect = Skeleton ? Skeleton.FaceRect : null;
		let ClipRect = null;
		
		if ( AlwaysFindFaceRect )
		{
			if ( FaceRect && FindFaceAroundLastFaceRect )
			{
				ClipRect = ScaleRect( FaceRect, FindFaceAroundLastFaceRectScale );
				ClampRect01( ClipRect );
			}
			FaceRect = null;
		}
		
		CurrentProcessingImageCount++;

		let ResizePromise = null;
		let SmallImage = null;
		let SmallImageWidth = NewFrameImage.GetWidth();
		let SmallImageHeight = NewFrameImage.GetHeight();
		let HeightRatio = SmallImageHeight / SmallImageWidth;
		SmallImageWidth = 640;
		SmallImageHeight = SmallImageWidth * HeightRatio;
		
		//	the face search looks for 80x80 size faces
		if ( !FaceRect )
		{
			if ( ClipRect == null )
			{
				SmallImageWidth = 500;
				SmallImageHeight = SmallImageWidth * HeightRatio;
			}
			else
			{
				//ClipRect[3] = ClipRect[2] * HeightRatio;
				SmallImageWidth = (NewFrameImage.GetWidth() * ClippedImageScale) * ClipRect[2];
				SmallImageHeight = (NewFrameImage.GetHeight() * ClippedImageScale) * ClipRect[3];
			}
		}
		
		if ( ClipRect == null )
			ClipRect = [0,0,1,1];
		
		if ( OpenglContext )
		{
			let ResizeRender = function(RenderTarget,RenderTargetTexture)
			{
				if ( !ResizeFragShader )
				{
					ResizeFragShader = new OpenglShader( RenderTarget, VertShaderSource, ResizeFragShaderSource );
				}
				
				let SetUniforms = function(Shader)
				{
					Shader.SetUniform("ClipRect", ClipRect );
					Shader.SetUniform("Source", NewFrameImage, 0 );
				}
				RenderTarget.DrawQuad( ResizeFragShader, SetUniforms );
				NewFrameImage.Clear();
			}
			SmallImage = new Image( [SmallImageWidth, SmallImageHeight] );
			SmallImage.SetLinearFilter(true);
			//Debug("searching SmallImage.width=" + SmallImage.GetWidth() + " SmallImage.height=" + SmallImage.GetHeight() );
			let ReadBackPixels = true;
			ResizePromise = OpenglContext.Render( SmallImage, ResizeRender, ReadBackPixels );
		}
		else
		{
			let ResizeCpu = function(Resolve,Reject)
			{
				try
				{
					SmallImage = new Image();
					SmallImage.Copy(NewFrameImage);
					NewFrameImage.Clear();
					SmallImage.Resize( SmallImageWidth, SmallImageHeight );
					Resolve();
				}
				catch(e)
				{
					Debug(e);
					Reject();
				}
			}
			ResizePromise = new Promise( ResizeCpu );
		}
		
		let GetFindFacePromise = function()
		{
			let FindFacePromise;
	
			if ( FaceRect )
			{
				FindFacePromise = FaceProcessor.FindFaceFeatures( SmallImage, FaceRect );
				return FindFacePromise;
			}
			else if ( FindFaceIfNoSkeleton )
			{
				FindFacePromise = FaceProcessor.FindFaces( SmallImage );
				return FindFacePromise;
			}
			else
			{
				Debug("throwing");
				throw "Waiting for face rect; FindFaceIfNoSkeleton=" + FindFaceIfNoSkeleton;
			}
		}
		
		ResizePromise
		.then( GetFindFacePromise )
		.then( function(f){	OnFace(f,SmallImage,ClipRect); } )
		.catch( OnFaceError );
	}
	catch(e)
	{
		CurrentProcessingImageCount--;

		Debug("Error setting up promise chain: " + e );
	}
}

function GetDeviceNameMatch(DeviceNames,MatchName)
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


function OnBroadcastMessage(PacketBytes,Sender,Socket)
{
	Debug("Got UDP broadcast x" + PacketBytes.length + " bytes");
	
	//	get string from bytes
	let PacketString = String.fromCharCode.apply(null, PacketBytes);
	Debug(PacketString);
	
	//	reply
	if ( ServerSkeletonSender && PacketString == "whereisobserverserver" )
	{
		//	get all addresses and filter best one (ie, ignore local host)
		let Addresses = ServerSkeletonSender.GetAddress().split(',');
		if ( Addresses.length > 1 )
		{
			let IsNotLocalhost = function(Address)
			{
				return !Address.startsWith("127.0.0.1:");
			}
			Addresses = Addresses.filter( IsNotLocalhost );
		}
		let Address = Addresses[0];
			
		//Debug("Send back [" + Address + "]" );
		
		//	udp needs a binary array, we'll make c++ more flexible later
		let Address8 = new Uint8Array(Address.length);
		for ( let i=0;	i<Address.length;	i++ )
			Address8[i] = Address.charCodeAt(i);
		Socket.Send( Sender, Address8 );
	}
}


function OnSkeletonJson(SkeletonJson)
{
	//Debug("Got skeleton: " + SkeletonJson);
	let Skeleton = JSON.parse(SkeletonJson);
	let MinScore = 0.3;
	
	let GetKeypointPos = function(Name)
	{
		let FindKeypointPart = function(Keypoint)
		{
			if ( Keypoint.score < MinScore )
				return false;
			return Keypoint.part == Name;
		};
		let Keypoints = Skeleton.keypoints;
		let kp = Keypoints.find( FindKeypointPart );
		if ( kp === undefined )
		{
			//Debug("Failed to find keypoint " + Name);
			return undefined;
		}
		if ( FlipInputSkeleton )
			kp.position.y = 1 - kp.position.y;
		return kp.position;
	}
	
	let SimpleSkeleton = {};
	SimpleSkeleton.Timestamp = Date.now();
	
	let PushKeypoint = function(Name)
	{
		let Pos = GetKeypointPos(Name);
		if ( Pos == undefined )
			return;
		
		//	capitalise name
		SimpleSkeleton[Name] = Pos;
	}
	PushKeypoint('nose');
	PushKeypoint('leftEye');
	PushKeypoint('rightEye');
	PushKeypoint('leftEar');
	PushKeypoint('rightEar');
	PushKeypoint('leftShoulder');
	PushKeypoint('rightShoulder');
	PushKeypoint('leftElbow');
	PushKeypoint('rightElbow');
	PushKeypoint('leftWrist');
	PushKeypoint('rightWrist');
	PushKeypoint('leftHip');
	PushKeypoint('rightHip');
	PushKeypoint('leftKnee');
	PushKeypoint('rightKnee');
	PushKeypoint('leftAnkle');
	PushKeypoint('rightAnkle');
	
	SimpleSkeleton.FaceRect = GetSkeletonFaceRect(SimpleSkeleton);
	
	LastSkeleton = SimpleSkeleton;
	if ( OutputSkeleton == null )
		OutputSkeleton = LastSkeleton;
}

function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("PopTrack4");
	Window1.OnRender = function(){	WindowRender(Window1);	};
	

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
		
			let VideoCapture = new MediaSource(VideoDeviceName);
			let FindFaceIfNoSkeleton = true;
			let OpenglContext = Window1;
			VideoCapture.OnNewFrame = function(img)	{	OnNewFrame(img,FindFaceIfNoSkeleton,LastSkeleton,OpenglContext);	};
		}
		catch(e)
		{
			Debug(e);
		}

	}

	/*
	//let TestImage = new Image('NataliePortman.jpg');
	//let TestImage = new Image('MicTest1.png');
	//let TestImage = new Image('MicTest2.png');
	let TestImage = new Image('MicTest3.png');
	
	//let TestImage = new Image('Face.png');
	//let TestImage = new Image('FaceLeft.jpg');
	//OnNewFrame(TestImage,'Face.json');
	OnNewFrame(TestImage,null,true);
	return;
	//TestImage = null;
	//GarbageCollect();
	 */
	
	let MediaDevices = new Media();
	MediaDevices.EnumDevices().then( LoadDevice );


	let AllocSkeletonReciever = function()
	{
		ServerSkeletonReciever = new WebsocketServer(ServerSkeletonRecieverPort);
		ServerSkeletonReciever.OnMessage = OnSkeletonJson;
	}
	
	let AllocSkeletonSender = function()
	{
		ServerSkeletonSender = new WebsocketServer(ServerSkeletonSenderPort);
	}
	
	let AllocBroadcastServer = function()
	{
		BroadcastServer = new UdpBroadcastServer(BroadcastServerPort);
		BroadcastServer.OnMessage = function(Data,Sender)	{	OnBroadcastMessage(Data,Sender,BroadcastServer);	}
	}
	
	let AllocWebServer = function()
	{
		WebServer = new HttpServer(WebServerPort);
	}


	let Retry = function(RetryFunc,Timeout)
	{
		let RetryAgain = function(){	Retry(RetryFunc,Timeout);	};
		try
		{
			RetryFunc();
		}
		catch(e)
		{
			Debug(e+"... retrying in " + Timeout);
			setTimeout( RetryAgain, Timeout );
		}
	}
	Retry( AllocSkeletonReciever, 1000 );
	Retry( AllocSkeletonSender, 1000 );
	Retry( AllocBroadcastServer, 1000 );
	Retry( AllocWebServer, 1000 );

}

//	main
Main();
