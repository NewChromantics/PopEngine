#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"

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
	void					PopFrame(Bind::TCallback& Params);
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
	
	static void 				Decode(Bind::TCallback& Arguments);
	
public:
	std::shared_ptr<PopH264::TInstance>&		mDecoder = mObject;
	std::shared_ptr<SoyWorkerJobThread>			mDecoderThread;
	Array<Bind::TPersistent>			mFrameBuffers;
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
	void 			GetNextPacket(Bind::TCallback& Params);

	void			OnPacketOutput();

public:
	Bind::TPromiseQueue	mNextPacketPromises;
	std::shared_ptr<X264::TInstance>&	mEncoder = mObject;
	std::shared_ptr<SoyWorkerJobThread>	mEncoderThread;
};
