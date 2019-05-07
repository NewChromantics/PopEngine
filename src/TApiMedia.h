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

namespace PopCameraDevice
{
	class TInstance;
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



class TPopCameraDeviceWrapper : public Bind::TObjectWrapper<ApiMedia::Source_TypeName,PopCameraDevice::TInstance>
{
public:
	TPopCameraDeviceWrapper(Bind::TLocalContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
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



extern const char AvcDecoder_TypeName[];
class TAvcDecoderWrapper : public Bind::TObjectWrapper<AvcDecoder_TypeName,PopH264::TInstance>
{
public:
	TAvcDecoderWrapper(Bind::TLocalContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
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
