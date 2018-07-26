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

function OpenglExtension_LoseContext()
{
	this.loseContext = function()
	{
		Debug("Do loseContext");
	}
}

function OpenglExtension_EXTColorBufferFloat()
{
	
}

function OpenglDataBuffer(Name)
{
	this.Name = Name;
}

function OpenglFrameBuffer(Name)
{
	this.Name = Name;
}

function FakeOpenglContext(ContextType)
{
	Debug("FakeOpenglContext(" + ContextType + ")");

	this.DataBufferCounter = 0;
	this.FrameBufferCounter = 0;
	
	this.getExtension = function(ExtensionName)
	{
		if ( ExtensionName == "WEBGL_lose_context" )
			return new OpenglExtension_LoseContext();
		if ( ExtensionName == "EXT_color_buffer_float" )
			return new OpenglExtension_EXTColorBufferFloat();
		
		//WEBGL_get_buffer_sub_data_async
		return null;
	}
	//WebGLRenderingContext.getSupportedExtensions.
	
	this.disable = function(GlStateEnum)	{	Debug("gldisable(" + GlStateEnum +")");	}
	this.enable = function(GlStateEnum)		{	Debug("glenable(" + GlStateEnum +")");	}
	this.cullFace = function(CullFaceEnum)	{	Debug("cullFace(" + CullFaceEnum +")");	}

	this.createBuffer = function()
	{
		this.DataBufferCounter++;
		let NewBuffer = new OpenglDataBuffer(this.DataBufferCounter);
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
		let NewBuffer = new OpenglFrameBuffer(this.FrameBufferCounter);
		return NewBuffer;
		
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

function FakeCanvas()
{
	this.Context = null;
	
	let This = this;
	this.getContext = function(ContextType)
	{
		if ( This.Context == null )
		{
			This.Context = new FakeOpenglContext(ContextType);
		}
		return This.Context;
	}
	
}

function FakeScreen()
{
	this.width = 999;
	this.height = 888;
}

function FakeWindow()
{
	this.screen = new FakeScreen();
}

function FakeDocument()
{
	this.createElement = function(Type)
	{
		if ( Type == "canvas" )
			return new FakeCanvas();
		
		throw "Need to create a fake " + Type;
	}
}

function FakeConsole()
{
	this.log = Debug;
	this.warn = Debug;
}


function createVertexShader$1(e) {
	return createVertexShader(e, "\n    precision highp float;\n    attribute vec3 clipSpacePos;\n    attribute vec2 uv;\n    varying vec2 resultUV;\n\n    void main() {\n      gl_Position = vec4(clipSpacePos, 1);\n      resultUV = uv;\n    }")
}
function createVertexBuffer(e) {
	return createStaticVertexBuffer(e, new Float32Array([-1, 1, 0, 0, 1, -1, -1, 0, 0, 0, 1, 1, 0, 1, 1, 1, -1, 0, 1, 0]))
}
function createIndexBuffer(e) {
	return createStaticIndexBuffer(e, new Uint16Array([0, 1, 2, 2, 1, 3]))
}



//	gr: window wrapper to emulate browser for tensor flow
var window = new FakeWindow();
var console = new FakeConsole();
var document = new FakeDocument();



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

function StartPoseDetection(Posenet)
{
	Debug("Posenet loaded!");
	Debug(Posenet);
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

	
	//	load posenet
	Debug("Posenet is... " + posenet );
	posenet.load().then( StartPoseDetection ).catch( PosenetFailed );

	
}

//	main
Main();
