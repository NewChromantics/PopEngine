
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
			Debug("Building shader...");
			//	gr: this is expected to be immediate for tensor flow/webgl, but we want a deffered version (promise)
			//		lets see if we can have all stubs for webgl api and handle it our javascript side
			//this.Shader = new OpenglShader( OpenglContext, VertSource, FragSource );
			this.Shader = {};
		}
		catch(e)
		{
			this.Error = e;
		}
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
				//throw e;
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
	this.texImage2D = function()	{	this.CommandQueue.Push( this.GetOpenglContext().texImage2D, arguments );		}
	this.useProgram = function()	{	this.CommandQueue.Push( this.GetOpenglContext().useProgram, arguments );		}
	this.texParameteri = function()	{	this.CommandQueue.Push( this.GetOpenglContext().texParameteri, arguments );		}
	this.attachShader = function()	{	this.CommandQueue.Push( this.GetOpenglContext().attachShader, arguments );		}
	this.vertexAttribPointer = function()	{	this.CommandQueue.Push( this.GetOpenglContext().vertexAttribPointer, arguments );		}
	this.enableVertexAttribArray = function()	{	this.CommandQueue.Push( this.GetOpenglContext().enableVertexAttribArray, arguments );		}
	this.SetUniform = function()	{	this.CommandQueue.Push( this.GetOpenglContext().setUniform, arguments );		}
	this.texSubImage2D = function()	{	this.CommandQueue.Push( this.GetOpenglContext().texSubImage2D, arguments );		}
	//this.readPixels = function()	{	this.CommandQueue.Push( this.GetOpenglContext().readPixels, arguments );		}
	this.viewport = function()	{	this.CommandQueue.Push( this.GetOpenglContext().viewport, arguments );		}
	this.scissor = function()	{	this.CommandQueue.Push( this.GetOpenglContext().scissor, arguments );		}
	this.activeTexture = function()	{	this.CommandQueue.Push( this.GetOpenglContext().activeTexture, arguments );		}
	this.drawElements = function()	{	this.CommandQueue.Push( this.GetOpenglContext().drawElements, arguments );		}
	

	
	this.readPixels = function()
	{
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
		//Program.Build( this.GetOpenglContext() );
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



