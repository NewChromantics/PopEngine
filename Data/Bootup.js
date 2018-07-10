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

let ImageFragShaderSource = `
	#version 410
	in vec2 uv;
	uniform sampler2D Image;
	void main()
	{
		vec2 Flippeduv = vec2( uv.x, 1-uv.y );
		gl_FragColor = texture( Image, Flippeduv );
		//gl_FragColor *= vec4(uv.x,uv.y,0,1);
	}
`;

let DebugFrameFragShaderSource = LoadFileAsString('Data/DebugFrame.frag');
let RgbToHslFragShaderSource = LoadFileAsString('Data/FrameToHsl.frag');
let GrassFilterFragShaderSource = LoadFileAsString('Data/GrassFilter.frag');
let GrassLineFilterFragShaderSource = LoadFileAsString('Data/GrassLineFilter.frag');
let DrawLinesFragShaderSource = LoadFileAsString('Data/DrawLines.frag');
let DrawCornersFragShaderSource = LoadFileAsString('Data/DrawCorners.frag');
let TestLinesKernelSource = LoadFileAsString('Data/TestLines.cl');
let TestLinesKernelName = 'GetTestLines';
let HoughLinesKernelSource = LoadFileAsString('Data/HoughLines.cl');
let CalcAngleXDistanceXChunksKernelName = 'CalcAngleXDistanceXChunks';
let GraphAngleXDistancesKernelName = 'GraphAngleXDistances';


let EdgeFragShaderSource = `
	#version 410
	in vec2 uv;
	uniform sampler2D Image;

	float GetLum(vec3 rgb)
	{
		float lum = max( rgb.x, max( rgb.y, rgb.z ) );
		return lum;
	}

	float GetLumSample(vec2 uvoffset)
	{
		vec3 rgb = texture( Image, uv+uvoffset ).xyz;
		return GetLum(rgb);
	}

	void main()
	{
		vec2 ImageSize = vec2( 1280, 720 );
		vec2 uvstep2 = 1.0 / ImageSize;
		#define NeighbourCount	(3*3)
		float NeighbourLums[NeighbourCount];
		vec2 NeighbourSteps[NeighbourCount] =
		vec2[](
			vec2(-1,-1),	vec2(0,-1),	vec2(1,-1),
			vec2(-1,0),	vec2(0,0),	vec2(1,-1),
			vec2(-1,1),	vec2(0,1),	vec2(1,1)
		);
		
		for ( int n=0;	n<NeighbourCount;	n++ )
		{
			NeighbourLums[n] = GetLumSample( NeighbourSteps[n] * uvstep2 );
		}
		
		float BiggestDiff = 0;
		float ThisLum = NeighbourLums[4];
		for ( int n=0;	n<NeighbourCount;	n++ )
		{
			float Diff = abs( ThisLum - NeighbourLums[n] );
			BiggestDiff = max( Diff, BiggestDiff );
		}
		
		if ( BiggestDiff > 0.1 )
			gl_FragColor = vec4(1,1,1,1);
		else
			gl_FragColor = vec4(0,0,0,1);
	}
`;


var DrawImageShader = null;
var DebugShader = null;
var EdgeShader = null;
var DebugFrameShader = null;
var LastProcessedFrame = null;
var RgbToHslShader = null;
var GrassFilterShader = null;
var GrassLineFilterShader = null;
var DrawLinesShader = null;
var DrawCornersShader = null;
var TestLinesKernel = null;
var CalcAngleXDistanceXChunksKernel = null;
var GraphAngleXDistancesKernel = null;

function GetRgbToHslShader(OpenglContext)
{
	if ( !RgbToHslShader )
	{
		RgbToHslShader = new OpenglShader( OpenglContext, VertShaderSource, RgbToHslFragShaderSource );
	}
	return RgbToHslShader;
}

function GetGrassFilterShader(OpenglContext)
{
	if ( !GrassFilterShader )
	{
		GrassFilterShader = new OpenglShader( OpenglContext, VertShaderSource, GrassFilterFragShaderSource );
	}
	return GrassFilterShader;
}

function GetGrassLineFilterShader(OpenglContext)
{
	if ( !GrassLineFilterShader )
	{
		GrassLineFilterShader = new OpenglShader( OpenglContext, VertShaderSource, GrassLineFilterFragShaderSource );
	}
	return GrassLineFilterShader;
}

function GetDrawLinesShader(OpenglContext)
{
	if ( !DrawLinesShader )
	{
		DrawLinesShader = new OpenglShader( OpenglContext, VertShaderSource, DrawLinesFragShaderSource );
	}
	return DrawLinesShader;
}

function GetDrawCornersShader(OpenglContext)
{
	if ( !DrawCornersShader )
	{
		DrawCornersShader = new OpenglShader( OpenglContext, VertShaderSource, DrawCornersFragShaderSource );
	}
	return DrawCornersShader;
}

function GetTestLinesKernel(OpenclContext)
{
	if ( !TestLinesKernel )
	{
		TestLinesKernel = new OpenclKernel( OpenclContext, TestLinesKernelSource, TestLinesKernelName );
	}
	return TestLinesKernel;
}

function GetCalcAngleXDistanceXChunksKernel(OpenclContext)
{
	if ( !CalcAngleXDistanceXChunksKernel )
	{
		CalcAngleXDistanceXChunksKernel = new OpenclKernel( OpenclContext, HoughLinesKernelSource, CalcAngleXDistanceXChunksKernelName );
	}
	return CalcAngleXDistanceXChunksKernel;
}

function GetGraphAngleXDistancesKernel(OpenclContext)
{
	if ( !GraphAngleXDistancesKernel )
	{
		GraphAngleXDistancesKernel = new OpenclKernel( OpenclContext, HoughLinesKernelSource, GraphAngleXDistancesKernelName );
	}
	return GraphAngleXDistancesKernel;
}



function MakePromise(Func)
{
	return new Promise( Func );
}

function GetNumberRangeInclusive(Min,Max,Steps)
{
	let Numbers = [];
	for ( let t=0;	t<=1;	t+=1/Steps)
	{
		let v = Min + (t * (Max-Min));
		Numbers.push( v );
	}
	return Numbers;
}
	
function ReturnSomeString()
{
	return "Hello world";
}


function MakeHsl(OpenglContext,Frame)
{
	let Render = function(RenderTarget,RenderTargetTexture)
	{
		let Shader = GetRgbToHslShader(RenderTarget);

		let SetUniforms = function(Shader)
		{
			Shader.SetUniform("Frame", Frame, 0 );
		}
		
		RenderTarget.DrawQuad( Shader, SetUniforms );
	}
	
	Frame.Hsl = new Image( [Frame.GetWidth(),Frame.GetHeight() ] );
	let Prom = OpenglContext.Render( Frame.Hsl, Render );
	return Prom;
}


function MakeGrassMask(OpenglContext,Frame)
{
	let Render = function(RenderTarget,RenderTargetTexture)
	{
		let Shader = GetGrassFilterShader(RenderTarget);
		
		let SetUniforms = function(Shader)
		{
			Shader.SetUniform("Frame", Frame, 0 );
			Shader.SetUniform("hsl", Frame.Hsl, 1 );
		}
		
		RenderTarget.DrawQuad( Shader, SetUniforms );
	}
	
	let GrassMaskScale = 1/10;
	let GrassMaskWidth = Frame.GetWidth() * GrassMaskScale;
	let GrassMaskHeight = Frame.GetHeight() * GrassMaskScale;
	Frame.GrassMask = new Image( [GrassMaskWidth, GrassMaskHeight] );
	Frame.GrassMask.SetLinearFilter(true);
	let Prom = OpenglContext.Render( Frame.GrassMask, Render );
	return Prom;
}


function MakeLineMask(OpenglContext,Frame)
{
	let Render = function(RenderTarget,RenderTargetTexture)
	{
		let Shader = GetGrassLineFilterShader(RenderTarget);
		
		let SetUniforms = function(Shader)
		{
			Shader.SetUniform("Hsl", Frame.Hsl, 0 );
			Shader.SetUniform("GrassMask", Frame.GrassMask, 1 );
		}
		
		RenderTarget.DrawQuad( Shader, SetUniforms );
	}
	
	let LineMaskWidth = Frame.GetWidth()/2;
	let LineMaskHeight = Frame.GetHeight()/2;
	Frame.LineMask = new Image( [LineMaskWidth,LineMaskHeight] );
	let ReadBackTexture = true;
	let Prom = OpenglContext.Render( Frame.LineMask, Render, ReadBackTexture );
	return Prom;
}

function ExtractTestLines(Frame)
{
	let BoxLines = [
					[0,0,1,0],
					[0,1,1,1],
					[0,0,0,1],
					[1,0,1,1]
					];
	
	let Runner = function(Resolve,Reject)
	{
		if ( !Array.isArray(Frame.Lines) )
			Frame.Lines = new Array();
		Frame.Lines.push(...BoxLines);
		Resolve();
	}
	
	//	high level promise
	var Prom = new Promise( Runner );
	return Prom;
}

function ExtractOpenclTestLines(OpenclContext,Frame)
{
	Debug("Opencl ExtractLines");
	let Kernel = GetTestLinesKernel(OpenclContext);
	
	let OnIteration = function(Kernel,IterationIndexes)
	{
		//Debug("OnIteration(" + Kernel + ", " + IterationIndexes + ")");
		let LineBuffer = new Float32Array( 10*4 );
		let LineCount = new Int32Array(1);
		Kernel.SetUniform("Lines", LineBuffer );
		Kernel.SetUniform("LineCount", LineCount );
		Kernel.SetUniform("LinesSize", LineBuffer.length/4 );
	}
	
	let OnFinished = function(Kernel)
	{
		//Debug("OnFinished(" + Kernel + ")");
		let LineCount = Kernel.ReadUniform("LineCount");
		let Lines = Kernel.ReadUniform("Lines");
		if ( !Array.isArray(Frame.Lines) )
			Frame.Lines = new Array();
		Frame.Lines.push(...Lines);
		Debug("Output linecount=" + LineCount);
	}

	let Prom = OpenclContext.ExecuteKernel( Kernel, [1], OnIteration, OnFinished );
	return Prom;
}

function GraphAngleXDistances(OpenclContext,Frame)
{
	let Kernel = GetGraphAngleXDistancesKernel(OpenclContext);
	Frame.HoughHistogram = new Image( [Frame.LineMask.GetWidth(),Frame.LineMask.GetHeight()] );
	//Debug(Frame.AngleXDistanceXChunks);

	let OnIteration = function(Kernel,IterationIndexes)
	{
		Debug("GraphAngleXDistances OnIteration(" + Kernel + ", " + IterationIndexes + ")");
		Kernel.SetUniform('xFirst', IterationIndexes[0] );
		Kernel.SetUniform('yFirst', IterationIndexes[1] );

		
		if ( IterationIndexes[0]==0 && IterationIndexes[1]==0 )
		{
			Kernel.SetUniform('AngleCount', Frame.Angles.length );
			Kernel.SetUniform('DistanceCount', Frame.Distances.length );
			Kernel.SetUniform('ChunkCount', Frame.Params.ChunkCount );
			Kernel.SetUniform('HistogramHitMax', Frame.Params.HistogramHitMax );
			Kernel.SetUniform('AngleXDistanceXChunks', Frame.AngleXDistanceXChunks );
			Kernel.SetUniform('AngleXDistanceXChunkCount', Frame.AngleXDistanceXChunks.length );
			Kernel.SetUniform('EdgeTexture', Frame.LineMask );
			Kernel.SetUniform('GraphTexture', Frame.HoughHistogram );
		}
	}
	
	let OnFinished = function(Kernel)
	{
		Frame.HoughHistogram = Kernel.ReadUniform('GraphTexture');
		Debug("GraphAngleXDistances OnFinished(" + Kernel + ") histogram: " + Frame.HoughHistogram.GetWidth() + "x" + Frame.HoughHistogram.GetHeight() );
	}
	
	let Dim = [ Frame.HoughHistogram.GetWidth(), Frame.HoughHistogram.GetHeight() ];
	Debug("GraphAngleXDistances Dim=" + Dim);
	let Prom = OpenclContext.ExecuteKernel( Kernel, Dim, OnIteration, OnFinished );
	return Prom;
}

function CalcAngleXDistanceXChunks(OpenclContext,Frame)
{
	let Kernel = GetCalcAngleXDistanceXChunksKernel(OpenclContext);
	let MaskTexture = Frame.LineMask;
	Frame.Angles = GetNumberRangeInclusive( 0, 179, Frame.Params.AngleCount );
	let DistanceRange = 0.68;
	Frame.Distances = GetNumberRangeInclusive( -DistanceRange, DistanceRange, Frame.Params.DistanceCount );
	Frame.AngleXDistanceXChunks = new Uint32Array( Frame.Angles.length * Frame.Distances.length * Frame.Params.ChunkCount );
	Debug("Frame.AngleXDistanceXChunks.length = " + Frame.AngleXDistanceXChunks.length);
	
	Frame.GetAngleXDistanceXChunkIndex = function(AngleIndex,DistanceIndex,ChunkIndex)
	{
		AngleIndex = Math.floor(AngleIndex);
		DistanceIndex = Math.floor(DistanceIndex);
		ChunkIndex = Math.floor(ChunkIndex);
		let DistanceCount = Frame.Distances.length;
		let ChunkCount = Frame.Params.ChunkCount;
		let AngleXDistanceXChunkIndex = (AngleIndex * DistanceCount * ChunkCount);
		AngleXDistanceXChunkIndex += DistanceIndex * ChunkCount;
		AngleXDistanceXChunkIndex += ChunkIndex;
		return AngleXDistanceXChunkIndex;
	}
	
	let OnIteration = function(Kernel,IterationIndexes)
	{
		Debug("CalcAngleXDistanceXChunks OnIteration(" + Kernel + ", " + IterationIndexes + ")");
		Kernel.SetUniform('xFirst', IterationIndexes[0] );
		Kernel.SetUniform('yFirst', IterationIndexes[1] );
		Kernel.SetUniform('AngleIndexFirst', IterationIndexes[2] );
		if ( IterationIndexes[0]==IterationIndexes[1]==IterationIndexes[2]==0 )
		{
			Kernel.SetUniform('Angles', Frame.Angles );
			Kernel.SetUniform('Distances', Frame.Distances );
			Kernel.SetUniform('DistanceCount', Frame.Distances.length );
			Kernel.SetUniform('ChunkCount', Frame.Params.ChunkCount );
			Kernel.SetUniform('HistogramHitMax', Frame.Params.HistogramHitMax );
			Kernel.SetUniform('EdgeTexture', MaskTexture );
			Kernel.SetUniform('AngleXDistanceXChunks', Frame.AngleXDistanceXChunks );
			Kernel.SetUniform('AngleXDistanceXChunkCount', Frame.AngleXDistanceXChunks.length );
		}
	}
	
	let OnFinished = function(Kernel)
	{
		Debug("CalcAngleXDistanceXChunks OnFinished(" + Kernel + ")");
		Frame.AngleXDistanceXChunks = Kernel.ReadUniform('AngleXDistanceXChunks');
		//Debug(Frame.AngleXDistanceXChunks);
		Debug("readuniform Frame.AngleXDistanceXChunks.length = " + Frame.AngleXDistanceXChunks.length);
		
	}

	let Dim = [MaskTexture.GetWidth(),MaskTexture.GetHeight(), Frame.Angles.length];
	Debug("CalcAngleXDistanceXChunks Dim=" + Dim);
	let Prom = OpenclContext.ExecuteKernel( Kernel, Dim, OnIteration, OnFinished );
	return Prom;
}


function AddTestAngleXDistanceXChunks(Frame)
{
	let WriteTest = function(Resolve)
	{
		let AngleIndex = 0 * Frame.Angles.length;
		let DistanceIndex = 0.7 * Frame.Distances.length;
		let ChunkIndex = 0.5 * Frame.Params.ChunkCount;
		AngleIndex = 3;
		//ChunkIndex-=5;
		Debug("ChunkIndex="+ChunkIndex);
		//for ( let DistanceIndex=0;	DistanceIndex<Frame.Distances.length;	DistanceIndex+=3 )
		for ( let AngleIndex=0;	AngleIndex<Frame.Angles.length;	AngleIndex+=10 )
		for ( let ChunkIndex=0;	ChunkIndex<Frame.Params.ChunkCount;	ChunkIndex+=2 )
		//for ( let ChunkIndex=1;	ChunkIndex<Frame.Params.ChunkCount-1;	ChunkIndex++ )
		{
			let adc_index = Frame.GetAngleXDistanceXChunkIndex( AngleIndex, DistanceIndex, ChunkIndex );
			Frame.AngleXDistanceXChunks[adc_index] += Frame.Params.HistogramHitMax;
		}
		Resolve();
	}
	
	let Prom = MakePromise( WriteTest );
	return Prom;
}

function ExtractHoughLines(OpenclContext,Frame)
{
	let Angles = Frame.Angles;
	let Distances = Frame.Distances;
	let DistanceCount = Distances.length;
	let ChunkCount = Frame.Params.ChunkCount;
	let HoughLines = [];
	let AngleXDistanceXChunks = Frame.AngleXDistanceXChunks;
	let AngleXDistanceXChunkCount = AngleXDistanceXChunks.length;
	Debug("ExtractHoughLines..");
	
	var BiggestScore = 0;
	let GetHoughLineScore = function(AngleIndex,DistanceIndex,ChunkIndex)
	{
		if ( AngleIndex < 0 || AngleIndex >= Frame.Angles.length )		return null;
		if ( DistanceIndex < 0 || DistanceIndex >= Frame.Distances.length )		return null;
		if ( ChunkIndex < 0 || ChunkIndex >= Frame.Params.ChunkCount )		return null;
		let HistogramIndex = Frame.GetAngleXDistanceXChunkIndex( AngleIndex, DistanceIndex, ChunkIndex );
		let HitCount = AngleXDistanceXChunks[HistogramIndex];
		let Score = HitCount / Frame.Params.HistogramHitMax;
		BiggestScore = Math.max( BiggestScore, Score );
		return Score;
	}
	
	let HasBetterNeighbour = function(ThisScore,AngleIndex,DistanceIndex,ChunkIndex,Ranges)
	{
		let Neighbours = [];
		
		let AngleRange = Ranges.AngleRange;
		let DistanceRange = Ranges.DistanceRange;
		let ChunkRange = (Frame.Params.ExtendChunks===true) ? Ranges.ChunkRange : 0;
		for ( let a=-AngleRange;	a<=AngleRange;	a++ )
			for ( let d=-DistanceRange;	d<=DistanceRange;	d++ )
				for ( let c=-ChunkRange;	c<=ChunkRange;	c++ )
					if ( !(a==0&&d==0&&c==0) )
						Neighbours.push( [a,d,c] );

		for ( let n=0;	n<Neighbours.length;	n++ )
		{
			let a = Neighbours[n][0];
			let d = Neighbours[n][1];
			let c = Neighbours[n][2];
			let ns = GetHoughLineScore( AngleIndex+a, DistanceIndex+d, ChunkIndex+c );
			if ( ns === null )
				continue;
			if ( ns > ThisScore )
			{
				return true;
			}
		}
		return false;
	}
	
	let GetHoughLine = function(AngleIndex,DistanceIndex,ChunkIndex)
	{
		let Score = GetHoughLineScore( AngleIndex, DistanceIndex, ChunkIndex );
		if ( Score < Frame.Params.ExtractHoughLineMinScore )
			return;
		
		//	see if we have a better neighbour
		if ( Frame.Params.SkipIfBetterNeighbourRanges !== undefined )
			if ( HasBetterNeighbour( Score, AngleIndex, DistanceIndex, ChunkIndex, Frame.Params.SkipIfBetterNeighbourRanges ) )
				return;
		
		let Line = {};
		Line.Origin = [Frame.Params.HoughOriginX, Frame.Params.HoughOriginY];
		Line.Angle = Angles[AngleIndex];
		Line.Distance = Distances[DistanceIndex];
		Line.AngleIndex = AngleIndex;
		Line.DistanceIndex = DistanceIndex;
		Line.ChunkIndex = ChunkIndex;
		Line.Score = Score;
		HoughLines.push( Line );
		
	}
	
	for ( ai=0;	ai<Angles.length;	ai++ )
		for ( di=0;	di<Distances.length;	di++ )
			for ( ci=0;	ci<ChunkCount;	ci++ )
				GetHoughLine( ai, di, ci );
	
	let CompareScore = function(ha,hb)
	{
		if ( ha.Score > hb.Score )
			return -1;
		if ( ha.Score < hb.Score )
			return 1;
		return 0;
	};
	
	let hypotenuse = function(o,a)			{	return Math.sqrt( (a*a)+(o*o) );	}
	let DegreesToRadians = function(Degrees){	return Degrees * (Math.PI / 180);	}
	
	//	get lines to render
	let HoughLineToLine = function(Angle,Distance,ChunkStartTime,ChunkEndTime,OriginX,OriginY)
	{
		//	UV space lines
		let Length = hypotenuse(1,1);
		Length = 0.68;
		let rho = Distance;
		let theta = DegreesToRadians(Angle);
		let Cos = Math.cos( theta );
		let Sin = Math.sin( theta );
	
		//	center of the line
		let CenterX = (Cos * rho) + OriginX;
		let CenterY = (Sin * rho) + OriginY;
		let OffsetX = Length * -Sin;
		let OffsetY = Length * Cos;
		
		let HoughLineStartX = CenterX + OffsetX;
		let HoughLineStartY = CenterY + OffsetY;
		let HoughLineEndX = CenterX - OffsetX;
		let HoughLineEndY = CenterY - OffsetY;
		
		let Lerp = function(Min,Max,Time)
		{
			return Min + ( (Max-Min) * Time );
		}
		
		let sx = Lerp( HoughLineStartX, HoughLineEndX, ChunkStartTime );
		let sy = Lerp( HoughLineStartY, HoughLineEndY, ChunkStartTime );
		let ex = Lerp( HoughLineStartX, HoughLineEndX, ChunkEndTime );
		let ey = Lerp( HoughLineStartY, HoughLineEndY, ChunkEndTime );
		
		return [sx,sy,ex,ey];
	}
	let IsValidLine = function(Line)
	{
		if ( Line[0] < 0 || Line[0] > 1 || Line[1] < 0 || Line[1] > 1 )
			return false;
		return true;
	}
	let PushHoughLineToLines = function(HoughLine)
	{
		let ChunkStartTime = (HoughLine.ChunkIndex+0) / ChunkCount;
		let ChunkEndTime = (HoughLine.ChunkIndex+1) / ChunkCount;
		
		if ( Frame.Params.ExtendChunks === true )
		{
			ChunkStartTime = 0;
			ChunkEndTime = 1;
		}
		else if ( Number.isInteger(Frame.Params.ExtendChunks) )
		{
			ChunkStartTime = (HoughLine.ChunkIndex-Frame.Params.ExtendChunks) / ChunkCount;
			ChunkEndTime = (HoughLine.ChunkIndex+1+Frame.Params.ExtendChunks) / ChunkCount;
		}
		
		let Line = HoughLineToLine( HoughLine.Angle, HoughLine.Distance, ChunkStartTime, ChunkEndTime, HoughLine.Origin[0], HoughLine.Origin[1] );
		//Debug(Line);
		if ( Frame.FilterOutsideLines === true )
			if ( !IsValidLine(Line) )
				return;
		Frame.Lines.push( Line );
		Frame.LineScores.push( HoughLine.Score );
		Frame.HoughLines.push( HoughLine );
	}
	
	HoughLines.sort(CompareScore);
	Debug("Got " + HoughLines.length + " hough lines. BiggestScore=" + BiggestScore);
	Debug("Top 10 scores: ");
	for ( let i=0;	i<10 && i<HoughLines.length;	i++ )
		Debug("#" + i + " " + HoughLines[i].Score );
	Frame.HoughLines = HoughLines;

	//	convert to real lines
	if ( !Array.isArray(Frame.Lines) )
		Frame.Lines = [];
	if ( !Array.isArray(Frame.LineScores) )
		Frame.LineScores = [];
	Frame.UnfilteredHoughLines = Frame.HoughLines;
	Frame.HoughLines = [];
	Frame.UnfilteredHoughLines.forEach( PushHoughLineToLines );
	Debug("Filtered to " + Frame.Lines.length + " valid lines.");
}

function GetHoughLines(OpenclContext,Frame)
{
	let HoughRunner = function(Resolve,Reject)
	{
		//let a = function()	{	return VisualiseAngleXDistanceXChunks(OpenclContext,Frame);	}
		let a = function()	{	return MakePromise( function(res){res();} );	}
		let b = function()	{	return CalcAngleXDistanceXChunks(OpenclContext,Frame);	}
		let c = function()	{	return AddTestAngleXDistanceXChunks(Frame);	}
		let d = function()	{	return GraphAngleXDistances(OpenclContext,Frame);	}
		let e = function()	{	return ExtractHoughLines(OpenclContext,Frame);	}
		let DoResolve = function(){	return MakePromise( Resolve );	}
		let OnError = function(err)
		{
			Debug("hough runner error: " + err);
			Reject();
		};
		
		if ( Frame.Params.LoadPremadeLineMask != undefined )
			Frame.LineMask = new Image(Frame.Params.LoadPremadeLineMask);

		a()
		.then( b )
		//.then( c )
		.then( d )
		.then( e )
		//.then( MakePromise(c) )
		.then( DoResolve )
		.catch( OnError );
	}
	
	//	high level promise
	return MakePromise( HoughRunner, false );
}

function DrawLines(OpenglContext,Frame)
{
	Debug("DrawLines");
	let Render = function(RenderTarget,RenderTargetTexture)
	{
		let Shader = GetDrawLinesShader(RenderTarget);
		
		let SetUniforms = function(Shader)
		{
			if ( !Array.isArray(Frame.Lines) )
				Frame.Lines = [];
			if ( Frame.Lines.length > Frame.Params.MaxLines )
				Frame.Lines.length = Frame.Params.MaxLines;
			
			if ( !Array.isArray(Frame.LineScores) )
				Frame.LineScores = [];
			if ( Frame.LineScores.length > Frame.Params.MaxLines )
				Frame.LineScores.length = Frame.Params.MaxLines;
			
			Shader.SetUniform("Lines", Frame.Lines );
			//Shader.SetUniform("LineScores", Frame.LineScores );
			//	gr: causing uniform error
			Shader.SetUniform("Background", Frame, 0 );
		}
		
		RenderTarget.DrawQuad( Shader, SetUniforms );
	}
	
	Frame.DebugLines = new Image( [Frame.GetWidth(),Frame.GetHeight() ] );
	let Prom = OpenglContext.Render( Frame.DebugLines, Render );
	return Prom;
}



function GetLineCorners(Frame)
{
	let GetCorners = function(Resolve)
	{
		let Lines = Frame.Lines;
		let GetLineIntersection = function(LineA,LineB,ScoreA,ScoreB)
		{
			let CornerScore = (ScoreA+ScoreB)/2;
			//return [0.5,0.5,1];
			
			//	https://stackoverflow.com/a/1968345
			
			// Returns 1 if the lines intersect, otherwise 0. In addition, if the lines
			// intersect the intersection point may be stored in the floats i_x and i_y.
			let p0_x = LineA[0];
			let p0_y = LineA[1];
			let p1_x = LineA[2];
			let p1_y = LineA[3];
			let p2_x = LineB[0];
			let p2_y = LineB[1];
			let p3_x = LineB[2];
			let p3_y = LineB[3];

			let s1_x = p1_x - p0_x;
			let s1_y = p1_y - p0_y;
			let s2_x = p3_x - p2_x;
			let s2_y = p3_y - p2_y;
		
			let s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
			let t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);
				
			if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
			{
				let ix = p0_x + (t * s1_x);
				let iy = p0_y + (t * s1_y);
				return [ix,iy,CornerScore];
			}
			return null;
		}
		
		let GetAngle180Diff = function(AngleA,AngleB)
		{
			let Diff = AngleB - AngleA;
			while ( Diff > 90 )
				Diff -= 180;
			while ( Diff < -90 )
				Diff += 180;
			return Diff;
		}
		
		let GetDistance = function(xy0,xy1)
		{
			var xdelta = xy0[0] - xy1[0];
			var ydelta = xy0[1] - xy1[1];
			return Math.sqrt( xdelta*xdelta + ydelta*ydelta );
		}
		
		let GetDuplicateCornerIndex = function(NewCorner)
		{
			if ( Frame.Params.MergeCornerMaxDistance === undefined )
				return false;
		
			for ( let i=0;	i<Frame.Corners.length;	i++ )
			{
				let OldCorner = Frame.Corners[i];
				let Distance = GetDistance( OldCorner, NewCorner );
				if ( Distance >= Frame.Params.MergeCornerMaxDistance )
					continue;
				return i;
			}
			return false;
		}
		
		let PushCorner = function(NewCorner)
		{
			//	reject/replace if better case
			let DuplicateCornerIndex = GetDuplicateCornerIndex(NewCorner);
			if ( DuplicateCornerIndex === false )
			{
				Frame.Corners.push( NewCorner );
				return;
			}
			
			//	replace the duplicate with the better score
			if ( Frame.Corners[DuplicateCornerIndex][3] < NewCorner[3] )
			{
				Frame.Corners[DuplicateCornerIndex] = NewCorner;
			}
		}
		
		for ( let la=0;	la<Lines.length;	la++ )
		{
			for ( let lb=la+1;	lb<Lines.length;	lb++ )
			{
				let AngleA = Frame.HoughLines[la].Angle;
				let AngleB = Frame.HoughLines[lb].Angle;
				let AngleDiff = GetAngle180Diff( AngleA, AngleB );
				if ( Math.abs(AngleDiff) < Frame.Params.CornerAngleDiffMin )
					continue;

				let ScoreA = Frame.LineScores[la];
				let ScoreB = Frame.LineScores[lb];
				let Intersection = GetLineIntersection( Lines[la], Lines[lb], ScoreA, ScoreB );
				if ( Intersection === null )
					continue;
				PushCorner( Intersection );
			}
		}

		if ( Frame.Params.WriteCornersToFilename != undefined )
		{
			let NiceCorners = [];
			let PushNiceCorner = function(Corner)
			{
				NiceCorners.push( { x:Corner[0], y:Corner[1] } );
			};
			Frame.Corners.forEach( PushNiceCorner );
			NiceCorners = { Corners:NiceCorners };
			let CornersJson = JSON.stringify( NiceCorners, null, '\t' );
			WriteStringToFile( Frame.Params.WriteCornersToFilename, CornersJson );
		}
		
		if ( Frame.Corners.length > Frame.Params.MaxCorners )
			Frame.Corners.length = Frame.Params.MaxCorners;
		
		Resolve();
	}
	
	Frame.Corners = [];
	let Prom = MakePromise( GetCorners );
	return Prom;
}



function FindCornerTransform(Frame)
{
	let DoFindCornerTransform = function(Resolve)
	{
		//	load ground truth corners
		let CornersJson = JSON.parse( LoadFileAsString(Frame.Params.GroundTruthCorners) );
		
		//	test: replace our corners with ground truth
		let GroundTruthCorners = [];
		let GetGroundTruthCorner = function(GroundTruthCorner)
		{
			let Corner = [ GroundTruthCorner.x, GroundTruthCorner.y, 99 ];
			GroundTruthCorners.push( Corner );
		};
		CornersJson.Corners.forEach( GetGroundTruthCorner );
		Frame.Corners = GroundTruthCorners;
		
		Resolve();
	}
	
	let Prom = MakePromise( DoFindCornerTransform );
	return Prom;
}


function DrawCorners(OpenglContext,Frame)
{
	let Render = function(RenderTarget,RenderTargetTexture)
	{
		let Shader = GetDrawCornersShader(RenderTarget);
		
		let SetUniforms = function(Shader)
		{
			Shader.SetUniform("CornerAndScores", Frame.Corners );
			Shader.SetUniform("Background", Frame.LineMask, 0 );
		}
		
		RenderTarget.DrawQuad( Shader, SetUniforms );
	}
	
	Frame.DebugCorners = new Image( [Frame.GetWidth(),Frame.GetHeight() ] );
	let Prom = OpenglContext.Render( Frame.DebugCorners, Render );
	return Prom;
}





function StartProcessFrame(Frame,OpenglContext,OpenclContext)
{
	Debug( "Frame size: " + Frame.GetWidth() + "x" + Frame.GetHeight() );
	//LastProcessedFrame = Frame;
	
	let TemplateParams = {};
	TemplateParams.HistogramHitMax = Math.sqrt( Frame.GetWidth() * Frame.GetHeight() ) / 10;
	TemplateParams.HoughOriginX = 0.5;
	TemplateParams.HoughOriginY = 0.5;
	TemplateParams.ExtractHoughLineMinScore = 0.3;
	TemplateParams.MaxLines = 100;
	TemplateParams.MaxCorners = 100;
	TemplateParams.ChunkCount = 20;
	TemplateParams.DistanceCount = 400;
	TemplateParams.AngleCount = 180;
	TemplateParams.CornerAngleDiffMin = 10;
	//TemplateParams.FilterOutsideLines = true;
	TemplateParams.LoadPremadeLineMask = "Data/PitchMaskHalf.png";
	TemplateParams.SkipIfBetterNeighbourRanges = { AngleRange:10, DistanceRange:10, ChunkRange:1 };
	TemplateParams.ExtendChunks = 1;
	TemplateParams.MergeCornerMaxDistance = 0.05;
	TemplateParams.WriteCornersToFilename = "Data/PitchGroundTruthCorners.json";

	let LiveParams = {};
	LiveParams.HistogramHitMax = Math.sqrt( Frame.GetWidth() * Frame.GetHeight() ) / 10;
	LiveParams.HoughOriginX = 0.5;
	LiveParams.HoughOriginY = 0.5;
	LiveParams.ExtractHoughLineMinScore = 0.3;
	LiveParams.MaxLines = 100;
	LiveParams.MaxCorners = 100;
	LiveParams.ChunkCount = 20;
	LiveParams.DistanceCount = 400;
	LiveParams.AngleCount = 180;
	LiveParams.CornerAngleDiffMin = 10;
	//LiveParams.FilterOutsideLines = true;
	//LiveParams.LoadPremadeLineMask = "Data/PitchMaskHalf.png";
	LiveParams.SkipIfBetterNeighbourRanges = { AngleRange:10, DistanceRange:10, ChunkRange:1 };
	LiveParams.ExtendChunks = true;
	LiveParams.GroundTruthCorners = "Data/PitchGroundTruthCorners.json";

	
	
	Frame.Params = TemplateParams;
	Frame.Params = LiveParams;
	/*
	Frame.HistogramHitMax = Math.sqrt( Frame.GetWidth() * Frame.GetHeight() ) / 10;
	Debug("Frame.HistogramHitMax="+ Frame.HistogramHitMax);
	Frame.HoughOriginX = 0.5;
	Frame.HoughOriginY = 0.5;
	Frame.ExtractHoughLineMinScore = 0.3;
	Frame.MaxLines = 100;
	Frame.ChunkCount = 20;
	Frame.DistanceCount = 400;
	Frame.AngleCount = 180;
	Frame.CornerAngleDiffMin = 10;
	//Frame.FilterOutsideLines = true;
	Frame.LoadPremadeLineMask = "Data/PitchMaskHalf.png";
	Frame.SkipIfBetterNeighbourRanges = { AngleRange:10, DistanceRange:10, ChunkRange:1 };
	//Frame.ExtendChunks = Frame.ChunkCount/4;
	*/
	
	let OnError = function(Error)
	{
		Debug(Error);
	};
	
	let Part1 = function()	{	return MakeHsl( OpenglContext, Frame );	}
	let Part2 = function()	{	return MakeGrassMask( OpenglContext, Frame );	}
	let Part3 = function()	{	return MakeLineMask( OpenglContext, Frame );	}
	let Part4 = function()	{	return ExtractTestLines( Frame );	}
	let Part5 = function()	{	return ExtractOpenclTestLines( OpenclContext, Frame );	}
	let Part6 = function()	{	return GetHoughLines( OpenclContext, Frame );	}
	let Part7 = function()	{	return DrawLines( OpenglContext, Frame );	}
	let Part8 = function()	{	return GetLineCorners( Frame );	}
	let Part9 = function()	{	return FindCornerTransform( Frame );	}
	let Part10 = function()	{	return DrawCorners( OpenglContext, Frame );	}
	let Finish = function()
	{
		LastProcessedFrame = Frame;
		Debug("Done frame!");
	};
	
	//	run sequence
	Part1()
	.then( Part2 )
	.then( Part3 )
	//.then( Part4 )
	//.then( Part5 )
	.then( Part6 )
	.then( Part7 )
	.then( Part8 )
	.then( Part9 )
	.then( Part10 )
	.then( Finish )
	.catch( OnError );
}


function WindowRender(RenderTarget)
{
	try
	{
		if ( LastProcessedFrame == null )
		{
			RenderTarget.ClearColour(0,1,1);
			return;
		}
		
		if ( !DebugFrameShader )
		{
			DebugFrameShader = new OpenglShader( RenderTarget, VertShaderSource, DebugFrameFragShaderSource );
		}
		
		let SetUniforms = function(Shader)
		{
			Shader.SetUniform("Image0", LastProcessedFrame, 0 );
			Shader.SetUniform("Image1", LastProcessedFrame.Hsl, 1 );
			Shader.SetUniform("Image2", LastProcessedFrame.GrassMask, 2 );
			Shader.SetUniform("Image3", LastProcessedFrame.LineMask, 3 );
			Shader.SetUniform("Image4", LastProcessedFrame.HoughHistogram, 4 );
			Shader.SetUniform("Image5", LastProcessedFrame.DebugLines, 5 );
			Shader.SetUniform("Image6", LastProcessedFrame.DebugCorners, 6 );
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

function Main()
{
	//Debug("log is working!", "2nd param");
	let Window1 = new OpenglWindow("Hello!");
	Window1.OnRender = function(){	WindowRender( Window1 );	};
	
	
	let OpenclDevices = OpenclEnumDevices();
	Debug("Opencl devices x" + OpenclDevices.length );
	if ( OpenclDevices.length == 0 )
		throw "No opencl devices";
	OpenclDevices.forEach( Debug );
	let Opencl = new OpenclContext( OpenclDevices[0] );

	let Filenames =
	[
		//"Data/PitchMask2.png",
		"Data/SwedenVsEngland.png",
		//"Data/ArgentinaVsCroatia.png"
	];
	
	let ProcessFrame = function(Filename)
	{
		let Pitch = new Image(Filename);
		let OpenglContext = Window1;
		StartProcessFrame( Pitch, OpenglContext, Opencl );
	};
	Filenames.forEach(ProcessFrame);
}

//	main
Main();
