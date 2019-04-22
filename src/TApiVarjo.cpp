#include "TApiVarjo.h"
#include "TApiCommon.h"

#include "Libs/VarjoGlint/Glint_ScanlineSearch.h"

namespace ApiVarjo
{
	const char Namespace[] = "Varjo";

	DEFINE_BIND_FUNCTIONNAME(Scanline_SearchForWindows);
	
	void	Scanline_SearchForWindows(Bind::TCallback& Params);
}




void ApiVarjo::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindGlobalFunction<Scanline_SearchForWindows_FunctionName>( Scanline_SearchForWindows, Namespace );
}


void ApiVarjo::Scanline_SearchForWindows(Bind::TCallback& Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	
	//	grab pixels
	SoyPixels Luma;
	Image.GetPixels( Luma );
	Luma.SetFormat( SoyPixelsFormat::Greyscale );
	
	
	const int RectBufferCount = 100;
	TRect Rects[RectBufferCount];
	auto* Pixels = &Luma.GetPixelPtr(0,0,0);
	auto RectCount = Scanline_SearchForWindows( Luma.GetWidth(), Luma.GetHeight(), Luma.GetMeta().GetChannels(), Pixels, Rects, RectBufferCount );
	
	Params.Return( RectCount );
}
