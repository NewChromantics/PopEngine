#pragma once

/*
	SoyVideoContainer wrapper for PopMovieTexture!
 
*/

//	include here so other files can access params etc
#include <Build/PopMovieTextureOsxFramework.framework/Headers/PopMovieTextureOsxFramework.h>
#include <SoyMedia.h>



class PopVideoDecoderBase
{
public:
	PopVideoDecoderBase(const TVideoDecoderParams& Params);
	~PopVideoDecoderBase();
	
public:
	SoyEvent<const std::string>					mOnError;
	SoyEvent<std::pair<SoyPixelsImpl*,SoyTime>>	mOnFrame;
	
protected:
	std::shared_ptr<PopMovie::TInstance>	mInstance;
};




class PopVideoDecoder : public PopVideoDecoderBase, public SoyWorkerThread
{
public:
	PopVideoDecoder(const TVideoDecoderParams& Params);
	~PopVideoDecoder();
	
protected:
	virtual bool		Iteration() override;
	
protected:
	size_t				mStreamIndex;
	SoyTime				mInitialTime;	//	use SoyTime() to decode "in real time"
};



