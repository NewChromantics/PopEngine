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

var RefineUi_FragShaderSource = LoadFileAsString('Data/RefineUi.frag');
var RefineUi_Shader = null;


var UserCoords = [];
var UserCoordViewport = [0,0,1,1];

//  using nomenclature from auto-resolver
var LastFrame = null;

function Range(Min,Max,Value)
{
	return (Value-Min) / (Max-Min);
}

function Clamp01(Value)
{
	return Math.min( 1, Math.max( 0, Value ) );
}

function Range01(Min,Max,Value)
{
	return Clamp01( Range(Min,Max,Value) );
}

function Range01InRect(Rect,Pos2)
{
	let w = Rect[2];
	let h = Rect[3];
	Pos2[0] = Range01( Rect[0], w, Pos2[0] );
	Pos2[1] = Range01( Rect[1], h, Pos2[1] );
	return Pos2;
}

function WindowRender(RenderTarget)
{
	RenderTarget.ClearColour(0,1,1);
	
	if ( !RefineUi_Shader )
		RefineUi_Shader = new OpenglShader( RenderTarget, VertShaderSource, RefineUi_FragShaderSource );
	
	let SetUniforms = function(Shader)
	{
		Shader.SetUniform("Positions", UserCoords );
	}
	
	//RenderTarget.SetViewport( UserCoordViewport );
	RenderTarget.DrawQuad( DebugFrameShader, SetUniforms );

}

function OnNewFrame(NewFrame)
{
    LastFrame = NewFrame;
}

function OnError(Error)
{
    Debug(Error);
    LastFrame = null;
}

function CalculateFrame(MatchRects)
{
    //  setup frame with params
    let Frame = {};
    
    let RunCalcFrame = function(Resolve,Reject)
    {
        //  run the usual kernels
        Resolve();
    };
    
    let Prom = MakePromise( RunCalcFrame );
    return Prom;
}

function OnUserCoordsChanged(UserCoords)
{
    //  recalc frame
    let Rect = [ UserCoords ];
    let Rects = [Rect];
    
    //  trigger frame calculation
    CalculateFrame(Rects).
    then( OnNewFrame ).
    catch( OnError );
}

function OnClick(uv)
{
    //  add a new coord and cap to 4
	UserCoords.push( Range01InRect(UserCoordViewport,uv) );
	UserCoords = UserCoords.slice(-4);
	OnUserCoordsChanged();
}



function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Auto Refine UI");
	Window1.OnRender = function(){	WindowRender( Window1 );	};
	Window1.OnClick = OnClick;

	/*
	let OpenclDevices = OpenclEnumDevices();
	Debug("Opencl devices x" + OpenclDevices.length );
	if ( OpenclDevices.length == 0 )
		throw "No opencl devices";
	OpenclDevices.forEach( Debug );
	let Opencl = new OpenclContext( OpenclDevices[0] );
    */
}

//	main
Main();
