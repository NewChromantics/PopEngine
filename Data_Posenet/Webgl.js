
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
	
	this.disable = function(GlStateEnum)	{	Debug("gldisable(" + GlStateEnum +")");	}
	this.enable = function(GlStateEnum)		{	Debug("glenable(" + GlStateEnum +")");	}
	this.cullFace = function(CullFaceEnum)	{	Debug("cullFace(" + CullFaceEnum +")");	}

	this.getParameter = function(ParameterEnum)
	{
		if ( ParameterEnum == this.MAX_TEXTURE_SIZE )
			return 8192;
		
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
	
	this.useProgram = function(Program)
	{
		//	current program = x
	}
	
	this.getAttribLocation = function(Program,Name)
	{
		return Name;
		//	return -1 if not found
		return 0;
	}
	
	this.vertexAttribPointer = function(index, size, type, normalized, stride, offset)
	{
	}
	
	this.enableVertexAttribArray = function(index)
	{
		
	}
	
	this.getUniformLocation = function(Program,Name)
	{
		//	return -1 if not found
		return Name;
		return 0;
	}
	
	this.SetUniform = function(Location,Value)	{	Debug("SetUniform(" + Location +")");	}
	
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

	this.drawElements = function(PrimitiveEnum, count, type, offset)
	{
		
	}

	//	lots of variants;
	//	https://developer.mozilla.org/en-US/docs/Web/API/WebGLRenderingContext/texSubImage2D
	this.texSubImage2D = function(Binding,MipLevel,xoffset,yoffset,MoreParams)	{}
	
	this.readPixels = function(x, y, width, height, format, type, pixels, offset)
	{
		//	pixels is optional
		if ( offset === undefined )
		{
			offset = pixels;
			pixels = undefined;
		}
	}
	
	this.viewport = function(x, y, width, height)
	{
		
	}
	
	this.scissor = function(x, y, width, height)
	{
		
	}
	
	this.activeTexture = function(TextureEnum)
	{
		
	}
	
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
		//	gr: go with cpu for now
		return null;
		
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



