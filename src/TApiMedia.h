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
}

namespace ApiMedia
{
	void	Bind(Bind::TContext& Context);
	
	DECLARE_BIND_TYPENAME(Source);
	DECLARE_BIND_TYPENAME(PopCameraDevice);
	DECLARE_BIND_TYPENAME(AvcDecoder);
	DECLARE_BIND_TYPENAME(H264Encoder);
}


class TFrameRequestParams
{
public:
	bool				mSeperatePlanes = false;
	size_t				mStreamIndex = 0;
	bool				mLatestFrame = true;
	Bind::TPersistent	mDestinationImage;
};

class TFrameRequest : public TFrameRequestParams
{
public:
	TFrameRequest()	{}
	TFrameRequest(const TFrameRequestParams& Copy) :	TFrameRequestParams (Copy)	{}
	
public:
	Bind::TPromise	mPromise;
};



class TPopCameraDeviceWrapper : public Bind::TObjectWrapper<ApiMedia::BindType::Source,PopCameraDevice::TInstance>
{
public:
	TPopCameraDeviceWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context )
	{
	}

	static void								CreateTemplate(Bind::TTemplate& Template);

	virtual void 							Construct(Bind::TCallback& Params) override;

	void									OnNewFrame();
	static void								Free(Bind::TCallback& Params);
	static void								GetNextFrame(Bind::TCallback& Params);
	static void								PopFrame(Bind::TCallback& Params);

	Bind::TPromise							AllocFrameRequestPromise(Bind::TLocalContext& Context,const TFrameRequestParams& Params);
	Bind::TObject							PopFrame(Bind::TLocalContext& Context,TFrameRequestParams& Params);


public:
	std::shared_ptr<PopCameraDevice::TInstance>&	mExtractor = mObject;

	//	gr: this should really store params per promise, but I want to use TPromiseQUeue
	TFrameRequestParams						mFrameRequestParams;
	Bind::TPromiseQueue						mFrameRequests;
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
