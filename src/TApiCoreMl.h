#pragma once
#include "TBind.h"

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

	virtual void 		Construct(Bind::TCallback& Arguments) override;

	static void			Yolo(Bind::TCallback& Arguments);
	static void			Hourglass(Bind::TCallback& Arguments);
	static void			Cpm(Bind::TCallback& Arguments);
	static void			OpenPose(Bind::TCallback& Arguments);
	static void			SsdMobileNet(Bind::TCallback& Arguments);
	static void			MaskRcnn(Bind::TCallback& Arguments);

	
protected:
	std::shared_ptr<CoreMl::TInstance>&		mCoreMl = mObject;
};

