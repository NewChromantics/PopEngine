#pragma once

#include <functional>
#include "Array.hpp"
#include "HeapArray.hpp"

class SoyPixelsImpl;

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
	TDecoder(std::function<void(const SoyPixelsImpl&)> OnFrameDecoded);
	~TDecoder();
	
	void			Decode(ArrayBridge<uint8_t>&& PacketData);

private:
	void			OnMeta(const H264SwDecInfo& Meta);
	void			OnPicture(const H264SwDecPicture& Picture,const H264SwDecInfo& Meta);
	bool			DecodeNextPacket();	//	returns true if more data to proccess
	
public:
	H264SwDecInst	mDecoderInstance;
	Array<uint8_t>	mPendingData;

	std::function<void(const SoyPixelsImpl&)>	mOnFrameDecoded;
};
