
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

let FrameFragShaderSource = LoadFileAsString("Data_dlib/DrawFrameAndPose.frag");
var FrameShader = null;
var LastFrameImage = null;
var LastFace = null;
var LastSkeleton = null;

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

function GetSkeletonLines(Skeleton)
{
	let Lines = [];

	if ( LastSkeletonFaceRect )
		Lines = Lines.concat( GetRectLines(LastSkeletonFaceRect) );
	
	if ( Skeleton == null )
		return Lines;
	
	let PushLine = function(namea,nameb)
	{
		let Posa = Skeleton[namea];
		let Posb = Skeleton[nameb];
		if ( Posa == undefined || Posb == undefined )
			return;
		
		Lines.push( Posa.x );
		Lines.push( Posa.y );
		Lines.push( Posb.x );
		Lines.push( Posb.y );
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
	
	return Lines;
}

function GetFeatureLines(Face)
{
	let Lines = [];
	
	if ( Face != null )
	{
		let FaceRect = Face.Rect;
		Lines.concat( GetRectLines(FaceRect) );
	}
	
	let LineOffset = 1 / 400;
	let PushFeatureLine = function(Feature)
	{
		let fx = Feature.x;
		let fy = Feature.y;
		let x0 = fx-LineOffset;
		let x1 = fx+LineOffset;
		let y0 = fy-LineOffset;
		let y1 = fy+LineOffset;
		Lines.push( [x0,y0,x1,y1] );
	}
	if ( Face != null )
		Face.Features.forEach( PushFeatureLine );

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
			
			let SkeletonLines = GetSkeletonLines( LastSkeleton );
			let PoseLines = GetFeatureLines(LastFace);
			
			PoseLines = SkeletonLines.concat( PoseLines );
			
			const MAX_LINES = 200;
			PoseLines.length = Math.min( PoseLines.length, MAX_LINES );
			
			Shader.SetUniform("Lines", PoseLines );
		}
		
		RenderTarget.DrawQuad( FrameShader, SetUniforms );
	}
	catch(Exception)
	{
		RenderTarget.ClearColour(1,0,0);
		Debug(Exception);
	}
}


function OnNewFace(FaceLandmarks,FaceImage,SaveFilename)
{
	LastFrameImage = FaceImage;
	
	//	make a face
	let Face = {};
	Face.Features = [];
	let PushFeature = function(fx,fy)
	{
		let f = { x:fx, y:fy };
		Face.Features.push( f );
	}
	
	//	first 4 floats are the rect
	Face.Rect = [ FaceLandmarks[0], FaceLandmarks[1], FaceLandmarks[2], FaceLandmarks[3] ];
	
	for ( let i=4;	i<FaceLandmarks.length;	i+=2 )
	{
		PushFeature( FaceLandmarks[i+0], FaceLandmarks[i+1] );
	}
	
	Debug("Got face: x" + Face.Features.length + " features" );
	
	if ( SaveFilename != undefined )
	{
		let FaceJson = JSON.stringify( Face, null, '\t' );
		WriteStringToFile( SaveFilename, FaceJson );
	}
	
	LastFace = Face;
}

function OnFailedNewFace(Error)
{
	Debug("Failed to get facelandmarks: " + Error);
}

function EnumDevices(DeviceNames)
{
	let EnumDevice = function(DeviceName)
	{
		Debug(DeviceName);
	}
	DeviceNames.forEach( EnumDevice );
}

var DlibThreadCount = 1;
var DlibLandMarksdat = LoadFileAsArrayBuffer('Data_Dlib/shape_predictor_68_face_landmarks.dat');
var FaceProcessor = null;
var CurrentProcessingImageCount = 0;

var LastSkeletonFaceRect = null;
var ShoulderToHeadWidthRatio = 0.45;
var HeadWidthToHeightRatio = 2.1;
var NoseHeightInHead = 0.5;


function OnNewFrame(NewFrameImage,SaveFilename)
{
	NewFrameImage.Timestamp = Date.now();
	
	//	temp work throttler
	if ( CurrentProcessingImageCount > DlibThreadCount )
		return;
	
	Debug("Now processing image " + NewFrameImage.GetWidth() + "x" + NewFrameImage.GetHeight() );
	
	let OnFace = function(Face)
	{
		CurrentProcessingImageCount--;
		OnNewFace(Face,NewFrameImage,SaveFilename);
	}
	
	if ( LastSkeletonFaceRect == null )
	{
		Debug("Waiting for LastSkeletonFaceRect");
		LastFrameImage = NewFrameImage;
		return;
	}

	//	load on first use
	if ( FaceProcessor == null )
		FaceProcessor = new Dlib( DlibLandMarksdat, DlibThreadCount );

	try
	{
		let FaceRect = LastSkeletonFaceRect;
		CurrentProcessingImageCount++;

		//	gr: this was silently throwing when FaceProcessor == null/undefined!
		//FaceProcessor.FindFaces(NewFrameImage)
		FaceProcessor.FindFaceFeatures( NewFrameImage, FaceRect )
		.then( OnFace )
		.catch( OnFailedNewFace );
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


var Server = null;
var ServerPort = 8008;
var BroadcastServer = null;
var BroadcastServerPort = 8009;


function OnBroadcastMessage(PacketBytes,Sender,Socket)
{
	Debug("Got UDP broadcast x" + PacketBytes.length + " bytes");

	//	get string from bytes
	let PacketString = String.fromCharCode.apply(null, PacketBytes);
	Debug(PacketString);
	
	//	reply
	//Socket.Send( Sender, Server.GetAddress() );
}


function OnSkeletonJson(SkeletonJson)
{
	Debug("Got skeleton: " + SkeletonJson);
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
	
	
	//	make a skeleton face rect
	{
		//	gr: ears are unreliable, get head size from shoulders
		//		which is a shame as we basically need ear to ear size
		let Nose = SimpleSkeleton.nose;
		let Left = SimpleSkeleton.leftShoulder;
		let Right = SimpleSkeleton.rightShoulder;
		if ( Nose && Left && Right )
		{
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
			LastSkeletonFaceRect = [x,y,Width,Height];
		}
	}
	
	
	LastSkeleton = SimpleSkeleton;
}

function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("dlib");
	Window1.OnRender = function(){	WindowRender(Window1);	};
	

	
	let VideoDeviceName = "Facetime";
	
	let LoadDevice = function(DeviceNames)
	{
		try
		{
			//	find best match name
			Debug("Got devices: x" + DeviceNames.length);
			Debug(DeviceNames);
			VideoDeviceName = GetDeviceNameMatch(DeviceNames,VideoDeviceName);
			Debug("Loading device: " + VideoDeviceName);
		
			let VideoCapture = new MediaSource(VideoDeviceName);
			VideoCapture.OnNewFrame = OnNewFrame;
		}
		catch(e)
		{
			Debug(e);
		}

	}
	
	let MediaDevices = new Media();
	MediaDevices.EnumDevices().then( LoadDevice );

	//let TestImage = new Image('Data_dlib/NataliePortman.jpg');
	//let TestImage = new Image('Data_dlib/Face.png');
	//let TestImage = new Image('Data_dlib/FaceLeft.jpg');
	//OnNewFrame(TestImage,'Data_dlib/Face.json');
	
	Server = new WebsocketServer(ServerPort);
	Server.OnMessage = OnSkeletonJson;
	
	BroadcastServer = new UdpBroadcastServer(BroadcastServerPort);
	BroadcastServer.OnMessage = OnBroadcastMessage;

	
}

//	main
Main();
