
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


function GetSkeletonLines(Skeleton)
{
	if ( Skeleton == null )
		return [];
	
	let Lines = [];
	
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
	
	Debug( Lines.length + " skeleton lines");
	return Lines;
}

function GetFeatureLines(Face)
{
	let Lines = [];
	
	if ( Face != null )
	{
		let FaceRect = Face.Rect;
		let l = FaceRect[0];
		let r = FaceRect[0] + FaceRect[2];
		let t = FaceRect[1];
		let b = FaceRect[1] + FaceRect[3];
		Lines.push( [l,t,r,t] );
		Lines.push( [r,t,r,b] );
		Lines.push( [r,b,l,b] );
		Lines.push( [l,b,l,t] );
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
			
			const MAX_LINES = 100;
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

var DlibThreadCount = 10;
var DlibLandMarksdat = LoadFileAsArrayBuffer('Data_Dlib/shape_predictor_68_face_landmarks.dat');
//var FaceProcessor = new Dlib( DlibLandMarksdat, DlibThreadCount );
var CurrentProcessingImageCount = 0;

function OnNewFrame(NewFrameImage,SaveFilename)
{
	//LastFrameImage = NewFrameImage;
	//	temp work throttler
	if ( CurrentProcessingImageCount > DlibThreadCount )
		return;
	CurrentProcessingImageCount++;
	Debug("Now processing image " + NewFrameImage.GetWidth() + "x" + NewFrameImage.GetHeight() );
	
	let OnFace = function(Face)
	{
		CurrentProcessingImageCount--;
		OnNewFace(Face,NewFrameImage,SaveFilename);
	}
	
	let FaceRect = [0.3,0.2,0.3,0.4];
	
	//FaceProcessor.FindFaces(NewFrameImage)
	FaceProcessor.FindFaceFeatures( NewFrameImage, FaceRect )
	.then( OnFace )
	.catch( OnFailedNewFace );
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

function OnSkeletonJson(SkeletonJson)
{
	var Skeleton = JSON.parse(SkeletonJson);
	
	let GetKeypointPos = function(Name)
	{
		let FindKeypointPart = function(Keypoint)
		{
			return Keypoint.part == Name;
		};
		let Keypoints = Skeleton.keypoints;
		let kp = Keypoints.find( FindKeypointPart );
		if ( kp === undefined )
		{
			Debug("Failed to find keypoint " + Name);
			return undefined;
		}
		return kp.position;
	}
	
	let SimpleSkeleton = {};
	
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
	//MediaDevices.EnumDevices().then( LoadDevice );

	//let TestImage = new Image('Data_dlib/NataliePortman.jpg');
	let TestImage = new Image('Data_dlib/Face.png');
	//let TestImage = new Image('Data_dlib/FaceLeft.jpg');
	//OnNewFrame(TestImage,'Data_dlib/Face.json');
	
	Server = new WebsocketServer(ServerPort);
	Server.OnMessage = OnSkeletonJson;

	
}

//	main
Main();
