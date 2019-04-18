#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"

class TMediaPacket;
class TMediaExtractor;
class TMediaExtractorParams;

namespace PopH264
{
	class TDecoderInstance;
}

namespace Broadway
{
	class TDecoder;
}

namespace PopCameraDevice
{
	class TDevice;
}

namespace ApiMedia
{
	void	Bind(Bind::TContext& Context);
	
	DECLARE_BIND_TYPENAME(Source);
	DECLARE_BIND_TYPENAME(PopCameraDevice);
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


class TMediaSourceWrapper : public Bind::TObjectWrapper<ApiMedia::Source_TypeName,TMediaExtractor>
{
public:
	TMediaSourceWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
	{
	}
	~TMediaSourceWrapper();
	
	static void								CreateTemplate(Bind::TTemplate& Template);
	
	virtual void 							Construct(Bind::TCallback& Params) override;

	void									OnNewFrame(size_t StreamIndex);
	static void								Free(Bind::TCallback& Params);
	static void								GetNextFrame(Bind::TCallback& Params);
	static void								PopFrame(Bind::TCallback& Params);

	Bind::TPromise							AllocFrameRequestPromise(Bind::TContext& Context,const TFrameRequestParams& Params);
	Bind::TObject							PopFrame(Bind::TContext& Context,const TFrameRequestParams& Params);

	static std::shared_ptr<TMediaExtractor>	AllocExtractor(const TMediaExtractorParams& Params);

public:
	std::shared_ptr<TMediaExtractor>&		mExtractor = mObject;
	
	//	gr: this should really store params per promise, but I want to use TPromiseQUeue
	TFrameRequestParams						mFrameRequestParams;
	Bind::TPromiseQueue						mFrameRequests;
};



class TPopCameraDeviceWrapper : public Bind::TObjectWrapper<ApiMedia::Source_TypeName,PopCameraDevice::TDevice>
{
public:
	TPopCameraDeviceWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
	{
	}

	static void								CreateTemplate(Bind::TTemplate& Template);

	virtual void 							Construct(Bind::TCallback& Params) override;

	void									OnNewFrame();
	static void								Free(Bind::TCallback& Params);
	static void								GetNextFrame(Bind::TCallback& Params);
	static void								PopFrame(Bind::TCallback& Params);

	Bind::TPromise							AllocFrameRequestPromise(Bind::TContext& Context,const TFrameRequestParams& Params);
	Bind::TObject							PopFrame(Bind::TContext& Context,const TFrameRequestParams& Params);


public:
	std::shared_ptr<PopCameraDevice::TDevice>&	mExtractor = mObject;

	//	gr: this should really store params per promise, but I want to use TPromiseQUeue
	TFrameRequestParams						mFrameRequestParams;
	Bind::TPromiseQueue						mFrameRequests;
};



extern const char AvcDecoder_TypeName[];
class TAvcDecoderWrapper : public Bind::TObjectWrapper<AvcDecoder_TypeName,PopH264::TDecoderInstance>
{
public:
	TAvcDecoderWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
	{
	}
	//~TAvcDecoderWrapper();
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Arguments) override;
	
	static void 				Decode(Bind::TCallback& Arguments);
	
public:
	std::shared_ptr<PopH264::TDecoderInstance>&		mDecoder = mObject;
	std::shared_ptr<SoyWorkerJobThread>				mDecoderThread;
	Array<Bind::TPersistent>			mFrameBuffers;
};
