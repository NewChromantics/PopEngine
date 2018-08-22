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
var PoseNetScale = 0.40;
var PoseNetOutputStride = 16;
var PoseNetMirror = false;
//var outputStride = 32;
//var ClipToSquare = false;
//var ClipToSquare = true;
var ClipToSquare = 800;
var EnableGpuClip = true;
var ClipToGreyscale = true;	//	GPU only! shader option
var ApplyBlurInClip = false;

var FindFaceAroundLastHeadRectScale = 1.1;	//	make this expand more width ways
var BlurLandmarkSearch = false;
var ShoulderToHeadWidthRatio = 0.8;
var HeadWidthToHeightRatio = 2.4;
var NoseHeightInHead = 0.5;

var ResizeFragShaderSource = LoadFileAsString("GreyscaleToRgb.frag");
var ResizeFragShader = null;
var DrawSmallImage = false;
var DrawRects = false;
var DrawSkeletonMinScore = 0.5;

var CurrentFrames = [];
var LastFrame = null;	//	completed TFrame



var DlibLandMarksdat = LoadFileAsArrayBuffer('shape_predictor_68_face_landmarks.dat');
var DlibThreadCount = 2;
var FaceProcessor = null;
var MaxConcurrentFrames = DlibThreadCount;
var SmallImageSize = 80 * 3;
var SmallImageSquare = true;
var FailIfNoFace = true;



var ServerSkeletonSender = null;
var ServerSkeletonSenderPort = 8007;
var BroadcastServer = null;
var BroadcastServerPort = 8009;
var OutputFilename = "../../../../SkeletonOutputFrames.json";
//var OutputFilename = null;
var FlipOutputSkeleton = true;
var MirrorOutputSkeleton = false;


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

function ClipRect01(Rect)
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


function ClipRect(ChildRect,ParentRect)
{
	let cl = ChildRect[0];
	let ct = ChildRect[1];
	let cr = cl + ChildRect[2];
	let cb = ct + ChildRect[3];
	
	let pl = ParentRect[0];
	let pt = ParentRect[1];
	let pr = pl + ParentRect[2];
	let pb = pt + ParentRect[3];

	cl = Clamp( pl,pr,cl );
	ct = Clamp( pt,pb,ct );
	cr = Clamp( pl,pr,cr );
	cb = Clamp( pt,pb,cb );
	
	ChildRect[0] = cl;
	ChildRect[1] = ct;
	ChildRect[2] = cr-cl;
	ChildRect[3] = cb-ct;
}


function ClampRect(ChildRect,ParentRect)
{
	let cl = ChildRect[0];
	let ct = ChildRect[1];
	let cr = cl + ChildRect[2];
	let cb = ct + ChildRect[3];
	
	let pl = ParentRect[0];
	let pt = ParentRect[1];
	let pr = pl + ParentRect[2];
	let pb = pt + ParentRect[3];
	
	//	move to stick inside
	if ( cl < pl )
	{
		let Shift = pl - cl;
		cl += Shift;
		cr += Shift;
	}
	if ( ct < pt )
	{
		let Shift = pt - ct;
		ct += Shift;
		cb += Shift;
	}
	if ( cr > pr )
	{
		let Shift = pr - cr;
		cl += Shift;
		cr += Shift;
	}
	if ( cb > pb )
	{
		let Shift = pb - cb;
		ct += Shift;
		cb += Shift;
	}
	
	//	THEN still clamp
	cl = Clamp( pl,pr,cl );
	ct = Clamp( pt,pb,ct );
	cr = Clamp( pl,pr,cr );
	cb = Clamp( pt,pb,cb );
	
	
	ChildRect[0] = cl;
	ChildRect[1] = ct;
	ChildRect[2] = cr-cl;
	ChildRect[3] = cb-ct;
	//Debug(ChildRect);
}

function UnnormaliseRect(ChildRect,ParentRect)
{
	//	ChildRect is normalised, so put it in parent rect space
	//	Soy::Rectx<TYPE>::ScaleTo
	let pl = ParentRect[0];
	let pr = pl + ParentRect[2];
	let pt = ParentRect[1];
	let pb = pt + ParentRect[3];
	
	let cl = ChildRect[0];
	let cr = cl + ChildRect[2];
	let ct = ChildRect[1];
	let cb = ct + ChildRect[3];

	
	let l = Lerp( pl, pr, cl );
	let r = Lerp( pl, pr, cr );
	let t = Lerp( pt, pb, ct );
	let b = Lerp( pt, pb, cb );
	let w = r-l;
	let h = b-t;
	ChildRect[0] = l;
	ChildRect[1] = t;
	ChildRect[2] = w;
	ChildRect[3] = h;
}

function UnnormalisePoint(x,y,ParentRect)
{
	//	ChildRect is normalised, so put it in parent rect space
	//	Soy::Rectx<TYPE>::ScaleTo
	let pl = ParentRect[0];
	let pr = pl + ParentRect[2];
	let pt = ParentRect[1];
	let pb = pt + ParentRect[3];
	
	let cl = x;
	let ct = y;
	
	let l = Lerp( pl, pr, cl );
	let t = Lerp( pt, pb, ct );

	return [l,t];
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

function SetRectSizeAlignMiddle(Rect,Width,Height)
{
	let ChangeX = Rect[2] - Width;
	Rect[0] += ChangeX/2;
	Rect[2] -= ChangeX;
	
	let ChangeY = Rect[3] - Height;
	Rect[1] += ChangeY/2;
	Rect[3] -= ChangeY;
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
		if ( Score < DrawSkeletonMinScore )
			return;
		
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


function GetPointLinesAndScores(Points,Lines,Scores,Normalise,Score)
{
	if ( !Points )
		return;
	
	let PushX = function(xy)
	{
		Lines.push( [xy,xy] );
		Scores.push( Score );
	}

	Points.forEach( PushX );
}


function GetRectLines(Rect,Lines,Scores,Normalise,Score)
{
	if ( !Rect )
		return;
	
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
	this.OriginalImage = null;	//	if we clipped image on gpu for posenet, the original gets stored here
	this.SmallImage = null;		//	face search image (clipped image)
	this.FaceFeatures = null;	//	normalised to image(0..1)
	this.SkeletonPose = null;	//	image space(px)
	this.ImageData = null;
	this.OpenglContext = OpenglContext;
	
	//	rects are in Image space(px)
	this.HeadRect = [0,0,1,1];	//	head area on skeleton
	this.FaceRect = null;	//	detected face
	this.ClipRect = [0,0,1,1];	//	small image clip rect. Normalised to image(0..1)
	
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
		if ( this.OriginalImage )
		{
			this.OriginalImage.Clear();
			this.OriginalImage = null;
		}
		
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
		let AlreadyNormalised = function(x,y)
		{
			return [x,y];
		}
		
		if ( DrawRects )
		{
			GetRectLines( this.HeadRect, Lines, Scores, Normalise, 0 );
			GetRectLines( this.ClipRect, Lines, Scores, AlreadyNormalised, 1.5 );
			GetRectLines( this.FaceRect, Lines, Scores, AlreadyNormalised, 0.5 );
		}
		GetPoseLinesAndScores( this.SkeletonPose, Lines, Scores, Normalise );
		GetPointLinesAndScores( this.FaceFeatures, Lines, Scores, AlreadyNormalised, 1 );
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
		return this.SkeletonPose.keypoints.find( IsMatch );
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
	
	
	this.SetFaceLandmarks = function(FaceLandMarks)
	{
		if ( FaceLandMarks.length == 0 )
		{
			if ( FailIfNoFace )
				throw "No face found";

			//Debug("No face found");
			this.FaceRect = null;
			this.FaceFeatures = null;
			return;
		}
		
		//Debug("SetFaceLandmarks(x" + FaceLandMarks.length + ")");
		this.FaceRect = [];
		//	first four are the found-face rect
		this.FaceRect[0] = FaceLandMarks.shift();
		this.FaceRect[1] = FaceLandMarks.shift();
		this.FaceRect[2] = FaceLandMarks.shift();
		this.FaceRect[3] = FaceLandMarks.shift();
		
		UnnormaliseRect( this.FaceRect, this.ClipRect );
		
		this.FaceFeatures = [];
		for ( let ff=0;	ff<FaceLandMarks.length;	ff+=2 )
		{
			let fx = FaceLandMarks[ff+0];
			let fy = FaceLandMarks[ff+1];
			this.FaceFeatures.push( UnnormalisePoint( fx,fy, this.ClipRect ) );
		}
		
	}
	
	this.EnumKeypoints = function(EnumNamePosScore)
	{
		let Width = this.GetWidth();
		let Height = this.GetHeight();
		let Normalise = function(px,py)
		{
			px /= Width;
			py /= Height;
			return { x:px, y:py };
		}
		
		let EnumKeypoint = function(Keypoint)
		{
			//	gr: sending pose here is mutable!
			EnumNamePosScore( Keypoint.part, Normalise(Keypoint.position.x,Keypoint.position.y), Keypoint.score );
		}
		this.SkeletonPose.keypoints.forEach( EnumKeypoint );
		
		if ( this.FaceFeatures != null )
		{
			for ( let ff=0;	ff<this.FaceFeatures.length;	ff++)
			{
				let Name = FaceLandMarkNames[ff];
				let fx = this.FaceFeatures[ff][0];
				let fy = this.FaceFeatures[ff][1];
				let Pos = { x:fx, y:fy };
				let Score = 0.5;
				EnumNamePosScore( Name, Pos, Score );
			}
			
		}
		
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
	
	if ( CurrentFrames.length >= MaxConcurrentFrames )
	{
		//Debug("Waiting on " + CurrentFrames.length + " frames");
		return false;
	}
	
	return true;
}

function OnFrameCompleted(Frame)
{
	UpdateFrameCounter('FrameCompleted');
	
	CurrentFrames = CurrentFrames.filter( function(el)	{	return el!=Frame;	} );
	
	if ( LastFrame != null )
		LastFrame.Clear();
	LastFrame = Frame;
	
	try
	{
		OutputFrame(LastFrame);
	}
	catch(e)
	{
		Debug("Error outputting frame: " + e);
	}
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
	if ( Frame.OpenglContext && EnableGpuClip )
	{
		//	make it square
		let ClipRect = Frame.GetImageRect();
		let Size = Math.min( ClipRect[2], ClipRect[3] );
		SetRectSizeAlignMiddle( ClipRect, Size, Size );
		NormaliseRect( ClipRect, Frame.GetImageRect() );
		
		if ( typeof ClipToSquare == "number" )
			Size = ClipToSquare;
		
		//	gr: here we can do some custom filtering! up the contrast, greenscreen etc
		let ResizeRender = function(RenderTarget,RenderTargetTexture)
		{
			if ( !ResizeFragShader )
			{
				ResizeFragShader = new OpenglShader( RenderTarget, VertShaderSource, ResizeFragShaderSource );
			}
			
			let SetUniforms = function(Shader)
			{
				Shader.SetUniform("ClipRect", ClipRect );
				Shader.SetUniform("Source", Frame.OriginalImage, 0 );
				Shader.SetUniform("ApplyBlur", ApplyBlurInClip );
				Shader.SetUniform("OutputGreyscale", ClipToGreyscale );
			}
			RenderTarget.DrawQuad( ResizeFragShader, SetUniforms );
		}
		
		Frame.OriginalImage = Frame.Image;
		Frame.Image = new Image( [Size, Size] );
		Frame.Image.SetLinearFilter(true);
		let ReadBackPixels = true;
		
		let ResizePromise = Frame.OpenglContext.Render( Frame.Image, ResizeRender, ReadBackPixels );
		return ResizePromise;
	}
	else
	{
		let Runner = function(Resolve,Reject)
		{
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
			
			Resolve(Frame);
		}
		return new Promise(Runner);
	}
}


function GetPoseDetectionPromise(Frame)
{
	let Runner = function(Resolve,Reject)
	{
		Frame.SetupImageData();
		
		let OnPose = function(Pose)
		{
			Frame.SkeletonPose = Pose;
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

	//	resize down to 80x80 (or a multiple?)
	//	gr: decide here if we should blur (if we're going up or down maybe)
	//	gr: should be square?
	
	if ( SmallImageSquare )
	{
		let Size = Math.min( Frame.ClipRect[2], Frame.ClipRect[3] );
		Size = Math.max( SmallImageSize, Size );
		
		SetRectSizeAlignMiddle( Frame.ClipRect, Size, Size );
	}
	else
	{
		Frame.ClipRect[2] = Math.max( SmallImageSize, Frame.ClipRect[2] );
		Frame.ClipRect[3] = Math.max( SmallImageSize, Frame.ClipRect[3] );
	}
	ClampRect( Frame.ClipRect, Frame.GetImageRect() );
	let Scale = SmallImageSize / Frame.ClipRect[2];
	
	let SmallImageWidth = Frame.ClipRect[2] * Scale;
	let SmallImageHeight = Frame.ClipRect[3] * Scale;
	
	NormaliseRect( Frame.ClipRect, Frame.GetImageRect() );
	//Debug("SmallImage="+ SmallImageWidth+"x"+SmallImageHeight+ " Frame=" + Frame.GetWidth()+"x"+Frame.GetHeight() + " ClippedImageScale=" + ClippedImageScale + " Frame.ClipRect[2]x[3]=" + Frame.ClipRect[2]+"x"+Frame.ClipRect[3]);
	
	//	return a resizing promise
	if ( Frame.OpenglContext )
	{
		let ResizeRender = function(RenderTarget,RenderTargetTexture)
		{
			//Debug("ResizeRender Frame=" + Frame);

			if ( !ResizeFragShader )
			{
				ResizeFragShader = new OpenglShader( RenderTarget, VertShaderSource, ResizeFragShaderSource );
			}
				
			let SetUniforms = function(Shader)
			{
				Shader.SetUniform("ClipRect", Frame.ClipRect );
				Shader.SetUniform("Source", Frame.Image, 0 );
				Shader.SetUniform("ApplyBlur", BlurLandmarkSearch );
				Shader.SetUniform("OutputGreyscale", true );
			}
			RenderTarget.DrawQuad( ResizeFragShader, SetUniforms );
		}
		
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
	//Debug("GetFaceDetectionPromise on " + Frame.SmallImage );
	return FaceProcessor.FindFaces( Frame.SmallImage );
}





function OnNewVideoFrameFilter()
{
	UpdateFrameCounter('Webcam');
	//	filter if busy here
	return IsIdle();
}


function GetHandleNewFaceLandmarksPromise(Frame,Face)
{
	let Handle = function(Resolve,Reject)
	{
		Frame.SetFaceLandmarks(Face);
		OnFrameCompleted(Frame);
		Resolve();
	}
	
	return new Promise(Handle);
}

function GetSetupForFaceDetectionPromise(Frame)
{
	let Runner = function(Resolve,Reject)
	{
		throw "xxx";
		try
		{
			let FaceDetectionSetupPromise = SetupForFaceDetection(Frame);
			Resolve(FaceDetectionSetupPromise);
		}
		catch(e)
		{
			Debug("GetSetupForFaceDetectionPromise exception "+e);
			Reject(e);
		}
	}
	return new Promise( Runner );
}

function OnNewVideoFrame(FrameImage)
{
	if ( !IsIdle() )
	{
		FrameImage.Clear();
		//Debug("Skipped webcam image");
		return;
	}

	//	make a new frame
	let OpenglContext = window.OpenglContext;
	let Frame = new TFrame(OpenglContext);
	CurrentFrames.push( Frame );
	
	Frame.Image = FrameImage;

	SetupForPoseDetection( Frame )
	.then( function()			{	return GetPoseDetectionPromise(Frame);	}	)
	.then( SetupForFaceDetection )
	.then( function()			{	return GetFaceDetectionPromise(Frame);		}	)
	.then( function(NewFace)	{	return GetHandleNewFaceLandmarksPromise(Frame,NewFace);		}	)
	.catch( function(Error)		{	OnFrameError(Frame,Error);	}	);
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


function GetSkeletonJson(Frame,Pretty)
{
	let KeypointSkeleton = {};

	let EnumKeypoint = function(Name,Position,Score)
	{
		if ( Name.includes("!") )
			return;
		
		if ( FlipOutputSkeleton )
			Position.y = 1-Position.y;
		
		if ( MirrorOutputSkeleton )
			Position.x = 1-Position.x;

		let Keypoint = {};
		Keypoint.part = Name;
		Keypoint.position = Position;
		Keypoint.score = Score;
		KeypointSkeleton.keypoints.push( Keypoint );
	}

	//	other meta
	KeypointSkeleton.FaceRect = Frame.FaceRect;
	KeypointSkeleton.score = 0.4567;
	KeypointSkeleton.keypoints = [];

	//	get keypoints
	Frame.EnumKeypoints( EnumKeypoint );
	
	let Json = Pretty ? JSON.stringify( KeypointSkeleton, null, '\t' ) : JSON.stringify( KeypointSkeleton );
	return Json;
}


function OutputFrame(Frame)
{
	//	nowhere to output
	if ( !ServerSkeletonSender && !OutputFilename )
		return;

	//	need one line output if going to file
	let Pretty = OutputFilename ? false : true;
	let Json = GetSkeletonJson(Frame,Pretty) + "\n";

	if ( OutputFilename )
	{
		try
		{
			WriteStringToFile( OutputFilename, Json, true );
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


function LoadSockets()
{
	let AllocSkeletonSender = function()
	{
		ServerSkeletonSender = new WebsocketServer(ServerSkeletonSenderPort);
	}
	
	let AllocBroadcastServer = function()
	{
		BroadcastServer = new UdpBroadcastServer(BroadcastServerPort);
		BroadcastServer.OnMessage = function(Data,Sender)	{	OnBroadcastMessage(Data,Sender,BroadcastServer);	}
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
	
	Retry( AllocSkeletonSender, 1000 );
	Retry( AllocBroadcastServer, 1000 );
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
	LoadSockets();
}

//	main
Main();
