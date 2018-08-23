var WebglEnumNames = `
NO_ERROR
INVALID_ENUM
INVALID_VALUE
INVALID_OPERATION
INVALID_FRAMEBUFFER_OPERATION
OUT_OF_MEMORY
CONTEXT_LOST_WEBGL

DEPTH_TEST
STENCIL_TEST
BLEND
DITHER
POLYGON_OFFSET_FILL
SAMPLE_COVERAGE
SCISSOR_TEST
FRAMEBUFFER_COMPLETE
VERTEX_SHADER
FRAGMENT_SHADER

//	Params
GPU_DISJOINT_EXT
MAX_TEXTURE_SIZE
CULL_FACE
BACK
TEXTURE_WRAP_S
CLAMP_TO_EDGE
TEXTURE_WRAP_T
CLAMP_TO_EDGE
TEXTURE_MIN_FILTER
NEAREST
TEXTURE_MAG_FILTER
TEXTURE_2D

ARRAY_BUFFER
ELEMENT_ARRAY_BUFFER
STATIC_DRAW
FLOAT
FRAMEBUFFER
COLOR_ATTACHMENT0

RGB
RGBA
RGBA32F
FLOAT16
FLOAT32
UNSIGNED_BYTE
UNSIGNED_SHORT
UNSIGNED_SHORT_5_6_5
UNSIGNED_SHORT_4_4_4_4
UNSIGNED_SHORT_5_5_5_1
R32F
R16F
RED
HALF_FLOAT

POINTS
LINE_STRIP
LINE_LOOP
LINES
TRIANGLE_STRIP
TRIANGLE_FAN
TRIANGLES
`;
var IsValidWebglEnum = function(Enum)
{
	if ( Enum.length == 0 )
		return false;
	if ( Enum.startsWith('//') )
		return false;
	return true;
}
WebglEnumNames = WebglEnumNames.split('\n').filter(IsValidWebglEnum);




function WebglExtension_LoseContext()
{
	this.loseContext = function()
	{
		Debug("Do loseContext");
	}
	this.restoreContext = function()
	{
		Debug("@@@@@@@@@@@@@@@@@@@@ Do restorecontext");
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
	this.VertShaderSource = null;
	this.FragShaderSource = null;
	this.Shader = null;
	this.Error = null;
	
	this.AddShader = function(Shader)
	{
		if ( Shader instanceof WebglVertShader )
			this.VertShaderSource = Shader.Source;
		else if ( Shader instanceof WebglFragShader )
			this.FragShaderSource = Shader.Source;
		else
			throw "Don't know what type of shader is supplied for program";
	}
	
}

function OpenglCommandQueue()
{
	this.Commands = [];
	this.IsCompiledMode = false;
	Debug("OpenglCommandQueue()");
	
	this.Push = function(Function,arguments)
	{
		if ( typeof Function != 'function' )
			throw Function + " is not a function. From " + this.Push.caller;
		
		//	turn arguments into an array
		//	https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/arguments
		const args = Array.from(arguments);
		//var args = (arguments.length === 1 ? [arguments[0]] : Array.apply(null, arguments));
		//Debug("args... x" + args.length);
		//Debug(args);
		args.splice( 0, 0, Function );
		this.Commands.push( args );
	}
	
	this.Flush = function(Context,Async)
	{
		let ExecuteQueue = function(Commands)
		{
			Debug("Execute Queue x" + Commands.length + (Async?" (async)":"") );
			let ExecuteCommand = function(Command)
			{
				//	first arg is the function, then pass arguments
				let Func = Command.shift();
				let Arguments = Command;
				Func.apply( Context, Arguments );
			}
			try
			{
				Commands.forEach( ExecuteCommand );
			}
			catch(e)
			{
				Debug("exception in queue: ");
				Debug(e);
				throw e;
			}
		}
		
		//	run these commands on the opengl thread
		//Debug("Running opengl command queue");
		//	capture commands and remove from our list
		let Cmds = this.Commands;
		this.Commands = [];
		let RunCommands = function()
		{
			//Debug("Flush::RunCommands");
			ExecuteQueue(Cmds);
		}
		//Debug("Flush::Context.Execute(Async="+Async+")");
		let Prom = Context.Execute( RunCommands, !Async );
		//Debug("Flush::Context return promise(Async="+Async+")");
		return Prom;
	}
}



function OpenglCompiledCommandQueue()
{
	this.Commands = [];
	this.IsCompiledMode = true;
	Debug("OpenglCompiledCommandQueue()");

	
	this.Push = function(Function,arguments)
	{
		//	turn arguments into an array
		//	https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/arguments
		const args = Array.from(arguments);
		args.splice( 0, 0, Function );
		this.Commands.push( args );
	}
	
	this.Flush = function(Context)
	{
		//Debug("flush");
		let ExecuteQueue = function(Commands)
		{
			let ExecuteCommand = function(Command)
			{
				//Debug("cmd " + Command[0] + " (x" + (Command.length-1) + " args)");
			}
			try
			{
				Commands.forEach( ExecuteCommand );
			}
			catch(e)
			{
				Debug("exception in queue: ");
				Debug(e);
				throw e;
			}
		}
		
		//	run these commands on the opengl thread
		//Debug("Running opengl command queue");
		//	capture commands and remove from our list
		let Cmds = this.Commands;
		this.Commands = [];
		let RunCommands = function(Resolve,Reject)
		{
			ExecuteQueue(Cmds);
			Resolve();
		}
		let Prom = new Promise(RunCommands);
		return Prom;
	}
}


function GetTypename(Object)
{
	let Type = typeof Object;
	if ( Object === null )
	{
		Type = "null";
	}
	else if ( Type == "object" )
	{
		Type = Object.constructor.name;
		if ( Type.length == 0 )
			Type = "Object{} (No constructor)";
	}
	
	return Type;
}


function GetAllEnums(Enums)
{
	//Debug("Pre-existing enums: ");
	//Object.keys(Enums).forEach( function(k){	Debug("InputEnum: " + k + "=" + Enums[k]);} );
		

	//	add fake enums (as names so we can identify missing ones)
	let PushEnum = function(Name)
	{
		if ( Name.length == 0 )
			return;
		if ( Name.startsWith('//') )
			return;
		
		//	enum already done
		if ( Enums[Name] !== undefined )
			return;
		
		Debug("Added filler Enum "+Name);
		Enums[Name] = Name;
	}
	WebglEnumNames.forEach( PushEnum );
	
		//	special case (incrementing enum value)
	//Enums['MAX_COMBINED_TEXTURE_IMAGE_UNITS'] = 32;
	Enums.MAX_COMBINED_TEXTURE_IMAGE_UNITS = 27;
	Enums.MAX_COMBINED_TEXTURE_IMAGE_UNITS = Enums['TEXTURE0'] + Enums.MAX_COMBINED_TEXTURE_IMAGE_UNITS;
	//MAX_COMBINED_TEXTURE_IMAGE_UNITS
/*
	 for ( let i=0;	i<This.MAX_COMBINED_TEXTURE_IMAGE_UNITS;	i++ )
	 {
	 let Name = 'TEXTURE'+i;
	 if ( This.Enums[Name] !== undefined )
	 return;
	 This.Enums[Name] = This.Enums['TEXTURE0']+i;
	 }
	 */
	
	return Enums;
}
		
function FakeOpenglContext(ContextType,ParentCanvas,OnImageCreated)
{
	let ContextsType = ParentCanvas.WebWindow.OpenglContext.constructor.name;
	Debug("FakeOpenglContext(" + ContextType + ", " + ContextsType +")");
	this.ParentCanvas = ParentCanvas;
	
	//this.CommandQueue = new OpenglCompiledCommandQueue();
	this.CommandQueue = new OpenglCommandQueue();

	//	make a new context
	let ParentContext = ParentCanvas.WebWindow.OpenglContext;
	//Debug("Parent Context is " + GetTypename(ParentContext) );
	//Debug( Object.keys(ParentContext) );
	//Debug("ParentContext==null : " + (ParentContext==null) );
	
	//	for now, share a global context, we have a problem where a context gets deleted BEFORE an image (belonging to that context) does
	//	ownership of opengl textures in a TImageWrapper needs to move into the context and remove itself... i guess
	if ( !ParentCanvas.WebWindow.SharedOpenglContext )
		ParentCanvas.WebWindow.SharedOpenglContext = new OpenglImmediateContext( ParentCanvas.WebWindow.OpenglContext );
	this.SharedOpenglContext = ParentCanvas.WebWindow.SharedOpenglContext;
	
	//  setup enums
	let Enums = GetAllEnums( this.SharedOpenglContext.GetEnums() );
	
	//	copy enums [key]=value to this.key=value
	Object.assign( this, Enums );
	
	this.COMPILE_STATUS = 'COMPILE_STATUS';
	this.MAX_TEXTURE_SIZE = 'MAX_TEXTURE_SIZE';
	this.FRAMEBUFFER_COMPLETE = 'FRAMEBUFFER_COMPLETE';
	this.VERTEX_SHADER = 'VERTEX_SHADER';
	this.FRAGMENT_SHADER = 'FRAGMENT_SHADER';
	this.COMPILE_STATUS = 'COMPILE_STATUS';
	this.LINK_STATUS = 'LINK_STATUS';
	
	//Debug("This keys after adding enums:");
	//Debug(Object.keys(this));
	//Debug("TEXTURE0=" + this.TEXTURE0 );
	//Debug("TEXTURE31=" + this.TEXTURE31 );
	
	this.GetOpenglContext = function()
	{
		return this.SharedOpenglContext;
	}

	this.getExtension = function(ExtensionName)
	{
		if ( ExtensionName == "WEBGL_lose_context" )
			return new WebglExtension_LoseContext();

		if ( ExtensionName == "EXT_color_buffer_float" )
			return new WebglExtension_EXTColorBufferFloat();
		
		Debug("ignored/failed GetExtension(" + ExtensionName + ")");
		//EXT_disjoint_timer_query_webgl2
		
		//WEBGL_get_buffer_sub_data_async
		return null;
	}
	
	if ( this.CommandQueue.IsCompiledMode )
	{
		Debug("Setting up command mode");
		//WebGLRenderingContext.getSupportedExtensions.
		this.disable = function()				{	this.CommandQueue.Push( 'ds', arguments );	}
		this.enable = function()				{	this.CommandQueue.Push( 'en', arguments );		}
		this.cullFace = function()				{	this.CommandQueue.Push( 'cf', arguments );		}
		this.bindBuffer = function()			{	this.CommandQueue.Push( 'bb', arguments );		}
		this.bufferData = function()			{	this.CommandQueue.Push( 'bd', arguments );		}
		this.bindFramebuffer = function()		{	this.CommandQueue.Push( 'bfb', arguments );		}
		this.framebufferTexture2D = function()	{	this.CommandQueue.Push( 'fbt', arguments );		}
		this.bindTexture = function()			{	this.CommandQueue.Push( 'bt', arguments );		}
		this.texImage2D = function()			{	this.CommandQueue.Push( 'ti', arguments );	}
		this.texParameteri = function()			{	this.CommandQueue.Push( 'tp', arguments );		}
		this.vertexAttribPointer = function()	{	this.CommandQueue.Push( 'vap', arguments );		}
		this.enableVertexAttribArray = function()	{	this.CommandQueue.Push( 'eva', arguments );		}
		this.texSubImage2D = function()			{	this.CommandQueue.Push( 'tsi', arguments );		}
		this.viewport = function()				{	this.CommandQueue.Push( 'vp', arguments );		}
		this.scissor = function()				{	this.CommandQueue.Push( 'sc', arguments );		}
		this.activeTexture = function()			{	this.CommandQueue.Push( 'at', arguments );		}
		this.drawElements = function()			{	this.CommandQueue.Push( 'drw', arguments );		}
		this.RealReadPixels = function()		{	this.CommandQueue.Push( 'rp', arguments );		}
	}
	else
	{
		Debug("Setting up immediate mode");
		//WebGLRenderingContext.getSupportedExtensions.
		this.disable = function()				{	this.CommandQueue.Push( this.GetOpenglContext().disable, arguments );	}
		this.enable = function()				{	this.CommandQueue.Push( this.GetOpenglContext().enable, arguments );		}
		this.cullFace = function()				{	this.CommandQueue.Push( this.GetOpenglContext().cullFace, arguments );		}
		this.bindBuffer = function()			{	this.CommandQueue.Push( this.GetOpenglContext().bindBuffer, arguments );		}
		this.bufferData = function()			{	this.CommandQueue.Push( this.GetOpenglContext().bufferData, arguments );		}
		this.bindFramebuffer = function()		{	this.CommandQueue.Push( this.GetOpenglContext().bindFramebuffer, arguments );		}
		this.framebufferTexture2D = function()	{	this.CommandQueue.Push( this.GetOpenglContext().framebufferTexture2D, arguments );		}
		this.bindTexture = function()			{	this.CommandQueue.Push( this.GetOpenglContext().bindTexture, arguments );		}
		this.texImage2D = function()			{	this.CommandQueue.Push( this.GetOpenglContext().texImage2D, arguments );	}
		this.texParameteri = function()			{	this.CommandQueue.Push( this.GetOpenglContext().texParameteri, arguments );		}
		this.vertexAttribPointer = function()	{	this.CommandQueue.Push( this.GetOpenglContext().vertexAttribPointer, arguments );		}
		this.enableVertexAttribArray = function()	{	this.CommandQueue.Push( this.GetOpenglContext().enableVertexAttribArray, arguments );		}
		this.texSubImage2D = function()			{	this.CommandQueue.Push( this.GetOpenglContext().texSubImage2D, arguments );		}
		//this.readPixels = function()			{	this.CommandQueue.Push( this.GetOpenglContext().readPixels, arguments );		}
		this.viewport = function()				{	this.CommandQueue.Push( this.GetOpenglContext().viewport, arguments );		}
		this.scissor = function()				{	this.CommandQueue.Push( this.GetOpenglContext().scissor, arguments );		}
		this.activeTexture = function()			{	this.CommandQueue.Push( this.GetOpenglContext().activeTexture, arguments );		}
		this.drawElements = function()			{	this.CommandQueue.Push( this.GetOpenglContext().drawElements, arguments );		}
		this.RealReadPixels = function()		{	this.CommandQueue.Push( this.GetOpenglContext().readPixels, arguments );		}
	}

		
	this.useProgram = function()
	{
		if ( this.CommandQueue.IsCompiledMode )
		{
			this.CommandQueue.Push( 'up', arguments );
			return;
		}
		
		//	this should be executed on the immediate thread inside Execute()
		let AllocAndUseProgram = function()
		{
			let Context = this;
			if ( arguments[0] === null )
			{
				//Debug( GetTypename(Context) + ".Use program(null)");
				Context.useProgram( null );
				Context.LastProgram = null;
			}
			else
			{
				let Program = arguments[0];
				//Debug( GetTypename(this) + ".UseProgram( " + GetTypename(Program) + ")" );
				if ( Program.Shader == null )
				{
					//Debug("Allocating shader");
					let VertShaderSource = Program.VertShaderSource;
					let FragShaderSource = Program.FragShaderSource;
					
					//	tensorflow adds some funcs that GL ES doesn't support
					//	we need to remove them
					//	todo: do all the Soy shader upgrade stuff here!
					//	gr: there are some uses that expect int return, but the default returns float
					//		and we get int x = float errors, so work around it
					VertShaderSource = VertShaderSource.replaceAll(" round(", " roundToInt(");
					FragShaderSource = FragShaderSource.replaceAll(" round(", " roundToInt(");

					let RenderTarget = Context;
					//Debug("VertShaderSource="+VertShaderSource);
					//Debug("FragShaderSource="+FragShaderSource);
					Program.Shader = new OpenglShader( RenderTarget, VertShaderSource, FragShaderSource );
				}
				this.useProgram( Program.Shader );
				Context.LastProgram = Program;
			}
		}
		this.CommandQueue.Push( AllocAndUseProgram, arguments );
	}

	//	this is deffered like useProgram's allocation
	this.SetUniform = function()
	{
		if ( this.CommandQueue.IsCompiledMode )
		{
			this.CommandQueue.Push( 'su', arguments );
			return;
		}

		//	this should be executed on the immediate thread inside Execute()
		let ShaderSetUniform = function()
		{
			let Context = this;
			let CurrentProgram = Context.LastProgram;
			let CurrentShader = CurrentProgram.Shader;
			CurrentShader.SetUniform.apply( CurrentShader, arguments );
		}
		this.CommandQueue.Push( ShaderSetUniform, arguments );
	}

	
	this.readPixels = function(x,y,w,h,format,type,output)
	{
		//	gr: getting a deadlock from media new frame(?!)
		//	this steals the opengl thread, so we need to unlock breifly
		Sleep(0);
		
		Debug("readPixels("+w+"x"+h+"=" + output.length + ", format=" + format +")");
		//Debug("readPixels(" + Array.from(arguments) + ")");
		try
		{
			//	gr: losing arguments somewhere in the chain if we pass it along
			if ( this.CommandQueue.IsCompiledMode )
			{
				this.RealReadPixels( arguments );
			}
			else
			{
				this.CommandQueue.Push( this.GetOpenglContext().readPixels, arguments );
			}

			//	don't need to return immediately
			if ( output == null )
				return;
			
			Sleep(0);
			let Async = false;
			this.CommandQueue.Flush( this.GetOpenglContext(), Async );
		}
		catch(e)
		{
			Debug("readpixels exception: " + e);
		}
	}
	
	
	//	returns a promise, which executes the command buffer
	//	this will cache the pixels internally for the next proper (synchronous) read from posenet
	this.readPixelsAsync = function(x,y,w,h,format,type,output,Texture)
	{
		Sleep(0);
		//return null;
		Debug("readPixelsAsync("+w+"x"+h+"=" + output.length + ", format=" + format +")");
		//return null;
		/*
		Debug("readPixelsAsync( " + GetTypename(Texture) + ")");
		Debug( Object.keys(Texture) );
		
		Debug("readPixelsAsync(" + Array.from(arguments) + ")");
		*/
		let Async = true;
		this.CommandQueue.Push( this.GetOpenglContext().readPixels, arguments );
		let FlushPromise = this.CommandQueue.Flush( this.GetOpenglContext(), Async );
		if ( FlushPromise.constructor.name != 'Promise' )
			throw "FlushPromise(" + (FlushPromise.constructor.name) +" not promise";
		return FlushPromise;
	}
	
	
	//	gr: these need to create objects as caller checks for return
	this.BufferCounter = 1000;
	this.FrameBufferCounter = 1000;
	this.createBuffer = function()	{	return this.BufferCounter++;	}
	this.createFramebuffer = function()	{	return this.FrameBufferCounter++;	}

	this.getParameter = function(ParameterEnum)
	{
		if ( ParameterEnum == this.MAX_TEXTURE_SIZE )
			return 8192;
		
		Debug("getParameter(" + ParameterEnum + ")" );
	}

	this.createTexture = function()
	{
		//	gr: internally the code is currently expecting some pixels
		let NewImage = new Image([1,1]);
		if ( OnImageCreated )
		{
			OnImageCreated(NewImage);
		}
		return NewImage;
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

	this.attachShader = function(Program,Shader)
	{
		Program.AddShader( Shader );
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
	

	this.linkProgram = function(Program)
	{
		//	gr: link on use
	}
	
	this.getProgramInfoLog = function(Program)
	{
		return Program.Error;
	}
	
	//	just pass name around and resolve when we need it
	this.getAttribLocation = function(Program,Name)		{	return Name;	}
	this.getUniformLocation = function(Program,Name)	{	return Name;	}
	
	
	this.uniform1f = this.SetUniform;
	this.uniform2f = this.SetUniform;
	this.uniform3f = this.SetUniform;
	this.uniform4f = this.SetUniform;
	this.uniform1fv = this.SetUniform;
	this.uniform2fv = this.SetUniform;
	this.uniform3fv = this.SetUniform;
	this.uniform4fv = this.SetUniform;
	this.uniform1i = this.SetUniform;
	this.uniform2i = this.SetUniform;
	this.uniform3i = this.SetUniform;
	this.uniform4i = this.SetUniform;
	this.uniform1iv = this.SetUniform;
	this.uniform2iv = this.SetUniform;
	this.uniform3iv = this.SetUniform;
	this.uniform4iv = this.SetUniform;


	this.getError = function()
	{
		return this.NO_ERROR;
	}

	
}

if ( OnOpenglImageCreated === undefined )
	OnOpenglImageCreated = function(img)	{};

function FakeCanvas(WebWindow)
{
	//Debug("New canvas");
	this.WebWindow = WebWindow;
	//this.Window.OnRender = function(){	/*WindowRender( Window1 );*/	};

	this.WebglContext = null;
	
	this.getContext = function(ContextType)
	{
		if ( this.WebglContext == null )
		{
			try
			{
				this.WebglContext = new FakeOpenglContext( ContextType, this, OnOpenglImageCreated );
			}
			catch(e)
			{
				Debug("Error making webglcontext: " + e);
			}
		}
		return this.WebglContext;
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


//	gr: window wrapper to emulate browser for tensor flow
var window = new FakeWindow();
var console = new FakeConsole();
var document = new FakeDocument(window);



//	to allow tensorflow to TRY and read video, (and walk past the code), we at least need a constructor for instanceof HTMLVideoElement
function HTMLVideoElement()
{
	
}


//	gr: this might need to be more intelligently back if accessing pixels synchronously
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

