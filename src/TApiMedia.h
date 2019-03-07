#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"

class TMediaPacket;
class TMediaExtractor;
class TMediaExtractorParams;
class TDecoderInstance;

namespace Broadway
{
	class TDecoder;
}

namespace ApiMedia
{
	void	Bind(Bind::TContext& Context);
}


extern const char MediaSource_TypeName[];
class TMediaSourceWrapper : public Bind::TObjectWrapper<MediaSource_TypeName,TMediaExtractor>
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
	
	static std::shared_ptr<TMediaExtractor>	AllocExtractor(const TMediaExtractorParams& Params);

public:
	std::shared_ptr<TMediaExtractor>&		mExtractor = mObject;
};


extern const char AvcDecoder_TypeName[];
class TAvcDecoderWrapper : public Bind::TObjectWrapper<AvcDecoder_TypeName,TDecoderInstance>
{
public:
	TAvcDecoderWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
	{
	}
	//~TAvcDecoderWrapper();
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	
	virtual void 				Construct(Bind::TCallback& Arguments) override;
	
	void						OnNewFrame(const SoyPixelsImpl& Pixels);	//	call onPictureDecoded
	static void 				Decode(Bind::TCallback& Arguments);
	
public:
	std::shared_ptr<TDecoderInstance>&		mDecoder = mObject;
};
