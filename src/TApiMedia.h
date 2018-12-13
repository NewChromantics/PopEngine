#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"
#include "TV8ObjectWrapper.h"

class TMediaPacket;
class TMediaExtractor;
class TMediaExtractorParams;

namespace Broadway
{
	class TDecoder;
}

namespace ApiMedia
{
	void	Bind(TV8Container& Container);
}

class TMediaWrapper
{
public:
	TMediaWrapper() :
		mContainer		( nullptr )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	
	static v8::Local<v8::Value>				EnumDevices(const v8::CallbackInfo& Arguments);
	
public:
	std::shared_ptr<V8Storage<v8::Object>>	mHandle;
	TV8Container*				mContainer;
};


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
	
	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

	void									OnNewFrame(size_t StreamIndex);
	static v8::Local<v8::Value>				Free(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetNextFrame(const v8::CallbackInfo& Arguments);
	
	static std::shared_ptr<TMediaExtractor>	AllocExtractor(const TMediaExtractorParams& Params);

public:
	std::shared_ptr<V8Storage<v8::Function>>	mOnFrameFilter;
	
	std::shared_ptr<TMediaExtractor>&		mExtractor = mObject;
};




extern const char AvcDecoder_TypeName[];
class TAvcDecoderWrapper : public TObjectWrapper<AvcDecoder_TypeName,Broadway::TDecoder>
{
public:
	TAvcDecoderWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	//~TAvcDecoderWrapper();
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;
	
	void									OnNewFrame(const SoyPixelsImpl& Pixels);	//	call onPictureDecoded
	static v8::Local<v8::Value>				Decode(const v8::CallbackInfo& Arguments);
	
public:
	std::shared_ptr<Broadway::TDecoder>&		mBroadwayDecoder = mObject;
};
