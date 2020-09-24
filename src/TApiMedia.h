#pragma once
#include "TBind.h"
#include <SoyPixels.h>

class TMediaPacket;
class TMediaExtractor;
class TMediaExtractorParams;


namespace PopH264Decoder
{
	class TInstance;
}

namespace PopH264Encoder
{
	class TInstance;
}

namespace ApiMediaPopCameraDevice
{
	class TInstance;
	class TFrame;
}

namespace ApiMedia
{
	void	Bind(Bind::TContext& Context);
	
	class TCameraDeviceWrapper;
	DECLARE_BIND_TYPENAME(Source);
	
	class TH264DecoderWrapper;
	DECLARE_BIND_TYPENAME(H264Decoder);

	class TH264EncoderWrapper;
	DECLARE_BIND_TYPENAME(H264Encoder);
}



class ApiMediaPopCameraDevice::TFrame
{
public:
	SoyTime			mTime;			//	time of packet being popped
	uint32_t		mFrameNumber = 0;	//	time from device
	std::string		mMeta;	//	other meta
	//BufferArray<std::shared_ptr<SoyPixelsImpl>,4>	mPlanes;
	std::shared_ptr<SoyPixelsImpl>	mPlane0;
	std::shared_ptr<SoyPixelsImpl>	mPlane1;
	std::shared_ptr<SoyPixelsImpl>	mPlane2;
	std::shared_ptr<SoyPixelsImpl>	mPlane3;
};


class ApiMedia::TCameraDeviceWrapper : public Bind::TObjectWrapper<BindType::Source, ApiMediaPopCameraDevice::TInstance>
{
public:
	TCameraDeviceWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context )
	{
	}

	static void				CreateTemplate(Bind::TTemplate& Template);

	virtual void 			Construct(Bind::TCallback& Params) override;

	void					Free(Bind::TCallback& Params);
	void					WaitForNextFrame(Bind::TCallback& Params);

protected:
	void					FlushPendingFrames();
	void					OnNewFrame();

public:
	std::shared_ptr<ApiMediaPopCameraDevice::TInstance>&	mInstance = mObject;
	bool									mOnlyLatestFrame = true;
	Bind::TPromiseQueue				mFrameRequests;
	std::mutex						mFramesLock;
	Array<ApiMediaPopCameraDevice::TFrame>	mFrames;
};



class ApiMedia::TH264DecoderWrapper : public Bind::TObjectWrapper<BindType::H264Decoder,PopH264Decoder::TInstance>
{
public:
	TH264DecoderWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context )
	{
	}
	//~TAvcDecoderWrapper();
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Arguments) override;
	
	void 						Decode(Bind::TCallback& Arguments);
	void 						GetDecodeJobCount(Bind::TCallback& Arguments);
	void						WaitForNextFrame(Bind::TCallback& Params);

protected:
	void						FlushPendingFrames();
	void						OnNewFrame();
	void						OnError(const std::string& Error);

	//	access queue of data for decoder
	std::shared_ptr<Array<uint8_t>>	PopQueuedData();
	void 							PushQueuedData(const ArrayBridge<uint8_t>&& Data);
	void						FlushQueuedData();
	
public:
	std::shared_ptr<PopH264Decoder::TInstance>&		mDecoder = mObject;
	
	bool							mOnlyLatest = true;
	bool							mSplitPlanes = true;
	Bind::TPromiseQueue				mFrameRequests;
	std::mutex						mFramesLock;
	Array<ApiMediaPopCameraDevice::TFrame>	mFrames;
	Array<std::string>				mErrors;
	
	//	lots of tiny jobs are expensive, (eg. udp fragmented packets)
	//	so buffer as much data as possible and let the job queue pump
	//	as much as it can in each job. Decoder should cope
	std::mutex						mPushDataLock;
	//	rather than resize one array and re-copy on pop, push & pop an allocated array
	//	this would work well as a pool/double buffer
	std::shared_ptr<Array<uint8_t>>	mPushData;
	std::shared_ptr<SoyWorkerThread>		mDecoderThread;
};



class ApiMedia::TH264EncoderWrapper : public Bind::TObjectWrapper<BindType::H264Encoder,PopH264Encoder::TInstance>
{
public:
	TH264EncoderWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void 			Encode(Bind::TCallback& Params);
	void 			EncodeFinished(Bind::TCallback& Params);
	void 			WaitForNextPacket(Bind::TCallback& Params);

	void			OnPacketOutput();

public:
	Bind::TPromiseQueue	mNextPacketPromises;
	std::shared_ptr<PopH264Encoder::TInstance>&	mEncoder = mObject;
	std::shared_ptr<SoyWorkerJobThread>	mEncoderThread;
};


