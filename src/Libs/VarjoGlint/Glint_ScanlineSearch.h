#pragma once

#include <stdint.h>

struct TRect
{
	uint32_t	mLeft = 0;
	uint32_t	mTop = 0;
	uint32_t	mHeight = 0;
	uint32_t	mWidth = 0;
};

struct TPixels
{
	uint8_t*	mPixels = nullptr;
	TRect		mSize;				//	only used for width/height
	uint32_t	mPixelStride = 0;
	uint32_t	mPixelLumaChannel = 0;
	bool		mInvert = false;
	uint32_t&	Width = mSize.mWidth;
	uint32_t&	Height = mSize.mHeight;
};


struct TParams
{
	//	floats are 0..1
	uint32_t	MinWidth = 20;
	uint32_t	MaxWidth = 200;
	uint32_t	MinHeight = 20;
	uint32_t	MaxHeight = 200;
	float		MinWhiteRatio = 0.20;
	float		MaxWhiteRatio = 0.90;
	uint32_t	LumaTolerance = 50;
	uint32_t	LineStride = 2;
	
	//	rescale luma for the dark
	uint32_t	LumaMin = 10;
	uint32_t	LumaMax = 200;
};


extern "C" void VarjoGlint_FindScanlines(const TPixels& Pixels,const TParams& Params,TRect* HorzRects,uint32_t* HorzRectCount,TRect* VertRects,uint32_t* VertRectCount);

