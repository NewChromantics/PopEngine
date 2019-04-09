let Debug = Pop.Debug;

let VertShader = Pop.LoadFileAsString('Quad.vert.glsl');
let Uvy844FragShader = Pop.LoadFileAsString('Uvy844.frag.glsl');
let Yuv888FragShader = Pop.LoadFileAsString('Yuv8_88.frag.glsl');
let Yuv8888FragShader = Pop.LoadFileAsString('Yuv8888.frag.glsl');
let BlitFragShader = Pop.LoadFileAsString('Blit.frag.glsl');

//let GetChromaUvy844Shader = Pop.LoadFileAsString('GetChroma_Uvy844.frag.glsl');


let ColourTextureCache = [];
let GetColourHash = function(Colour4)
{
	let r = Math.floor( Colour4[0] * 255 );
	let g = Math.floor( Colour4[1] * 255 );
	let b = Math.floor( Colour4[2] * 255 );
	let a = Math.floor( Colour4[3] * 255 );
	let Hash = (r<<0) | (g<<8) | (b<<16) | (a<<24);
	return Hash;
}

let GetColourTexture = function(Colour4)
{
	let Hash = GetColourHash(Colour4);
	if ( ColourTextureCache.hasOwnProperty(Hash) )
		return ColourTextureCache[Hash];
	
	ColourTextureCache[Hash] = CreateColourTexture( Colour4 );
	return ColourTextureCache[Hash];
}


function TCameraWindow(CameraName)
{
	this.NullTexture = CreateColourTexture( [0.3,0.0,0.0,1] );
	this.FrameTexture = null;
	this.CameraFrameCounter = new TFrameCounter( CameraName );
	this.CameraFrameCounter.Report = function(FrameRate)	{	Debug( CameraName + " @" + FrameRate);	}

	this.OnRender = function()
	{
		let RenderTarget = this.Window;
		let FragShader = GetShader( RenderTarget, Uvy844FragShader );
		//let FragShader = GetShader( RenderTarget, Yuv8888FragShader );
		//let FragShader = GetShader( RenderTarget, BlitFragShader );
		let FrameTexture = this.FrameTexture ? this.FrameTexture : this.NullTexture;
		let SetUniforms = function(Shader)
		{
			Shader.SetUniform("Texture", FrameTexture );
			Shader.SetUniform("TextureWidth", FrameTexture.GetWidth() );
			Shader.SetUniform("LumaTexture", FrameTexture );
			Shader.SetUniform("ChromaTexture", this.NullTexture );
		}
		RenderTarget.DrawQuad( FragShader, SetUniforms.bind(this) );
	}
	
	this.GetChromaPlane = async function(Frame)
	{
		if ( Frame.Planes.length == 0 )
			throw "Frame has no planes";
		if ( Frame.Planes.length > 2 )
			throw "Frame has x" + Frame.Planes.length + " planes, don't know what to do";
		
		if ( Frame.Planes.length == 2 )
		{
			let Chroma = Frame.Planes[1];
			Frame.Planes[0].Clear();
			Frame.Planes.length = 0;
			return Chroma;
		}
		
		//	todo:
		let Chroma = Frame.Planes[0];
		Frame.Planes.length = 0;
		return Chroma;
		
		//	one plane
		let OnePlaneFormat = Frame.Planes[0].Format;
		if ( OnePlaneFormat != 'Uvy_844' )
			throw "Todo: process " + OnePlaneFormat;
		
		//GetChromaUvy844Shader
		throw "Todo: process " + OnePlaneFormat;
	}
	
	this.ProcessVideoFrame = async function(Frame)
	{
		if ( this.FrameTexture )
			this.FrameTexture.Clear();
		
		this.FrameTexture = await this.GetChromaPlane(Frame);
	}
	
	this.ListenForFrames = async function()
	{
		while ( true )
		{
			try
			{
				let Stream = 0;
				let Latest = true;
				await Pop.Yield(4);
				let NextFrame = await this.Source.GetNextFrame( [], Stream, Latest );
				
				await this.ProcessVideoFrame(NextFrame);

				this.CameraFrameCounter.Add();
			}
			catch(e)
			{
				//	sometimes OnFrameExtracted gets triggered, but there's no frame? (usually first few on some cameras)
				//	so that gets passed up here. catch it, but make sure we re-request
				if ( e != "No frame packet buffered" )
					Debug( CameraName + " ListenForFrames: " + e);
			}
		}
	}
	
	this.Window = new Pop.Opengl.Window(CameraName);
	this.Window.OnRender = this.OnRender.bind(this);
	this.Window.OnMouseMove = function(){};
	this.Window.OnMouseDown = function(){};
	this.Window.OnMouseUp = function(){};
	this.Source = new Pop.Media.Source(CameraName);
	this.ListenForFrames().catch(Debug);
	
}


let CameraWindows = [];

async function FindCamerasLoop()
{
	let CreateCamera = function(CameraName)
	{
		if ( CameraWindows.hasOwnProperty(CameraName) )
		{
			Debug("Already have window for " + CameraName);
			return;
		}
		
		try
		{
			let Window = new TCameraWindow(CameraName);
			//CameraWindows.push(Window);
		}
		catch(e)
		{
			Debug(e);
		}
	}
	
	while ( true )
	{
		try
		{
			let Devices = await Pop.Media.EnumDevices();
			Debug("Pop.Media.EnumDevices found(" + Devices + ") result type=" + (typeof Devices) );
			Devices.forEach( CreateCamera );
			await Pop.Yield( 1 );
			
			//	todo: EnumDevices needs to change to "OnDevicesChanged"
			break;
		}
		catch(e)
		{
			Debug("FindCamerasLoop error: " + e );
		}
	}
}

//	start tracking cameras
FindCamerasLoop().catch(Debug);
