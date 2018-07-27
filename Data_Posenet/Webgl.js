
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
		let ExecuteQueue = function()
		{
			let ExecuteCommand = function(Command)
			{
				//	first arg is the function, then pass arguments
				let Func = Command.shift();
				Func( Command );
			}
			
			try
			{
				this.Commands.forEach( ExecuteCommand );
			}
			catch(e)
			{
				this.Commands = [];
				throw e;
			}
		}
		
		//	run these commands on the opengl thread
		Debug("Running opengl command queue");
		Context.Execute( ExecuteQueue );
	}
}

function FakeOpenglContext(ContextType,ParentCanvas)
{
	Debug("FakeOpenglContext(" + ContextType + ")");

	//	constants, we just need arbritry enum values
	let CONST = 1;
	
	this.NO_ERROR = CONST++;
	this.INVALID_ENUM = CONST++;
	this.INVALID_VALUE = CONST++;
	this.INVALID_OPERATION = CONST++;
	this.INVALID_FRAMEBUFFER_OPERATION = CONST++;
	this.OUT_OF_MEMORY = CONST++;
	this.CONTEXT_LOST_WEBGL = CONST++;
	
	this.FRAMEBUFFER_COMPLETE = CONST++;
	this.VERTEX_SHADER = CONST++;
	this.FRAGMENT_SHADER = CONST++;
	this.TEXTURE0 = CONST++;
	this.TEXTURE1 = CONST++;
	this.TEXTURE2 = CONST++;
	this.TEXTURE3 = CONST++;
	this.TEXTURE4 = CONST++;
	this.TEXTURE5 = CONST++;
	this.TEXTURE6 = CONST++;
	this.TEXTURE7 = CONST++;
	this.TEXTURE8 = CONST++;
	this.TEXTURE9 = CONST++;
	this.TEXTURE10 = CONST++;
	this.TEXTURE11 = CONST++;
	this.TEXTURE12 = CONST++;
	this.TEXTURE13 = CONST++;
	this.TEXTURE14 = CONST++;
	this.TEXTURE15 = CONST++;
	
	//	Parameters
	this.GPU_DISJOINT_EXT = CONST++;
	this.MAX_TEXTURE_SIZE = CONST++;
	
	
	this.ParentCanvas = ParentCanvas;
	this.CommandQueue = new OpenglCommandQueue();
	
	
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
	this.SetUniform = function()	{	this.CommandQueue.Push( this.GetOpenglContext().SetUniform, arguments );		}
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
	
	
	
	this.createBuffer = function()
	{
		let NewBuffer = new WebglDataBuffer();
		return NewBuffer;
	}
	
	this.getParameter = function(ParameterEnum)
	{
		if ( ParameterEnum == this.MAX_TEXTURE_SIZE )
			return 8192;
		
		Debug("getParameter(" + ParameterEnum + ")" );
	}

	this.createFramebuffer = function()
	{
		this.FrameBufferCounter++;
		let NewBuffer = new WebglFrameBuffer(this.FrameBufferCounter);
		return NewBuffer;
	}
	
	this.createTexture = function()
	{
		return new Image();
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
	

	this.getAttribLocation = function(Program,Name)
	{
		return Name;
		//	return -1 if not found
		return 0;
	}
	
	
	this.getUniformLocation = function(Program,Name)
	{
		//	return -1 if not found
		return Name;
		return 0;
	}
	
	
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


//	gr: window wrapper to emulate browser for tensor flow
var window = new FakeWindow();
var console = new FakeConsole();
var document = new FakeDocument(window);



