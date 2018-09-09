

var GuiSliderShader = null;
var GuiSliderShader_File = new File("GuiSlider.frag");
GuiSliderShader_File.OnChanged = function(File)
{
	GuiSliderShader = null;
}



var SdfTexture = "SdfFont_SansSerif.png";
var SdfShader_FragSource = "SdfFont.frag";
//	from https://mapbox.github.io/tiny-sdf/
//	open console and change chars= to this line (gr: broken up for font layout)
var SdfChars = [
				'ABCDEFGHIJKLMNOPQRST',
				'UVWXYZabcdefghijklmn',
				'opqrstuvwxyz01234567',
				'89 !%.:,()/\"*^&-=_+\''
				];


function TGuiFont(SdfFontFilename,FontMap,FragSource)
{
	this.SdfTexture = new Image(SdfFontFilename);
	this.SdfTexture.SetLinearFilter(true);
	
	this.FragSource = LoadFileAsString(FragSource);
	this.Shader = null;
	this.FontMap = FontMap;
	
	//	sdf render settings
	this.InnerDistance = 0.9;
	this.OuterDistance = 0.7;
	this.NullDistance = 0.4;
	

	
	this.GetFontMapCharacterRect = function(Char)
	{
		let RowAndIndex = false;
		for ( let row=0;	row<this.FontMap.length;	row++)
		{
			let Index = this.FontMap[row].indexOf(Char);
			if ( Index == -1 )
				continue;
			RowAndIndex = [row,Index];
		}
		if ( RowAndIndex === false )
			RowAndIndex = [3,3];	//	!
		let h = this.FontMap.length*2;	//	<----- wrong!
		let w = this.FontMap[0].length;
		let y = RowAndIndex[0] / h;
		let x = RowAndIndex[1] / w;
		return [x,y,1/w,1/h];
	}
	
	
	this.Render = function(RenderTarget,String,RenderRect)
	{
		if ( !this.Shader )
		{
			this.Shader = new OpenglShader( RenderTarget, VertShaderSource, this.FragSource );
		}
		
		let FontWidthRatio = 0.6;
		let FontMargin = 0.1;
		let FontKerning = 0.4;
		
		//	get font size
		let FontHeight = Math.min( RenderRect[3], 10/100 );
		FontHeight *= 1;
		let FontWidth = FontHeight * FontWidthRatio;
		let Kerning = FontWidth * FontKerning;
		RenderRect[2] = FontWidth;
		RenderRect[3] = FontHeight;
		
		//	pad from rect
		let Margin = FontHeight * FontMargin;
		RenderRect[1] += Margin;
		RenderRect[3] -= Margin*2;
		
		let FontTexture = this.SdfTexture;
		for ( let c=0;	c<String.length;	c++ )
		{
			let Char = String[c];
			
			let FontRect = this.GetFontMapCharacterRect(Char);
			let This = this;
			let SetUniforms = function(Shader)
			{
				if ( c == 0 )
				{
					Shader.SetUniform("SdfTexture", FontTexture, 0 );
					if ( This.InnerDistance )
					{
						Shader.SetUniform("InnerDistance", This.InnerDistance );
						Shader.SetUniform("OuterDistance", This.OuterDistance );
						Shader.SetUniform("NullDistance", This.NullDistance );
					}
				}
				Shader.SetUniform("VertexRect", RenderRect );
				Shader.SetUniform("SdfRect", FontRect );
			}
			if ( c == 0 )
				RenderTarget.EnableBlend(true);
			//	add option to draw N quads
			RenderTarget.DrawQuad( this.Shader, SetUniforms );
			RenderRect[0] += RenderRect[2] - Kerning;
		}
	}
}

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
	
	this.OnClick = function(x,y,FirstClick)
	{
		x = Math.min( 1, Math.max( 0, x ) );
		this.SetNormalised(x,FirstClick);
	}
	
	this.GetLabel = function()
	{
		let Value = this.Getter();
		let Label = this.Name + ": ";
		if (typeof Value == 'number')
			Label += Value.toFixed(2);
		else
			Label += Value;
		return Label;
	}
	
	this.Render = function(RenderTarget,Rect,Font)
	{
		if ( !GuiSliderShader )
		{
			let Source = GuiSliderShader_File.GetString();
			GuiSliderShader = new OpenglShader( RenderTarget, VertShaderSource, Source );
		}
		
		let ValueNorm = this.GetNormalised();
		let SetUniforms = function(Shader)
		{
			Shader.SetUniform("Value", ValueNorm );
			Shader.SetUniform("VertexRect", Rect );
		}
		RenderTarget.EnableBlend(true);
		RenderTarget.DrawQuad( GuiSliderShader, SetUniforms );
		
		let Label = this.GetLabel();
		Font.Render( RenderTarget, Label, Rect );
	}
}

function TGuiSlider()
{
	TGuiElement.apply(this,arguments);
}

function TGuiSliderInt()
{
	TGuiElement.apply(this,arguments);
	
	this.GetLabel = function()
	{
		let Value = this.Getter();
		let Label = this.Name + ": " + Value.toFixed(0);
		return Label;
	}
}

function TGuiToggle()
{
	TGuiElement.apply(this,arguments);
	
	this.GetNormalised = function()
	{
		return this.Getter() ? 1 : 0;
	}
	
	this.OnClick = function(x,y,FirstClick)
	{
		let Value = this.Getter();
		Value = !Value;
		if ( FirstClick )
			this.Setter(Value);
	}

	this.GetLabel = function()
	{
		let Value = this.Getter();
		let Label = Value ? "(X)" : "( )";
		Label += " " + this.Name;
		return Label;
	}
}



function TGuiButton(Name,OnClick)
{
	TGuiElement.apply(this,arguments);
	this.Getter = function(){	return 0;	};
	this.Setter = function(v){};
	this.OnClickCallback = OnClick;
	
	this.OnClick = function(x,y,FirstClick)
	{
		if ( FirstClick )
			this.OnClickCallback();
	}
	
	this.GetLabel = function()
	{
		let Label = "<" + this.Name + ">";
		return Label;
	}
}


function TGui(GuiRect)
{
	this.LockedLayoutElement = null;
	this.GuiRect = GuiRect;
	this.Font = new TGuiFont(SdfTexture,SdfChars,SdfShader_FragSource);
	
	//	to make a good immediate mode, we cache the layout of what we renderered and refer back to it
	this.LastLayout = [];
	
	this.OnMouseDown = function(x,y)
	{
		this.MouseDown = true;
		this.LockedLayoutElement = this.GetLayoutElement( x,y );
		this.OnClick( x, y, true );
	}
	
	this.OnMouseMove = function(x,y)
	{
		this.OnClick( x,y, false );
	}
	
	this.OnMouseUp = function(x,y)
	{
		this.LockedLayoutElement = null;
	}
	
	this.OnClick = function(x,y,FirstClick)
	{
		if ( this.LockedLayoutElement === null )
		{
			this.OnHover(x,y);
			return;
		}
		
		//	get local xy
		let Element = this.LockedLayoutElement;
		let RectXy = GetRectNormalisedCoord(x,y,Element.Rect);
		Element.Element.OnClick( RectXy[0], RectXy[1], FirstClick );
	}
	
	this.OnHover = function(x,y)
	{
		let Element = this.GetLayoutElement(x,y);
		if ( Element === null )
			return;
		
		//	get local xy
		let RectXy = GetRectNormalisedCoord(x,y,Element.Rect);
		Element.Element.OnHover( RectXy[0], RectXy[1] );
	}
	
	this.OnEnumElements = function(Enum)
	{
		Debug("Replace this func to draw your elements.")
	}
	
	this.Render = function(RenderTarget)
	{
		this.ClearLayout();
		
		//	more "immediate mode"
		let MaxElementBoxWidth = 100/500;
		let MaxElementBoxHeight = 20/500;
		let ElementBoxSpacing = 5/500;
		let ElementRect = [ this.GuiRect[0], this.GuiRect[1], MaxElementBoxWidth, MaxElementBoxHeight ];
		ElementRect[0] += ElementBoxSpacing;
		ElementRect[1] += ElementBoxSpacing;
		
		let This = this;
		let RenderNextElement = function(Element)
		{
			//	make copy of the array as stuff in scope will probably modify it
			let er = ElementRect.slice(0);
			This.CacheLayout( er, Element );
			Element.Render(RenderTarget,er,This.Font);
			
			ElementRect[1] += ElementRect[3] + ElementBoxSpacing;
		}
		
		this.OnEnumElements( RenderNextElement );
	}
	
	this.ClearLayout = function()
	{
		this.LastLayout = [];
	}
	
	this.CacheLayout = function(ElementRect,TheElement)
	{
		let LayoutElement = { Rect:ElementRect.slice(0), Element:TheElement };
		this.LastLayout.push(LayoutElement);
	}
	
	this.GetLayoutElement = function(x,y)
	{
		let FoundElement = null;
		let FindElement = function(LayoutElement)
		{
			let ElementRect = LayoutElement.Rect;
			if ( InsideRect(x,y,ElementRect) )
				FoundElement = LayoutElement;
		}
		this.LastLayout.forEach( FindElement );
		return FoundElement;
	}
	
}
