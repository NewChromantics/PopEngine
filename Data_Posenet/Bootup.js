function ToHexString(Bytes,JoinChar,MaxBytes)
{
	if ( MaxBytes === undefined )
		MaxBytes = Bytes.byteLength;
	let Length = Math.min( MaxBytes, Bytes.byteLength );
	
	if ( JoinChar === undefined )
		JoinChar = '';
	
	let Str = '';
	let PushByte = function(byte)
	{
		Str += ('0' + (byte & 0xFF).toString(16)).slice(-2);
		Str += JoinChar;
	}

	for ( let i=0;	i<Length;	i++ )
		PushByte(Bytes[i]);
	
	//Bytes.forEach(PushByte);
	return Str;
}

String.prototype.replaceAll = function(search, replace)
{
	//	https://stackoverflow.com/questions/1967119/why-does-javascript-replace-only-first-instance-when-using-replace
	if (replace === undefined) {
		return this.toString();
	}
	return this.split(search).join(replace);
}

function GetLocalFilenameOfUrl(Url)
{
	var UrlRegex = new RegExp('^http[s]://([a-zA-z\-.]+)/(.*)');
	var Match = Url.match( UrlRegex );
	
	let Domain = Match[1];
	let Path = Match[2];
	
	//	gr: maybe some better local cache thing
	//if ( Domain == 'storage.googleapis.com' )
	let PathParts = Path.split('/');
	let Filename = PathParts.pop();
	let LocalPath = PathParts.join('_');
	LocalPath = "Data_Posenet/" + LocalPath + "/" + Filename;
	Debug("Converted " + Url + " to " + Filename);
	
	return LocalPath;
}

//	to allow tensorflow to TRY and read video, (and walk past the code), we at least need a constructor for instanceof HTMLVideoElement
function HTMLVideoElement()
{
	
}


//	need to get this working with Image
/*
 e.prototype.fromPixels = function(e, t) {
 if (null == e)
 throw new Error("MathBackendWebGL.writePixels(): pixels can not be null");
 var r = [e.height, e.width]
 , n = [e.height, e.width, t];
 if (e instanceof HTMLVideoElement) {
 if (null == this.fromPixelsCanvas) {
 if (!ENV.get("IS_BROWSER"))
 throw new Error("Can't read pixels from HTMLImageElement outside the browser.");
 if ("complete" !== document.readyState)
 throw new Error("The DOM is not ready yet. Please call tf.fromPixels() once the DOM is ready. One way to do that is to add an event listener for `DOMContentLoaded` on the document object");
 this.fromPixelsCanvas = document.createElement("canvas")
 }
 this.fromPixelsCanvas.width = e.width,
 this.fromPixelsCanvas.height = e.height,
 this.fromPixelsCanvas.getContext("2d").drawImage(e, 0, 0, e.width, e.height),
 e = this.fromPixelsCanvas
 }
 var a = Tensor.make(r, {}, "int32");
 this.texData.get(a.dataId).usage = TextureUsage.PIXELS,
 this.gpgpu.uploadPixelDataToTexture(this.getTexture(a.dataId), e);
 var i = new FromPixelsProgram(n)
 , o = this.compileAndRun(i, [a]);
 return a.dispose(),
 o
 }
 
 
 
 
 
 
 
 function uploadPixelDataToTexture(e, t, r) {
 callAndCheck(e, function() {
 return e.bindTexture(e.TEXTURE_2D, t)
 }),
 callAndCheck(e, function() {
 return e.texImage2D(e.TEXTURE_2D, 0, e.RGBA, e.RGBA, e.UNSIGNED_BYTE, r)
 }),
 callAndCheck(e, function() {
 return e.bindTexture(e.TEXTURE_2D, null)
 })
 }
 */

//	wrapper for file loading in posenet
function XMLHttpRequest()
{
	//Debug("Created a XMLHttpRequest");
	this.status = 404;
	this.Filename = null;
	this.responseType = 'string';
	this.responseText = null;
	this.response = null;
	
	this.open = function(RequestMode,Url)
	{
		Debug("XMLHttpRequest.open( " + RequestMode + " " + Url );
 
		try
		{
			this.Filename = GetLocalFilenameOfUrl( Url );
		}
		catch(e)
		{
			Debug(e);
		}
	}
	
	this.onload = function()
	{
		Debug("OnLoad");
	}
	
	this.onerror = function(Error)
	{
		Debug("OnError(" + Error +")");
	}
	
	this.send = function()
	{
		try
		{
			Debug("Requesting " + this.Filename + " as " + this.responseType );

			if ( this.responseType == 'string' )
			{
				let Contents = LoadFileAsString(this.Filename);
				this.responseText = Contents;
				Debug("Loaded: " + this.Filename + " length: " + Contents.length );
				Debug(Contents);
			}
			else if ( this.responseType == 'arraybuffer' )
			{
				let Contents = LoadFileAsArrayBuffer(this.Filename)
				this.response = Contents;
				Debug("Loaded: " + this.Filename + " byte length: " + Contents.byteLength );
				Debug( ToHexString(Contents,' ',40) + "..." );
			}
			else
			{
				throw "Don't know how to load url/file as " + this.responseType;
			}
			
			Debug("calling onload...");
			this.status = 200;
			this.onload();
		}
		catch(e)
		{
			Debug("XMLHttpRequest error: " + e);
			this.onerror(e);
		}
	}
}

function WebglExtension_LoseContext()
{
	this.loseContext = function()
	{
		Debug("Do loseContext");
	}
}

function WebglExtension_EXTColorBufferFloat()
{
	
}

function WebglDataBuffer(Name)
{
	this.Name = Name;
}

function WebglFrameBuffer(Name)
{
	this.Name = Name;
}

function WebglVertShader()
{
	this.Source = null;
}

function WebglFragShader()
{
	this.Source = null;
}

function WebglProgram()
{
	this.VertShader = null;
	this.FragShader = null;
	this.Shader = null;
	this.Error = null;
	
	this.AddShader = function(Shader)
	{
		if ( Shader instanceof WebglVertShader )
			this.VertShader = Shader;
		else if ( Shader instanceof WebglFragShader )
			this.FragShader = Shader;
		else
			throw "Don't know what type of shader is supplied for program";
	}
	
	this.Build = function(OpenglContext)
	{
		try
		{
			let VertSource = this.VertShader.Source;
			let FragSource = this.FragShader.Source;
			this.Shader = new OpenglShader( OpenglContext, VertSource, FragSource );
		}
		catch(e)
		{
			this.Error = e;
		}
	}
}

function FakeOpenglContext(ContextType,ParentCanvas)
{
	Debug("FakeOpenglContext(" + ContextType + ")");

	//	constants
	let CONST = 1;
	this.FRAMEBUFFER_COMPLETE = CONST++;
	this.VERTEX_SHADER = CONST++;
	this.FRAGMENT_SHADER = CONST++;
	
	
	this.ParentCanvas = ParentCanvas;
	
	this.GetOpenglContext = function()
	{
		return this.ParentCanvas.WebWindow.OpenglContext;
	}

	this.getExtension = function(ExtensionName)
	{
		if ( ExtensionName == "WEBGL_lose_context" )
			return new WebglExtension_LoseContext();
		if ( ExtensionName == "EXT_color_buffer_float" )
			return new WebglExtension_EXTColorBufferFloat();
		
		//WEBGL_get_buffer_sub_data_async
		return null;
	}
	//WebGLRenderingContext.getSupportedExtensions.
	
	this.disable = function(GlStateEnum)	{	Debug("gldisable(" + GlStateEnum +")");	}
	this.enable = function(GlStateEnum)		{	Debug("glenable(" + GlStateEnum +")");	}
	this.cullFace = function(CullFaceEnum)	{	Debug("cullFace(" + CullFaceEnum +")");	}

	this.getParameter = function(ParameterEnum)
	{
		Debug("getParameter(" + ParameterEnum + ")" );
	}
	
	this.createBuffer = function()
	{
		let NewBuffer = new WebglDataBuffer(this.DataBufferCounter);
		return NewBuffer;
	}
	
	this.bindBuffer = function(BufferBinding,Buffer)
	{
		Debug("BindBuffer( " + BufferBinding + ", " + Buffer.Name + ")" );
	}
	
	this.bufferData = function(BufferBinding,Data,Mode)
	{
		Debug("bufferData( " + BufferBinding + ", " + Data + ", " + Mode + " )" );
	}
	
	this.createFramebuffer = function()
	{
		this.FrameBufferCounter++;
		let NewBuffer = new WebglFrameBuffer(this.FrameBufferCounter);
		return NewBuffer;
	}
	
	this.bindFramebuffer = function(Binding,FrameBuffer)
	{
		//Binding = e.FRAMEBUFFER
	}
	this.framebufferTexture2D = function(Binding,Attachment,TextureBinding,Texture,x)
	{
	}
	
	this.createTexture = function()
	{
		return new Image();
	}
	
	this.bindTexture = function(Binding,Texture)
	{
		Debug("bindTexture(" + Binding + ", " + Texture + ")");
	}

	this.texImage2D = function(Binding,a,b,c,d,e,internalformat,pixelformat,pixeldata)
	{
		Debug("texImage2D()");
	}
	

	this.texParameteri = function(Binding,ParameterEnum,Value)
	{
	}
	
	this.checkFramebufferStatus = function(Binding)
	{
		return this.FRAMEBUFFER_COMPLETE;
	}
	
	this.createShader = function(ShaderType)
	{
		if ( ShaderType == this.VERTEX_SHADER )
			return new WebglVertShader();
		
		if ( ShaderType == this.FRAGMENT_SHADER )
			return new WebglFragShader();
		
		throw "Unknown shader type " + ShaderType;
	}
	
	this.shaderSource = function(Shader,Source)
	{
		Shader.Source = Source;
	}
	
	this.compileShader = function(Shader)
	{
	}
	
	this.getShaderParameter = function(Shader,ParameterEnum)
	{
		if ( ParameterEnum == this.COMPILE_STATUS )
			return true;
		
		throw "Unknown shader parameter " + ParameterEnum;
	}
	
	this.getProgramParameter = function(Program,ParameterEnum)
	{
		if ( ParameterEnum == this.LINK_STATUS )
			return (Program.Error == null);
		
		throw "Unknown program parameter " + ParameterEnum;
	}
	
	this.createProgram = function()
	{
		return new WebglProgram();
	}
	
	this.attachShader = function(Program,Shader)
	{
		Program.AddShader(Shader);
	}

	this.linkProgram = function(Program)
	{
		Program.Build( this.GetOpenglContext() );
	}
	
	this.getProgramInfoLog = function(Program)
	{
		return Program.Error;
	}
}

/*
 unction bindVertexBufferToProgramAttribute(e, t, r, n, a, i, o) {
 var s = e.getAttribLocation(t, r);
 return -1 !== s && (callAndCheck(e, function() {
 return e.bindBuffer(e.ARRAY_BUFFER, n)
 }),
 callAndCheck(e, function() {
 return e.vertexAttribPointer(s, a, e.FLOAT, !1, i, o)
 }),
 callAndCheck(e, function() {
 return e.enableVertexAttribArray(s)
 }),
 !0)*/

function FakeCanvas(WebWindow)
{
	this.WebWindow = WebWindow;
	//this.Window.OnRender = function(){	/*WindowRender( Window1 );*/	};

	this.WebglContext = null;
	
	let This = this;
	this.getContext = function(ContextType)
	{
		if ( This.WebglContext == null )
		{
			This.WebglContext = new FakeOpenglContext( ContextType, This );
		}
		return This.WebglContext;
	}
	
}

function FakeScreen()
{
	this.width = 999;
	this.height = 888;
}

function FakeWindow()
{
	this.OpenglContext = null;
	this.screen = new FakeScreen();
}

function FakeDocument(WebWindow)
{
	this.createElement = function(Type)
	{
		if ( Type == "canvas" )
			return new FakeCanvas(WebWindow);
		
		throw "Need to create a fake " + Type;
	}
}

function FakeConsole()
{
	this.log = Debug;
	this.warn = Debug;
}

/*
function createVertexShader$1(e) {
	return createVertexShader(e, "\n    precision highp float;\n    attribute vec3 clipSpacePos;\n    attribute vec2 uv;\n    varying vec2 resultUV;\n\n    void main() {\n      gl_Position = vec4(clipSpacePos, 1);\n      resultUV = uv;\n    }")
}
function createVertexBuffer(e) {
	return createStaticVertexBuffer(e, new Float32Array([-1, 1, 0, 0, 1, -1, -1, 0, 0, 0, 1, 1, 0, 1, 1, 1, -1, 0, 1, 0]))
}
function createIndexBuffer(e) {
	return createStaticIndexBuffer(e, new Uint16Array([0, 1, 2, 2, 1, 3]))
}
*/


//	gr: window wrapper to emulate browser for tensor flow
var window = new FakeWindow();
var console = new FakeConsole();
var document = new FakeDocument(window);



//	gr: include is not a generic thing (or a wrapper yet) so we can change
//	LoadFileAsString to a file-handle to detect file changes to auto reload things
function include(Filename)
{
	let Source = LoadFileAsString(Filename);
	CompileAndRun( Source );
}
include("Data_Posenet/tfjs.0.11.7.js");
include("Data_Posenet/posenet.0.1.2.js");
//include("Data_Posenet/Hello.js");

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

let DebugFragShaderSource = `
#version 410
in vec2 uv;
void main()
{
	gl_FragColor = vec4(uv.x,uv.y,0,1);
}
`;

var DebugFrameShader = null;

function WindowRender(RenderTarget)
{
	try
	{
		if ( !DebugFrameShader )
		{
			DebugFrameShader = new OpenglShader( RenderTarget, VertShaderSource, DebugFragShaderSource );
		}
		
		let SetUniforms = function()
		{
			
		}
		
		RenderTarget.ClearColour(0,1,0);
		RenderTarget.DrawQuad( DebugFrameShader, SetUniforms );
	}
	catch(Exception)
	{
		RenderTarget.ClearColour(1,0,0);
		Debug(Exception);
	}
}


function RunPoseDetection(PoseNet,NewImage,OnPoseFound)
{
	var imageScaleFactor = 0.20;
	var outputStride = 16;
	var flipHorizontal = false;
	
	//console.log("Processing...");
	//console.log(NewImage);
	//let StartTime = performance.now();
	
	let OnNewPose = function(NewPose)
	{
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
		NewPose.keypoints.forEach( RescaleCoords );
		
		OnPoseFound(NewPose);
	}
	
	let OnEstimateFailed = function(e)
	{
		Debug("estimateSinglePose failed");
		Debug(e);
	}
	
	let EstimatePromise = PoseNet.estimateSinglePose(NewImage, imageScaleFactor, flipHorizontal, outputStride);
	EstimatePromise.then( OnNewPose ).catch( OnEstimateFailed );
}

function StartPoseDetection(PoseNet)
{
	Debug("Posenet loaded!");
	
	let OnFoundPose = function(Pose)
	{
		try
		{
			//SendNewPose(Pose);
		}
		catch(e)
		{
			console.log(e);
		}
		Debug("Found pose");
		//console.log("Found pose in " + Pose.ProcessingTimeMs + "ms: ");
		console.log(Pose);
	}

	let FrameImage = new Image('Data_Posenet/jazzflute.jpg');
	FrameImage.width = FrameImage.GetWidth();
	FrameImage.height = FrameImage.GetHeight();
	
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

function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Posenet");
	Window1.OnRender = function(){	WindowRender( Window1 );	};

	window.OpenglContext = Window1;

	
	//	load posenet
	Debug("Loading posenet...");
	posenet.load().then( StartPoseDetection ).catch( PosenetFailed );
	
}

//	main
Main();
