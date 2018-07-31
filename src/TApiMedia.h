#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"



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
	v8::Persistent<v8::Object>	mHandle;
	TV8Container*				mContainer;
};

