
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

function GetFeatureLines(Face)
{
	let Lines = [];
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
			
			const MAX_LINES = 100;
			let PoseLines = GetFeatureLines(LastFace);
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
	
	for ( let i=0;	i<FaceLandmarks.length;	i+=2 )
	{
		PushFeature( FaceLandmarks[i+0], FaceLandmarks[i+1] );
	}
	
	Debug("Got face: x" + Face.Features.length );
	
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
var FaceProcessor = new Dlib( DlibLandMarksdat, DlibThreadCount );
var CurrentProcessingImageCount = 0;

function OnNewFrame(NewFrameImage,SaveFilename)
{
	//	temp work throttler
	if ( CurrentProcessingImageCount > 5 )
		return;
	CurrentProcessingImageCount++;
	Debug("Now processing image " + NewFrameImage.GetWidth() + "x" + NewFrameImage.GetHeight() );
	
	let OnFace = function(Face)
	{
		CurrentProcessingImageCount--;
		OnNewFace(Face,NewFrameImage,SaveFilename);
	}
	
	FaceProcessor.FindFace(NewFrameImage)
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
	//MediaDevices.EnumDevices().then(LoadDevice);

	//let TestImage = new Image('Data_dlib/NataliePortman.jpg');
	let TestImage = new Image('Data_dlib/Face.png');
	//let TestImage = new Image('Data_dlib/FaceLeft.jpg');
	OnNewFrame(TestImage,'Data_dlib/Face.json');
	
	
}

//	main
Main();
