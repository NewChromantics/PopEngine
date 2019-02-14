#pragma once
#include "TV8Container.h"
#include "TV8ObjectWrapper.h"

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;

namespace ApiEzsift
{
	void	Bind(TV8Container& Container);
}

namespace Ezsift
{
	class TInstance;
}


extern const char Ezsift_TypeName[];
class TEzsiftWrapper : public TObjectWrapper<Ezsift_TypeName,Ezsift::TInstance>
{
public:
	TEzsiftWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

	static v8::Local<v8::Value>				GetFeatures(const v8::CallbackInfo& Arguments);

	
protected:
	std::shared_ptr<Ezsift::TInstance>&		mEzsift = mObject;
};


