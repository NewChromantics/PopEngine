#include "TApiVarjo.h"
#include "TApiCommon.h"

#include "Libs/VarjoGlint/Glint_ScanlineSearch.h"

namespace ApiVarjo
{
	const char Namespace[] = "Varjo";

	DEFINE_BIND_FUNCTIONNAME(FindScanlines);
	
	void	FindScanlines(Bind::TCallback& Params);
}




void ApiVarjo::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindGlobalFunction<FindScanlines_FunctionName>( FindScanlines, Namespace );
}


void ApiVarjo::FindScanlines(Bind::TCallback& Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	
	//	get params
	auto ScanParamsObject = Params.GetArgumentObject(1);
	TParams ScanParams;
	ScanParams.MinWidth = ScanParamsObject.GetInt("MinWidth");
	ScanParams.MaxWidth = ScanParamsObject.GetInt("MaxWidth");
	ScanParams.MinHeight = ScanParamsObject.GetInt("MinHeight");
	ScanParams.MaxHeight = ScanParamsObject.GetInt("MaxHeight");
	ScanParams.MinWhiteRatio = ScanParamsObject.GetFloat("MinWhiteRatio");
	ScanParams.MaxWhiteRatio = ScanParamsObject.GetFloat("MaxWhiteRatio");
	ScanParams.LumaTolerance = ScanParamsObject.GetInt("LumaTolerance");
	ScanParams.LineStride = ScanParamsObject.GetInt("LineStride");
	auto Invert = ScanParamsObject.GetInt("Invert");
	ScanParams.LumaMin = ScanParamsObject.GetInt("LumaMin");
	ScanParams.LumaMax = ScanParamsObject.GetInt("LumaMax");

	auto& Pixels = Image.GetPixels();
	
	const int HorzRectBufferCount = 100;
	TRect HorzRects[HorzRectBufferCount];
	const int VertRectBufferCount = 100;
	TRect VertRects[VertRectBufferCount];
	
	TPixels PixelsMeta;
	PixelsMeta.mPixels = &Pixels.GetPixelPtr(0,0,0);
	PixelsMeta.mPixelStride = Pixels.GetMeta().GetChannels();
	PixelsMeta.mSize.mLeft = 0;
	PixelsMeta.mSize.mTop = 0;
	PixelsMeta.mSize.mWidth = Pixels.GetWidth();
	PixelsMeta.mSize.mHeight = Pixels.GetHeight();
	PixelsMeta.mInvert = Invert;
	
	switch ( Pixels.GetFormat() )
	{
		case SoyPixelsFormat::Uvy_844_Full:
			PixelsMeta.mPixelLumaChannel = 1;
			break;
		default:
			PixelsMeta.mPixelLumaChannel = 0;
			break;
	}
	
	uint32_t HorzRectCount = HorzRectBufferCount;
	uint32_t VertRectCount = VertRectBufferCount;
	{
		Soy::TScopeTimerPrint Timer("VarjoGlint_FindScanlines",1);
		VarjoGlint_FindScanlines( PixelsMeta, ScanParams, HorzRects, &HorzRectCount, VertRects, &VertRectCount );
	}
	auto* HorzRectsInts = &HorzRects[0].mLeft;
	auto HorzRectsArray = GetRemoteArray( HorzRectsInts, HorzRectCount*4 );

	auto* VertRectsInts = &VertRects[0].mLeft;
	auto VertRectsArray = GetRemoteArray( VertRectsInts, VertRectCount*4 );

	auto Result = Params.mContext.CreateObjectInstance();
	Result.SetArray("HorzRects", GetArrayBridge(HorzRectsArray) );
	Result.SetArray("VertRects", GetArrayBridge(VertRectsArray) );
	Params.Return( Result );
}
