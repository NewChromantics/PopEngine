#pragma once

/*
	SoyVideoContainer wrapper for PopMovieTexture!
 
*/

#include <SoyVideoDevice.h>
#include <SoyOpenglContext.h>


class TVideoDecoder;
class TVideoDecoderParams;
class TMovieDecoderContainer;
class TMovieDecoder;



class TMovieDecoderContainer : public SoyVideoContainer
{
public:
	virtual void							GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas) override;
	virtual std::shared_ptr<TVideoDevice>	AllocDevice(const TVideoDeviceMeta& Meta,std::stringstream& Error) override;
	
	Array<std::shared_ptr<TMovieDecoder>>	mMovies;
};

class TMovieDecoder : public TVideoDevice, public SoyWorkerThread
{
public:
	TMovieDecoder(const TVideoDecoderParams& Params,const std::string& Serial,std::shared_ptr<Opengl::TContext> OpenglContext);
	
	virtual TVideoDeviceMeta	GetMeta() const override;
	
	virtual bool				Iteration() override;
	virtual bool				CanSleep() override;
	
	
public:
	std::string						mSerial;
	std::shared_ptr<TVideoDecoder>	mDecoder;
};

