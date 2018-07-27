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
	class TContext;
}

//	an image is a generic accessor for pixels, opengl textures, etc etc
class TImageWrapper
{
public:
	TImageWrapper(TV8Container& Container) :
		mContainer				( Container ),
		mLinearFilter			( false ),
		mRepeating				( false ),
		mPixelsVersion			( 0 ),
		mOpenglTextureVersion	( 0 )
	{
	}
	
	static std::string						GetObjectTypeName()	{	return "Image";	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static v8::Local<v8::Value>				Alloc(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				LoadFile(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetWidth(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetHeight(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetRgba8(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				SetLinearFilter(const v8::CallbackInfo& Arguments);

	static TImageWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TImageWrapper>( Value, 0 );	}
	
	void									DoLoadFile(const std::string& Filename);
	void									DoSetLinearFilter(bool LinearFilter);
	void									GetTexture(std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError);
	const Opengl::TTexture&					GetTexture();
	SoyPixels&								GetPixels();

	//	we consider version 0 uninitisalised
	size_t									GetLatestVersion() const;
	void									OnOpenglTextureChanged();
	void									ReadOpenglPixels();
	
public:
	v8::Persist<v8::Object>				mHandle;
	TV8Container&						mContainer;

protected:
	std::shared_ptr<SoyPixels>			mPixels;
	size_t								mPixelsVersion;			//	opengl texture changed
	std::shared_ptr<Opengl::TTexture>	mOpenglTexture;
	size_t								mOpenglTextureVersion;	//	pixels changed
	
	//	texture options
	bool								mLinearFilter;
	bool								mRepeating;
};

