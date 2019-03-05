#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"
#include "TV8ObjectWrapper.h"

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
	void	Bind(TV8Container& Container);
}


extern const char MediaSource_TypeName[];
class TMediaSourceWrapper : public TObjectWrapper<MediaSource_TypeName,TMediaExtractor>
{
public:
	TMediaSourceWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	~TMediaSourceWrapper();
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	virtual void 							Construct(Bind::TCallback& Arguments) override;

	void									OnNewFrame(size_t StreamIndex);
	static void								Free(Bind::TCallback& Arguments);
	static void								GetNextFrame(Bind::TCallback& Arguments);
	
	static std::shared_ptr<TMediaExtractor>	AllocExtractor(const TMediaExtractorParams& Params);

public:
	std::shared_ptr<V8Storage<v8::Function>>	mOnFrameFilter;
	
	std::shared_ptr<TMediaExtractor>&		mExtractor = mObject;
};


extern const char AvcDecoder_TypeName[];
class TAvcDecoderWrapper : public TObjectWrapper<AvcDecoder_TypeName,TDecoderInstance>
{
public:
	TAvcDecoderWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	//~TAvcDecoderWrapper();
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	virtual void 							Construct(Bind::TCallback& Arguments) override;
	
	void									OnNewFrame(const SoyPixelsImpl& Pixels);	//	call onPictureDecoded
	static v8::Local<v8::Value>				Decode(v8::TCallback& Arguments);
	
public:
	std::shared_ptr<TDecoderInstance>&		mDecoder = mObject;
};
