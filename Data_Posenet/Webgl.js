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
UNSIGNED_SHORT_5_6_5
UNSIGNED_SHORT_4_4_4_4
UNSIGNED_SHORT_5_5_5_1
R32F
R16F
RED
HALF_FLOAT
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
	
	this.Flush = function(Context)
	{
		let ExecuteQueue = function(Commands)
		{
			Debug("Execute Queue x" + Commands.length );
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
		Debug("Running opengl command queue");
		//	capture commands and remove from our list
		let Cmds = this.Commands;
		this.Commands = [];
		let RunCommands = function()
		{
			ExecuteQueue(Cmds);
		}
		let Promise = Context.Execute( RunCommands, true );
		return Promise;
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
			Type = "Object{}";
	}
	
	return Type;
}
		
function FakeOpenglContext(ContextType,ParentCanvas)
{
	let ContextsType = ParentCanvas.WebWindow.OpenglContext.constructor.name;
	Debug("FakeOpenglContext(" + ContextType + ", " + ContextsType +")");
	this.ParentCanvas = ParentCanvas;
	this.CommandQueue = new OpenglCommandQueue();

	//	setup enums
	let Enums = ParentCanvas.WebWindow.OpenglContext.Enums;
	Object.assign( this, Enums );
	
	this.MAX_COMBINED_TEXTURE_IMAGE_UNITS = 16;
	this.COMPILE_STATUS = 'COMPILE_STATUS';
	this.MAX_TEXTURE_SIZE = 'MAX_TEXTURE_SIZE';
	this.FRAMEBUFFER_COMPLETE = 'FRAMEBUFFER_COMPLETE';
	this.VERTEX_SHADER = 'VERTEX_SHADER';
	this.FRAGMENT_SHADER = 'FRAGMENT_SHADER';
	this.COMPILE_STATUS = 'COMPILE_STATUS';
	this.LINK_STATUS = 'LINK_STATUS';
	Debug(Object.keys(this));

	
	
	
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
		
		Debug("GetExtension(" + ExtensionName + ")");
		//EXT_disjoint_timer_query_webgl2
		
		//WEBGL_get_buffer_sub_data_async
		return null;
	}
	//WebGLRenderingContext.getSupportedExtensions.
	this.disable = function()	{	this.CommandQueue.Push( this.GetOpenglContext().disable, arguments );	}
	this.enable = function()		{	this.CommandQueue.Push( this.GetOpenglContext().enable, arguments );		}
	this.cullFace = function()	{	this.CommandQueue.Push( this.GetOpenglContext().cullFace, arguments );		}
	this.bindBuffer = function()	{	this.CommandQueue.Push( this.GetOpenglContext().bindBuffer, arguments );		}
	this.bufferData = function()	{	this.CommandQueue.Push( this.GetOpenglContext().bufferData, arguments );		}
	this.bindFramebuffer = function()	{	this.CommandQueue.Push( this.GetOpenglContext().bindFramebuffer, arguments );		}
	this.framebufferTexture2D = function()	{	this.CommandQueue.Push( this.GetOpenglContext().framebufferTexture2D, arguments );		}
	this.bindTexture = function()	{	this.CommandQueue.Push( this.GetOpenglContext().bindTexture, arguments );		}
	this.texImage2D = function()
	{
		let DebugStr = "texImage2D(";
		for ( let a=0;	a<arguments.length;	a++ )
			DebugStr += GetTypename( arguments[a] )+ ", ";
		DebugStr+=")";
		Debug(DebugStr);

		this.CommandQueue.Push( this.GetOpenglContext().texImage2D, arguments );
	}
	this.texParameteri = function()	{	this.CommandQueue.Push( this.GetOpenglContext().texParameteri, arguments );		}
	this.vertexAttribPointer = function()	{	this.CommandQueue.Push( this.GetOpenglContext().vertexAttribPointer, arguments );		}
	this.enableVertexAttribArray = function()	{	this.CommandQueue.Push( this.GetOpenglContext().enableVertexAttribArray, arguments );		}
	this.SetUniform = function()	{	this.CommandQueue.Push( this.GetOpenglContext().setUniform, arguments );		}
	this.texSubImage2D = function()	{	this.CommandQueue.Push( this.GetOpenglContext().texSubImage2D, arguments );		}
	//this.readPixels = function()	{	this.CommandQueue.Push( this.GetOpenglContext().readPixels, arguments );		}
	this.viewport = function()	{	this.CommandQueue.Push( this.GetOpenglContext().viewport, arguments );		}
	this.scissor = function()	{	this.CommandQueue.Push( this.GetOpenglContext().scissor, arguments );		}
	this.activeTexture = function()	{	this.CommandQueue.Push( this.GetOpenglContext().activeTexture, arguments );		}
	this.drawElements = function()	{	this.CommandQueue.Push( this.GetOpenglContext().drawElements, arguments );		}
	
	this.useProgram = function()
	{
		//	this should be executed on the immediate thread inside Execute()
		let AllocAndUseProgram = function()
		{
			let Context = this;
			if ( arguments[0] === null )
			{
				Debug( GetTypename(Context) + ".Use program(null)");
				Context.useProgram( null );
			}
			else
			{
				let Program = arguments[0];
				Debug( GetTypename(this) + ".UseProgram( " + GetTypename(Program) + ")" );
				if ( Program.Shader == null )
				{
					//Debug("Allocating shader");
					let VertShaderSource = Program.VertShaderSource;
					let FragShaderSource = Program.FragShaderSource;
					
					//	tensorflow adds some funcs that GL ES doesn't support
					//	we need to remove them
					//	todo: do all the Soy shader upgrade stuff here!
					VertShaderSource = VertShaderSource.replace("int round(float", "int IGNORETHISFUNC_round(float");
					FragShaderSource = FragShaderSource.replace("int round(float", "int IGNORETHISFUNC_round(float");

					let RenderTarget = Context;
					//Debug("VertShaderSource="+VertShaderSource);
					//Debug("FragShaderSource="+FragShaderSource);
					Program.Shader = new OpenglShader( RenderTarget, VertShaderSource, FragShaderSource );
				}
				this.useProgram( Program.Shader );
			}
		}
		this.CommandQueue.Push( AllocAndUseProgram, arguments );
	}

	
	this.readPixels = function()
	{
		//	work out what we're reading into
		Debug("ReadPixels into " + arguments[6].constructor.name);
		
		this.CommandQueue.Push( this.GetOpenglContext().readPixels, arguments );
		this.CommandQueue.Flush( this.GetOpenglContext() );
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
		return new Image([1,1]);
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
			this.WebglContext = new FakeOpenglContext( ContextType, this );
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



