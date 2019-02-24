#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"
#include "TV8ObjectWrapper.h"



namespace HidApi
{
	class TDevice;
}

namespace ApiInput
{
	void	Bind(TV8Container& Container);
}



extern const char InputDevice_TypeName[];
class TInputDeviceWrapper : public TObjectWrapper<InputDevice_TypeName,HidApi::TDevice>
{
public:
	TInputDeviceWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

	//void									OnStateChanged();
	static v8::Local<v8::Value>				GetState(const v8::CallbackInfo& Arguments);
	

public:
	std::shared_ptr<HidApi::TDevice>&		mDevice = mObject;
};

