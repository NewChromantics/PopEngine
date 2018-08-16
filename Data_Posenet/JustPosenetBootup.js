//	gr: include is not a generic thing (or a wrapper yet) so we can change
//	LoadFileAsString to a file-handle to detect file changes to auto reload things
function include(Filename)
{
	let Source = LoadFileAsString(Filename);
	return CompileAndRun( Source );
}

var PosenetTextures = [];
var OnOpenglImageCreated = function(Texture)
{
	PosenetTextures.push( Texture );
}

include('XMLHttpRequest.js');
include('Webgl.js');

var ShowPosenetImages = false;
var RGBAFromCamera = true;
var FlipCameraInput = false;

var WebServer = null;
var WebServerPort = 8000;



//	to allow tensorflow to TRY and read video, (and walk past the code), we at least need a constructor for instanceof HTMLVideoElement
function HTMLVideoElement()
{
	
}

var AllowBgraAsRgba = true;
var imageScaleFactor = 0.20;
var outputStride = 32;
//var outputStride = 32;
var ClipToSquare = false;
//var ClipToSquare = true;	//	gr: slow atm!
//var ClipToSquare = outputStride * 10;	//	gr: slow atm!


//	gr: this might eed to be more intelligently back if accessing pixels synchronously
function ImageData(Pixels)
{
	this.width = 0;
	this.height = 0;
	this.data = null;	//	Uint8ClampedArray rgba
	
	this.SetFromImage = function(Img)
	{
		this.width = Img.GetWidth();
		this.height = Img.GetHeight();
		this.data = Img.GetRgba8(AllowBgraAsRgba);
		
		//Debug( ToHexString(this.data,'  20 ) );
	}
	
	//	auto load
	if ( Pixels instanceof Image )
	{
		this.SetFromImage( Pixels );
	}
}


/*	This is the tensorflow pixel reading for cpu... make this fast
e.prototype.fromPixels = function(e, t) {
	if (null == e)
		throw new Error("MathBackendCPU.writePixels(): pixels can not be null");
	var r, n;
	if (e instanceof ImageData)
		r = e.data;
	else if (e instanceof HTMLCanvasElement)
		r = e.getContext("2d").getImageData(0, 0, e.width, e.height).data;
	else {
		if (!(e instanceof HTMLImageElement || e instanceof HTMLVideoElement))
			throw new Error("pixels is of unknown type: " + e.constructor.name);
		if (null == this.canvas)
			throw new Error("Can't read pixels from HTMLImageElement outside the browser.");
		this.canvas.width = e.width,
		this.canvas.height = e.height,
		this.canvas.getContext("2d").drawImage(e, 0, 0, e.width, e.height),
		r = this.canvas.getContext("2d").getImageData(0, 0, e.width, e.height).data
	}
	if (4 === t)
		n = new Int32Array(r);
	else {
		var a = e.width * e.height;
		n = new Int32Array(a * t);
		for (var i = 0; i < a; i++)
			for (var o = 0; o < t; ++o)
				n[i * t + o] = r[4 * i + o]
				}
	var s = [e.height, e.width, t];
	return tensor3d(n, s, "int32")
}
*/




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
var LastFrameImage = null;
var LastPose = null;

function GetPoseLines(Pose)
{
	if ( Pose == null )
		return [0,0,1,1];
	
	let Lines = [];

	let PushLine = function(Keypointa,Keypointb)
	{
		if ( Keypointa === undefined || Keypointb === undefined )
			return;
		let Mult = 1;
		Lines.push( Keypointa.position.x * Mult );
		Lines.push( Keypointa.position.y * Mult );
		Lines.push( Keypointb.position.x * Mult );
		Lines.push( Keypointb.position.y * Mult );
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
	
	//Debug("Got " + Lines.length + " lines for the pose");
	
	return Lines;
}

var CurrentTexture = 0;

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
		
			if ( PosenetTextures.length && ShowPosenetImages )
			{
				CurrentTexture++;
				let Index = Math.floor(CurrentTexture / 3) % PosenetTextures.length;
				//Debug(Index + " = " + PosenetTextures[Index]);
				Shader.SetUniform("Frame", PosenetTextures[Index], 0 );
				Shader.SetUniform("HasFrame", true );
			}
			
			
			const MAX_LINES = 100;
			let PoseLines = GetPoseLines(LastPose);
			PoseLines.length = Math.min( PoseLines.length, MAX_LINES );
			//Debug(PoseLines);
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


var CachedImageData = null;

function RunPoseDetection(PoseNet,NewImage)
{
	//	for CPU mode (and gpu?)
	if ( NewImage instanceof Image )
	{
		if ( CachedImageData == null )
		{
			CachedImageData = new ImageData(NewImage);
		}
		else
		{
			NewImage.GetRgba8(AllowBgraAsRgba,CachedImageData.data);
		}
		NewImage = CachedImageData;
	}
	
		
	var flipHorizontal = false;
	
	//console.log("Processing...");
	//console.log(NewImage);
	//let StartTime = performance.now();
	
	let OnEstimateFailed = function(e)
	{
		Debug("estimateSinglePose failed");
		Debug(e);
	}
	
	Debug("Estimating pose... on " + NewImage.width + "x" + NewImage.height );
	try
	{
		/*
		Debug(tf);
		Debug("PoseNet keys");
		Debug( Object.keys(PoseNet.mobileNet) );
		let tf = require("@tensorflow/tfjs");
		Debug(tf);
		//	gr: this input tensor is setup, then GPU uploads to a texture which is associated with the tensor's
		//		.dataId (arbirtry name)
		let TensorSize = [NewImage.width,NewImage.height];
		let InputTensor = Tensor.make( TensorSize, {}, "int32" );
		PoseNet.predictForSinglePose(InputTensor, outputStride);
		
		*/
		let EstimatePromise = PoseNet.estimateSinglePose(NewImage, imageScaleFactor, flipHorizontal, outputStride);
		return EstimatePromise;
	}
	catch(e)
	{
		Debug("Error during PoseNet.estimateSinglePose: " + e);
		throw e;
	}
}


function OnFoundPose(Pose,Image)
{
	Debug("OnNewPose...");
	//let EndTime = performance.now();
	//let ProcessingTime = EndTime - StartTime;
	//NewPose.ProcessingTimeMs = ProcessingTime;
		
	let ImageWidth = Image.GetWidth();
	let ImageHeight = Image.GetHeight();
	//console.log(ImageWidth);
		
	//	put coords in uv space
	let RescaleCoords = function(keypoint)
	{
		keypoint.position.x /= ImageWidth;
		keypoint.position.y /= ImageHeight;
		//keypoint.position.y = 1-keypoint.position.y;
		//keypoint.position.x = 1-keypoint.position.x;
	};
	Pose.keypoints.forEach( RescaleCoords );
	

	/*
		let OnFoundPose = function(Pose)
		{
	 Debug("Found pose, score=" + Pose.score );
	 let DebugKeypoint = function(kp)
	 {
	 Debug("Keypoint: " + kp.part + " score=" + kp.score + " " + kp.position.x + "," + kp.position.y );
	 }
	 Pose.keypoints.forEach( DebugKeypoint );
	 
	 LastPose = Pose;
	 
	 try
	 {
	 //SendNewPose(Pose);
	 }
	 catch(e)
	 {
	 console.log(e);
	 }
	 //console.log("Found pose in " + Pose.ProcessingTimeMs + "ms: ");
	 console.log(Pose);
		}
		*/

	
	LastPose = Pose;
	
	if ( LastFrameImage!=null )
	{
		LastFrameImage.Clear();
	}
	
	LastFrameImage = Image;
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


var CurrentProcessingCount = 0;

function StartPoseDetection(PoseNet)
{
	var UseTestImage = false;
	
	
	if ( UseTestImage )
	{
		
		let FrameImage = new Image('girlstanding.jpg');
		//FrameImage.Flip();
		LastFrameImage = FrameImage;
		try
		{
			RunPoseDetection( PoseNet, FrameImage )
			.then( OnFoundPose )
			.catch( Debug );
		}
		catch(e)
		{
			//OnFoundPose(null, 0);
			Debug(e);
		}
		
	}
	else//	not test image
	{
		let OnFrameFilter = function()
		{
			if ( CurrentProcessingCount >= 1 )
			{
				//Debug("Frame rejected:" + CurrentProcessingCount);
				return false;
			}
			return true;
		}
		
		let OnFrame = function(FrameImage)
		{
			if ( CurrentProcessingCount >= 1 )
			{
				//Debug("Skipping frame:" + CurrentProcessingCount);
				FrameImage.Clear();
				return;
			}
			CurrentProcessingCount++;
			
			if ( FlipCameraInput )
				FrameImage.Flip();
			
			let OnPose = function(Pose)
			{
				CurrentProcessingCount--;
				let PoseDetectionDuration = Date.now() - StartTime;
				Pose.Duration = PoseDetectionDuration;
				Debug("Pose detection took " + Pose.Duration + "ms" );
				OnFoundPose(Pose,FrameImage);
			}

			//	clip image to square
			if ( ClipToSquare )
			{
				let Width = FrameImage.GetWidth();
				let Height = FrameImage.GetHeight();
				if ( typeof ClipToSquare == "number" )
					Width = ClipToSquare;
				
				Width = Math.min( Width, Height );
				Height = Math.min( Width, Height );
				
				FrameImage.Clip( [0,0,Width,Height] );
			}
			
			//	run promise
			let StartTime = Date.now();
			RunPoseDetection( PoseNet, FrameImage )
			.then( OnPose )
			.catch( Debug );
		}
		
		//	tries to find these in order, then grabs any
		var VideoDeviceNames = ["c920","isight","facetime"];

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
				
				let VideoCapture = new MediaSource(VideoDeviceName,RGBAFromCamera,OnFrameFilter);
				VideoCapture.OnNewFrame = OnFrame;
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
}

function PosenetFailed(Arg1)
{
	Debug("Posenet failed to load");
	Debug(Arg1);
}


function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Posenet",true);
	Window1.OnRender = function(){	WindowRender( Window1 );	};
	
	//	navigator global window is setup earlier
	window.OpenglContext = Window1;
	
	Debug("Loading tensorflow");
	
	//	make a context, then let tensorflow grab the bindings
	include('tfjs.0.11.7.js');
	include('posenet.0.1.2.js');


	//	load posenet
	Debug("Loading posenet...");
	posenet.load().then( StartPoseDetection ).catch( PosenetFailed );
	
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
	
	Retry( AllocWebServer, 1000 );

}

//	main
Main();
