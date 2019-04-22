#pragma once

#include <stdint.h>

struct TRect
{
	uint32_t	mLeft = 0;
	uint32_t	mTop = 0;
	uint32_t	mHeight = 0;
	uint32_t	mWidth = 0;
};

//	returns number of rects found. (which may be more than RectCount!)
extern "C" uint32_t Scanline_SearchForWindows(uint32_t Width,uint32_t Height,uint32_t Channels,uint8_t* Pixels,TRect* Rects,uint32_t RectCount);
