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
	class TMobileNet;
}


//	an image is a generic accessor for pixels, opengl textures, etc etc
extern const char CoreMlMobileNet_TypeName[];
class TCoreMlMobileNetWrapper : public TObjectWrapper<CoreMlMobileNet_TypeName,CoreMl::TMobileNet>
{
public:
	TCoreMlMobileNetWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

	static v8::Local<v8::Value>				DetectObjects(const v8::CallbackInfo& Arguments);
	
protected:
	std::shared_ptr<CoreMl::TMobileNet>&	mMobileNet = mObject;
};

