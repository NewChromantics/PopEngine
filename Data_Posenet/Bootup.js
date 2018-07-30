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
		this.data = Img.GetRgba8();
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

let FrameFragShaderSource = LoadFileAsString("Data_Posenet/DrawFrameAndPose.frag");
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
		//	multiplier for debugging
		let Mult = 100;
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
			let PoseLines = GetPoseLines(LastPose);
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


function RunPoseDetection(PoseNet,NewImage,OnPoseFound)
{
	//	for CPU mode (and gpu?)
	if ( NewImage instanceof Image )
	{
		Debug("Converting image to ImageData..");
		NewImage = new ImageData(NewImage);
	}
	
	var imageScaleFactor = 0.20;
	//var outputStride = 16;
	var outputStride = 32;
	var flipHorizontal = false;
	
	//console.log("Processing...");
	//console.log(NewImage);
	//let StartTime = performance.now();
	
	let OnNewPose = function(NewPose)
	{
		Debug("OnNewPose...");
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
		//NewPose.keypoints.forEach( RescaleCoords );
		
		OnPoseFound(NewPose);
	}
	
	let OnEstimateFailed = function(e)
	{
		Debug("estimateSinglePose failed");
		Debug(e);
	}
	
	Debug("Estimating pose... on " + NewImage.width + "x" + NewImage.height );
	let EstimatePromise = PoseNet.estimateSinglePose(NewImage, imageScaleFactor, flipHorizontal, outputStride);
	EstimatePromise.then( OnNewPose ).catch( OnEstimateFailed );
}

function StartPoseDetection(PoseNet)
{
	Debug("Posenet loaded!");
	
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

	let FrameImage = new Image('Data_Posenet/jazzflute.jpg');
	//FrameImage.Flip();
	LastFrameImage = FrameImage;
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



//	adding here and we'll put them into our system
function AddWebglBindings(This)
{
	This.Enums = This.GetEnums();
	Debug("Pre-existing enums: ");
	Object.keys(This.Enums).forEach( function(k){	Debug(k+"=" + This.Enums[k]);} );

	let PushEnum = function(Name)
	{
		if ( Name.length == 0 )
			return;
		if ( Name.startsWith('//') )
			return;
		
		//	enum already done
		if ( This.Enums[Name] !== undefined )
			return;
		
		Debug("Added Enum "+Name);
		This.Enums[Name] = Name;
	}
	WebglEnumNames.forEach( PushEnum );

	//	special case (incrementing enum value)
	This.MAX_COMBINED_TEXTURE_IMAGE_UNITS = 32;
	/*
	for ( let i=0;	i<This.MAX_COMBINED_TEXTURE_IMAGE_UNITS;	i++ )
	{
		let Name = 'TEXTURE'+i;
		if ( This.Enums[Name] !== undefined )
			return;
		This.Enums[Name] = This.Enums['TEXTURE0']+i;
	}
	 */

	/*
	This.disable =				function(Enum)	{	Debug("disable("+ Enum);	};
	This.enable =				function(Enum)	{	Debug("enable("+ Enum);	};
	This.cullFace =				function(Enum)	{	Debug("cullFace("+ Enum);	};
	This.bindBuffer =			function()	{	Debug("bindBuffer");	};
	This.bufferData =			function()	{	Debug("bufferData");	};
	This.bindFramebuffer =		function()	{	Debug("bindFramebuffer");	};
	This.framebufferTexture2D =	function()	{	Debug("framebufferTexture2D");	};
	This.bindTexture =			function(Enum,Texture)	{	Debug("bindTexture("+ Enum+","+Texture);	};
	This.texImage2D =			function()	{	Debug("texImage2D");	};
	This.useProgram =			function()	{	Debug("useProgram");	};
	This.texParameteri =		function(Texture,Enum,Value)	{	Debug("texParameteri("+Texture+","+ Enum+"," + Value);	};
	This.attachShader =			function()	{	Debug("attachShader");	};
	This.vertexAttribPointer =	function()	{	Debug("vertexAttribPointer");	};
	This.enableVertexAttribArray = function()	{	Debug("enableVertexAttribArray");	};
	This.SetUniform =			function(Name,Value)	{	Debug("SetUniform("+Name+","+Value);	};
	This.texSubImage2D =		function()	{	Debug("texSubImage2D");	};
	This.readPixels =			function()	{	Debug("readPixels");	};
	This.viewport =				function()	{	Debug("viewport");	};
	This.scissor =				function()	{	Debug("scissor");	};
	This.activeTexture =		function()	{	Debug("activeTexture");	};
	This.drawElements =			function()	{	Debug("drawElements");	};
	 */
}




function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Posenet");
	Window1.OnRender = function(){	WindowRender( Window1 );	};
	AddWebglBindings(Window1);
	
	//	navigator global window is setup earlier
	window.OpenglContext = Window1;
	
	//	make a context, then let tensorflow grab the bindings
	include('Data_Posenet/tfjs.0.11.7.js');
	include('Data_Posenet/posenet.0.1.2.js');
	//include("Data_Posenet/Hello.js");


	
	//	load posenet
	Debug("Loading posenet...");
	posenet.load().then( StartPoseDetection ).catch( PosenetFailed );
	
}

//	main
Main();
