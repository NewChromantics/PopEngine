#pragma once
#include "TV8Container.h"
#include "TV8ObjectWrapper.h"

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;

namespace ApiCoreMl
{
	void	Bind(TV8Container& Container);
}

namespace CoreMl
{
	class TInstance;
}


extern const char CoreMl_TypeName[];
class TCoreMlWrapper : public TObjectWrapper<CoreMl_TypeName,CoreMl::TInstance>
{
public:
	TCoreMlWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

	static v8::Local<v8::Value>				Yolo(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Hourglass(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Cpm(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				OpenPose(const v8::CallbackInfo& Arguments);

protected:
	std::shared_ptr<CoreMl::TInstance>&		mCoreMl = mObject;
};

