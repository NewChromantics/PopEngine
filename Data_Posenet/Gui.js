
var GuiSliderShader_FragSource = LoadFileAsString("GuiSlider.frag");
var GuiSliderShader = null;


function GetRectNormalisedCoord(x,y,Rect)
{
	x = Range( Rect[0], Rect[0]+Rect[2], x );
	y = Range( Rect[1], Rect[1]+Rect[3], y );
	return [x,y];
}


function InsideRect(x,y,Rect)
{
	let xy = GetRectNormalisedCoord(x,y,Rect);
	if ( xy[0] < 0 )	return false;
	if ( xy[0] > 1 )	return false;
	if ( xy[1] < 0 )	return false;
	if ( xy[1] > 1 )	return false;
	return true;
}

function TGuiElement(Name,Getter,Setter,Min,Max)
{
	function Range(Min,Max,Value)
	{
		return (Value-Min) / (Max-Min);
	}
	
	function Lerp(Min,Max,Value)
	{
		return Min + ( Value * (Max-Min) );
	}

	
	this.Name = Name;
	this.Getter = Getter;
	this.Setter = Setter;
	this.Min = Min;
	this.Max = Max;
	
	this.GetNormalised = function()
	{
		let Value = this.Getter();
		let ValueNorm = Range( this.Min, this.Max, Value );
		return ValueNorm;
	}
	
	this.SetNormalised = function(ValueNorm)
	{
		let Value = Lerp( this.Min, this.Max, ValueNorm );
		this.Setter( Value );
	}
	
	this.OnHover = function(x,y)
	{
		
	}
	
	this.OnClick = function(x,y)
	{
		x = Math.min( 1, Math.max( 0, x ) );
		this.SetNormalised(x);
	}
	
	this.Render = function(RenderTarget,Rect)
	{
		if ( !GuiSliderShader )
		{
			GuiSliderShader = new OpenglShader( RenderTarget, VertShaderSource, GuiSliderShader_FragSource );
		}
		
		let This = this;
		let SetUniforms = function(Shader)
		{
			let ValueNorm = This.GetNormalised();
			Shader.SetUniform("Value", ValueNorm );
			Shader.SetUniform("VertexRect", Rect );
		}
		RenderTarget.DrawQuad( GuiSliderShader, SetUniforms );
	}
}


function TGui(GuiRect)
{
	this.Elements = [];
	this.LockedElementIndex = null;
	this.GuiRect = GuiRect;
	
	this.Add = function(Element)
	{
		this.Elements.push(Element);
	}
	
	this.OnMouseDown = function(x,y)
	{
		this.MouseDown = true;
		this.LockedElementIndex = this.GetElementIndexAt( x,y );
		this.OnClick( x, y );
	}
	
	this.OnMouseMove = function(x,y)
	{
		this.OnClick( x,y );
	}
	
	this.OnMouseUp = function(x,y)
	{
		this.LockedElementIndex = null;
	}
	
	this.OnClick = function(x,y)
	{
		if ( this.LockedElementIndex === null )
		{
			this.OnHover(x,y);
			return;
		}
		
		//	get local xy
		let Element = this.Elements[this.LockedElementIndex];
		let ElementRect = this.GetElementRect(this.LockedElementIndex);
		let RectXy = GetRectNormalisedCoord(x,y,ElementRect);
		Element.OnClick( RectXy[0], RectXy[1] );
	}
	
	this.OnHover = function(x,y)
	{
		let ElementIndex = this.GetElementIndexAt(x,y);
		if ( ElementIndex === null )
			return;
		
		//	get local xy
		let Element = this.Elements[this.LockedElementIndex];
		let ElementRect = this.GetElementRect(ElementIndex);
		let RectXy = GetRectNormalisedCoord(x,y,ElementRect);
		Element.OnHover( RectXy[0], RectXy[1] );
	}
	
	this.GetElementRect = function(ElementIndex)
	{
		let ElementBoxSpacing = 5/500;
		let MaxElementBoxWidth = 100/500;
		let MaxElementBoxHeight = 20/500;
		let ElementBoxHeight = Math.min( MaxElementBoxHeight, this.GuiRect[3] / this.Elements.length );
		
		let x = ElementBoxSpacing + this.GuiRect[0];
		let y = ElementBoxSpacing + this.GuiRect[1] + ( ElementBoxHeight * ElementIndex );
		let w = MaxElementBoxWidth - ElementBoxSpacing;
		let h = ElementBoxHeight - ElementBoxSpacing;
		
		return [x,y,w,h];
	}
	
	this.GetElementIndexAt = function(x,y)
	{
		for ( let e=0;	e<this.Elements.length;	e++ )
		{
			let ElementRect = this.GetElementRect(e);
			if ( InsideRect(x,y,ElementRect) )
				return e;
		}
		return null;
	}
	
	this.Render = function(RenderTarget)
	{
		for ( let e=0;	e<this.Elements.length;	e++ )
		{
			let Element = this.Elements[e];
			let ElementRect = this.GetElementRect(e);
			Element.Render(RenderTarget,ElementRect);
		}
	}
	
}
