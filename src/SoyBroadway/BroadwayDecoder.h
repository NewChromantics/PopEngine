#pragma once

extern "C"
{
#include "H264SwDecApi.h"
}
//	todo: implement a TMediaExtractor interface,
//	but for now I just want to expose the C api to match the WASM version (which currently doesnt run in v8, but does on the web)

//	from broadway
//extern "C" void broadwayOnHeadersDecoded();
//extern "C" void broadwayOnPictureDecoded(u8 *buffer, u32 width, u32 height);

namespace Broadway
{
	class TDecoder;
}


class Broadway::TDecoder
{
public:
	TDecoder();
	~TDecoder();
	
	H264SwDecInst	mDecoderInstance;
};
