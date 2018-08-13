#pragma once
#include "TV8Container.h"
#include "TV8ObjectWrapper.h"
#include <SoyPixels.h>
#include <SoyVector.h>

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;


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
extern const char Image_TypeName[];
class TImageWrapper : public TObjectWrapper<Image_TypeName,SoyPixels>
{
public:
	TImageWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This ),
		mLinearFilter			( false ),
		mRepeating				( false ),
		mPixelsVersion			( 0 ),
		mOpenglTextureVersion	( 0 ),
		mPixelBufferVersion		( 0 ),
		mPixels					( mObject )
	{
	}
	~TImageWrapper();
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

	static v8::Local<v8::Value>				Alloc(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Flip(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				LoadFile(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetWidth(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetHeight(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetRgba8(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				SetLinearFilter(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Copy(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Resize(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Clear(const v8::CallbackInfo& Arguments);

	static TImageWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TImageWrapper>( Value, 0 );	}
	
	void									DoLoadFile(const std::string& Filename);
	void									DoSetLinearFilter(bool LinearFilter);
	void									GetTexture(Opengl::TContext& Context,std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError);
	const Opengl::TTexture&					GetTexture();
	SoyPixels&								GetPixels();
	void									GetPixels(SoyPixelsImpl& CopyTarget);	//	safely copy pixels

	//	we consider version 0 uninitisalised
	size_t									GetLatestVersion() const;
	void									OnOpenglTextureChanged();
	void									ReadOpenglPixels();
	void									SetPixels(const SoyPixelsImpl& NewPixels);
	void									SetPixels(std::shared_ptr<SoyPixels> NewPixels);
	void									SetPixelBuffer(std::shared_ptr<TPixelBuffer> NewPixels);
	SoyPixelsMeta							GetMeta();
	void									GetPixelBufferPixels(std::function<void(const ArrayBridge<SoyPixelsImpl*>&,float3x3&)> Callback);	//	lock & unlock pixels for processing

protected:
	void								Free();
	
	
protected:
	std::recursive_mutex				mPixelsLock;			//	not sure if we need it for the others?
	std::shared_ptr<SoyPixels>&			mPixels = mObject;
	size_t								mPixelsVersion;			//	opengl texture changed

	std::shared_ptr<Opengl::TTexture>	mOpenglTexture;
	std::function<void()>				mOpenglTextureDealloc;
	size_t								mOpenglTextureVersion;	//	pixels changed

	//	abstracted pixel buffer from media
	std::shared_ptr<TPixelBuffer>		mPixelBuffer;
	size_t								mPixelBufferVersion;
	SoyPixelsMeta						mPixelBufferMeta;
	
	//	texture options
	bool								mLinearFilter;
	bool								mRepeating;
};

