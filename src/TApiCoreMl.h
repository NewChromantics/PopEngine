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

	virtual void 							Construct(v8::TCallback& Arguments) override;

	static v8::Local<v8::Value>				Yolo(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Hourglass(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Cpm(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				OpenPose(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				SsdMobileNet(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				MaskRcnn(v8::TCallback& Arguments);

	
protected:
	std::shared_ptr<CoreMl::TInstance>&		mCoreMl = mObject;
};

