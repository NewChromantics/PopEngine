#pragma once
#include "TBind.h"
#include <SoyPixels.h>

class TMediaPacket;
class TMediaExtractor;
class TMediaExtractorParams;


namespace PopH264
{
	class TInstance;
}

namespace X264
{
	class TInstance;
}

namespace PopCameraDevice
{
	class TInstance;
	class TFrame;
}

namespace ApiMedia
{
	void	Bind(Bind::TContext& Context);
	
	DECLARE_BIND_TYPENAME(Source);
	DECLARE_BIND_TYPENAME(PopCameraDevice);
	DECLARE_BIND_TYPENAME(AvcDecoder);
	DECLARE_BIND_TYPENAME(H264Encoder);
}



class PopCameraDevice::TFrame
{
public:
	SoyTime			mTime;
	std::string		mMeta;	//	other meta
	//BufferArray<std::shared_ptr<SoyPixelsImpl>,4>	mPlanes;
	std::shared_ptr<SoyPixelsImpl>	mPlane0;
	std::shared_ptr<SoyPixelsImpl>	mPlane1;
	std::shared_ptr<SoyPixelsImpl>	mPlane2;
	std::shared_ptr<SoyPixelsImpl>	mPlane3;
};


class TPopCameraDeviceWrapper : public Bind::TObjectWrapper<ApiMedia::BindType::Source,PopCameraDevice::TInstance>
{
public:
	TPopCameraDeviceWrapper(Bind::TContext& Context) :
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
	std::shared_ptr<PopCameraDevice::TInstance>&	mInstance = mObject;
	bool									mOnlyLatestFrame = true;
	Bind::TPromiseQueue				mFrameRequests;
	std::mutex						mFramesLock;
	Array<PopCameraDevice::TFrame>	mFrames;
};



class TAvcDecoderWrapper : public Bind::TObjectWrapper<ApiMedia::BindType::AvcDecoder,PopH264::TInstance>
{
public:
	TAvcDecoderWrapper(Bind::TContext& Context) :
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
	std::shared_ptr<PopH264::TInstance>&		mDecoder = mObject;
	
	bool							mOnlyLatest = true;
	bool							mSplitPlanes = true;
	Bind::TPromiseQueue				mFrameRequests;
	std::mutex						mFramesLock;
	Array<PopCameraDevice::TFrame>	mFrames;
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



class TH264EncoderWrapper : public Bind::TObjectWrapper<ApiMedia::BindType::H264Encoder,X264::TInstance>
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
	std::shared_ptr<X264::TInstance>&	mEncoder = mObject;
	std::shared_ptr<SoyWorkerJobThread>	mEncoderThread;
};
