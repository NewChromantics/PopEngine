#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"
#include "TV8ObjectWrapper.h"

class TInputWrapper;

namespace Soy
{
	class TInputDevice;
}

namespace ApiInput
{
	void	Bind(TV8Container& Container);
}

class TInputWrapper
{
public:
	TInputWrapper() :
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


extern const char InputDevice_TypeName[];
class TInputDeviceWrapper : public TObjectWrapper<InputDevice_TypeName,Soy::TInputDevice>
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
	std::shared_ptr<Soy::TInputDevice>&		mDevice = mObject;
};

