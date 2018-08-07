
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


//	skeleton + face
var OutputSkeleton = null;
var OutputImage = null;

//	last simple skeleton we recieved
var LastSkeleton = null;


var ServerSkeletonReciever = null;
var ServerSkeletonRecieverPort = 8008;
var ServerSkeletonSender = null;
var ServerSkeletonSenderPort = 8007;
var BroadcastServer = null;
var BroadcastServerPort = 8009;



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



function GetSkeletonLinesAndScores(Skeleton)
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

	//	make rect lines blue
	Lines = Lines.concat( GetRectLines(Skeleton.FaceRect) );
	Scores = Scores.concat( [9,9,9,9] );

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
			Shader.SetUniform("HasFrame", OutputImage!=null );
			
			let LinesAndScores = GetSkeletonLinesAndScores( OutputSkeleton );
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



//	todo: name properly
let FaceLandMarkNames =
[
 	//	right is actor-right, not image-right
	//	17 outline features
	"RightEarTop",
	"FaceOutline1",
	"FaceOutline2",
	"FaceOutline3",
	"FaceOutline4",
	"FaceOutline5",
	"FaceOutline6",
	"Chin",
	"FaceOutline8",
	"FaceOutline9",
	"FaceOutline10",
	"FaceOutline11",
	"FaceOutline12",
	"FaceOutline13",
	"FaceOutline14",
	"FaceOutline15",
	"LeftEarTop",
 
 	//
 "RightEyebrowOuter",
 "RightEyebrow1",
 "RightEyebrow2",
 "RightEyebrow3",
 "RightEyebrowInner",
 
 "LeftEyebrowInner",
 "LeftEyebrow3",
 "LeftEyebrow2",
 "LeftEyebrow1",
 "LeftEyebrowOuter",
 
 "NoseTop",
 "Nose1",
 "Nose2",
 "Nose3",
 "NoseRight",
 "NoseMidRight",
 "Nose",
 "NoseMidLeft",
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
 "Mouth1",
 "Mouth2",
 "MouthTop",
 "Mouth4",
 "Mouth5",
 "MouthLeft",
 
 "Mouth7",
 "Mouth8",
 "Mouth9",
 "MouthBottom",
 "Mouth11",
 "Mouth12",
 "Mouth13",

 "TeethTopRight",
 "TeethTopMiddle",
 "TeethTopLeft",
 "TeethBottomRight",
 "TeethBottomMiddle",
 "TeethBottomLeft",
 
];
if ( FaceLandMarkNames.length != 68 )
	throw "FaceLandMarkNames should have 68 entries, not " + FaceLandMarkNames.length;


function OnOutputSkeleton(Skeleton,Image,SaveFilename)
{
	OutputImage = Image;
	OutputSkeleton = Skeleton;
	
	if ( SaveFilename != undefined )
	{
		try
		{
			let Json = JSON.stringify( Skeleton, null, '\t' );
			WriteStringToFile( SaveFilename, Json );
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
			let Json = JSON.stringify( Skeleton, null, '\t' );
			let Peers = ServerSkeletonSender.GetPeers();
			
			if ( Peers.length > 0 )
			{
				Debug("Sending FaceJson to x" + Peers.length + " peers on socket " + ServerSkeletonSender.GetAddress() );
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


function OnNewFace(FaceLandmarks,Image,SaveFilename,Skeleton)
{
	//	handle no-face
	if ( FaceLandmarks == null )
	{
		OnOutputSkeleton( Skeleton, Image, SaveFilename );
		return;
	}
	
	//	first 4 floats are the rect
	let FaceRect = [ FaceLandmarks.shift(), FaceLandmarks.shift(), FaceLandmarks.shift(), FaceLandmarks.shift() ];

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
	
	let PushFeature = function(Name,fx,fy)
	{
		//Debug("Push feature: " + Name + " at " + fx + "," + fy);
		Skeleton[Name] = { x:fx, y:fy };
	}
	
	for ( let i=0;	i<FaceLandmarks.length;	i+=2 )
	{
		let FeatureName = FaceLandMarkNames[i/2];
		PushFeature( FeatureName, FaceLandmarks[i+0], FaceLandmarks[i+1] );
	}
	
	OnOutputSkeleton( Skeleton, Image, SaveFilename );
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

var ShoulderToHeadWidthRatio = 0.45;
var HeadWidthToHeightRatio = 2.1;
var NoseHeightInHead = 0.5;


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


function OnNewFrame(NewFrameImage,SaveFilename,FindFaceIfNoSkeleton,Skeleton)
{
	if ( OutputImage == null )
		OutputImage = NewFrameImage;
	
	NewFrameImage.Timestamp = Date.now();
	
	//	temp work throttler
	if ( CurrentProcessingImageCount > DlibThreadCount )
		return;
	
	//Debug("Now processing image " + NewFrameImage.GetWidth() + "x" + NewFrameImage.GetHeight() );
	
	let OnFaceError = function(Error)
	{
		Debug("Failed to get facelandmarks: " + Error);
		CurrentProcessingImageCount--;
		OnNewFace(null,NewFrameImage,SaveFilename,Skeleton);
	}

	let OnFace = function(Face)
	{
		CurrentProcessingImageCount--;
		if ( Face.length == 0 )
			Face = null;
		OnNewFace(Face,NewFrameImage,SaveFilename,Skeleton);
	}
	

	
	//	load on first use
	if ( FaceProcessor == null )
		FaceProcessor = new Dlib( DlibLandMarksdat, DlibThreadCount );

	try
	{
		let FaceRect = Skeleton ? Skeleton.FaceRect : null;
		CurrentProcessingImageCount++;

		let FindFacePromise;
		if ( FaceRect )
		{
			FindFacePromise = FaceProcessor.FindFaceFeatures( NewFrameImage, FaceRect );
		}
		else if ( FindFaceIfNoSkeleton )
		{
			FindFacePromise = FaceProcessor.FindFaces( NewFrameImage );
		}
		else
		{
			OnFaceError("Waiting for face rect; FindFaceIfNoSkeleton=" + FindFaceIfNoSkeleton);
			return;
		}
		
		FindFacePromise
		.then( OnFace )
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
	

	
	let VideoDeviceName = "isight";
	
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
			let FindFaceIfNoSkeleton = true;
			VideoCapture.OnNewFrame = function(img)	{	OnNewFrame(img,null,FindFaceIfNoSkeleton,LastSkeleton);	};
		}
		catch(e)
		{
			Debug(e);
		}

	}

	/*
	//let TestImage = new Image('Data_dlib/NataliePortman.jpg');
	//let TestImage = new Image('Data_dlib/MicTest1.png');
	//let TestImage = new Image('Data_dlib/MicTest2.png');
	let TestImage = new Image('Data_dlib/MicTest3.png');
	
	//let TestImage = new Image('Data_dlib/Face.png');
	//let TestImage = new Image('Data_dlib/FaceLeft.jpg');
	//OnNewFrame(TestImage,'Data_dlib/Face.json');
	OnNewFrame(TestImage,null,true);
	return;
	//TestImage = null;
	//GarbageCollect();
	 */
	
	let MediaDevices = new Media();
	MediaDevices.EnumDevices().then( LoadDevice );


	ServerSkeletonReciever = new WebsocketServer(ServerSkeletonRecieverPort);
	ServerSkeletonReciever.OnMessage = OnSkeletonJson;
	ServerSkeletonSender = new WebsocketServer(ServerSkeletonSenderPort);

	BroadcastServer = new UdpBroadcastServer(BroadcastServerPort);
	BroadcastServer.OnMessage = function(Data,Sender)	{	OnBroadcastMessage(Data,Sender,BroadcastServer);	}

	
}

//	main
Main();
