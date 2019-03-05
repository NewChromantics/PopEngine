#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"
#include "TV8ObjectWrapper.h"



namespace Hid
{
	class TDevice;
}

namespace ApiInput
{
	void	Bind(TV8Container& Container);
}



extern const char InputDevice_TypeName[];
class TInputDeviceWrapper : public TObjectWrapper<InputDevice_TypeName,Hid::TDevice>
{
public:
	TInputDeviceWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	virtual void 							Construct(v8::TCallback& Arguments) override;

	//void									OnStateChanged();
	static v8::Local<v8::Value>				GetState(v8::TCallback& Arguments);
	

public:
	std::shared_ptr<Hid::TDevice>&			mDevice = mObject;
};

