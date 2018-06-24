#pragma once
#include "TV8Container.h"

class SoyPixels;

namespace ApiCommon
{
	void	Bind(TV8Container& Container);
}

namespace Opengl
{
	class TTexture;
}

//	an image is a generic accessor for pixels, opengl textures, etc etc
class TImageWrapper
{
public:
	TImageWrapper() :
		mContainer	( nullptr )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static v8::Local<v8::Value>				LoadFile(const v8::CallbackInfo& Arguments);

	static TImageWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TImageWrapper>( Value, 0 );	}
	
	void									DoLoadFile(const std::string& Filename);
	void									GetTexture(std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError);
	const Opengl::TTexture&					GetTexture();

public:
	v8::Persistent<v8::Object>			mHandle;
	std::shared_ptr<SoyPixels>			mPixels;
	std::shared_ptr<Opengl::TTexture>	mOpenglTexture;
	TV8Container*						mContainer;
};

