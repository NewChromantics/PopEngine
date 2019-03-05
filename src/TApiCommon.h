#pragma once
#include "TV8Container.h"
#include "TV8ObjectWrapper.h"
#include "SoyPixels.h"
#include "SoyVector.h"

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;

namespace JsCore
{
	class TContext;
}

//	engine stuff under the Pop namespace
namespace ApiPop
{
	void	Bind(TV8Container& Container);
	void	Bind(JsCore::TContext& Context);
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
		mOpenglLastPixelReadBufferVersion	( 0 ),
		mPixels					( mObject )
	{
	}
	~TImageWrapper();
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	virtual void 		Construct(Bind::TCallback& Arguments) override;

	static void			Alloc(Bind::TCallback& Arguments);
	static void			Flip(Bind::TCallback& Arguments);
	static void			LoadFile(Bind::TCallback& Arguments);
	static void			GetWidth(Bind::TCallback& Arguments);
	static void			GetHeight(Bind::TCallback& Arguments);
	static void			GetRgba8(Bind::TCallback& Arguments);
	static void			SetLinearFilter(Bind::TCallback& Arguments);
	static void			Copy(Bind::TCallback& Arguments);
	static void			WritePixels(Bind::TCallback& Arguments);
	static void			Resize(Bind::TCallback& Arguments);
	static void			Clip(Bind::TCallback& Arguments);
	static void			Clear(Bind::TCallback& Arguments);
	static void			SetFormat(Bind::TCallback& Arguments);
	static void			GetFormat(Bind::TCallback& Arguments);
	
	static TImageWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TImageWrapper>( Value, 0 );	}
	
	void									DoLoadFile(const std::string& Filename);
	void									DoSetLinearFilter(bool LinearFilter);
	void									GetTexture(Opengl::TContext& Context,std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError);
	Opengl::TTexture&						GetTexture();
	SoyPixels&								GetPixels();
	void									GetPixels(SoyPixelsImpl& CopyTarget);	//	safely copy pixels

	//	we consider version 0 uninitisalised
	size_t									GetLatestVersion() const;
	void									OnOpenglTextureChanged();
	void									ReadOpenglPixels(SoyPixelsFormat::Type Format);
	void									SetPixels(const SoyPixelsImpl& NewPixels);
	void									SetPixels(std::shared_ptr<SoyPixels> NewPixels);
	void									SetPixelBuffer(std::shared_ptr<TPixelBuffer> NewPixels);
	SoyPixelsMeta							GetMeta();
	void									GetPixelBufferPixels(std::function<void(const ArrayBridge<SoyPixelsImpl*>&,float3x3&)> Callback);	//	lock & unlock pixels for processing
	void									OnPixelsChanged();	//	increase version

	void									SetOpenglLastPixelReadBuffer(std::shared_ptr<Array<uint8_t>> PixelBuffer);
	
	
protected:
	void								Free();
	
public:
	std::string							mName;					//	for debug

protected:
	std::recursive_mutex				mPixelsLock;			//	not sure if we need it for the others?
	std::shared_ptr<SoyPixels>&			mPixels = mObject;
	size_t								mPixelsVersion;			//	opengl texture changed

	std::shared_ptr<Opengl::TTexture>	mOpenglTexture;
	std::function<void()>				mOpenglTextureDealloc;
	size_t								mOpenglTextureVersion;	//	pixels changed
	std::shared_ptr<SoyPixels>			mOpenglClientStorage;	//	gr: apple specific client storage for texture. currently kept away from normal pixels for safety, but merge later

public:
	//	temporary caching system for immediate mode glReadPixels
	std::shared_ptr<Array<uint8_t>>		mOpenglLastPixelReadBuffer;
	size_t								mOpenglLastPixelReadBufferVersion;

protected:
	//	abstracted pixel buffer from media
	std::shared_ptr<TPixelBuffer>		mPixelBuffer;
	size_t								mPixelBufferVersion;
	SoyPixelsMeta						mPixelBufferMeta;
	
	//	texture options
	bool								mLinearFilter;
	bool								mRepeating;
};

