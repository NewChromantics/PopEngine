#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <map>

//#include "PopUnity.h"
//#include "PopMovieDecoder.h"
#include <SoyEvent.h>
#include <SoyThread.h>
#include <SoyMedia.h>


#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#endif

#include "AvfPixelBuffer.h"
//#include "AvfMovieDecoder.h"

#if defined(__OBJC__)
@class VideoCaptureProxy;
#endif


namespace TVideoQuality
{
	enum Type
	{
		Low,
		Medium,
		High,
	};
};


namespace Platform
{
	void								EnumCaptureDevices(std::function<void(const std::string&)> Append);
	std::shared_ptr<TMediaExtractor>	AllocCaptureExtractor(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext);
}


#if defined(__OBJC__)
class AvfVideoCapture : public TMediaExtractor
{
public:
	friend class AVCaptureSessionWrapper;
	
public:
	AvfVideoCapture(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext);
	virtual ~AvfVideoCapture();
	
	
	virtual void					GetStreams(ArrayBridge<TStreamMeta>&& Streams) override;
	virtual std::shared_ptr<Platform::TMediaFormat>	GetStreamFormat(size_t StreamIndex) override
	{
		return nullptr;
	}

	void		OnSampleBuffer(CMSampleBufferRef SampleBufferRef,size_t StreamIndex,bool DoRetain);
	void		OnSampleBuffer(CVPixelBufferRef PixelBufferRef,SoyTime Timestamp,size_t StreamIndex,bool DoRetain);

	
protected:
	virtual std::shared_ptr<TMediaPacket>	ReadNextPacket() override;
	
	TStreamMeta				GetFrameMeta(CMSampleBufferRef sampleBufferRef,size_t StreamIndex);

	//virtual void			GetStreamMeta(ArrayBridge<TStreamMeta>&& StreamMetas) override;
	//virtual TVideoMeta	GetMeta() override;

	void					StartStream();
	void					StopStream();

private:
	void		Shutdown();
	void		Run(const std::string& Serial,TVideoQuality::Type Quality,bool KeepOldFrames);
	void		QueuePacket(std::shared_ptr<TMediaPacket>& Packet);
	
public:
	std::map<size_t,TStreamMeta>		mStreamMeta;
	ObjcPtr<AVCaptureDevice>			mDevice;
	ObjcPtr<AVCaptureSession>			mSession;
	ObjcPtr<VideoCaptureProxy>			mProxy;
	ObjcPtr<AVCaptureVideoDataOutput>	mOutput;
	dispatch_queue_t					mQueue;
	bool								mDiscardOldFrames;
	bool								mForceNonPlanarOutput;
	
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::shared_ptr<AvfDecoderRenderer>	mRenderer;	//	persistent rendering data
	
	std::mutex								mPacketQueueLock;
	Array<std::shared_ptr<TMediaPacket>>	mPacketQueue;	//	extracted frames
};
#endif
