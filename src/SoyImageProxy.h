#pragma once

#include <SoyPixels.h>
class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;


namespace Opengl
{
	class TTexture;
	class TContext;
	class TAsset;
}

//	renamed to avoid clashes with SoyImage
class SoyImageProxy
{
public:
	static Array<SoyImageProxy*>	Pool;	//	counter atm
	
public:
	SoyImageProxy(const SoyImageProxy&) = delete;
	SoyImageProxy& operator=(const SoyImageProxy &) = delete;
	
	SoyImageProxy();
	~SoyImageProxy();
	
	SoyPixelsMeta		GetMeta();
	void				Free();
	void				Flip();
	void				Clip(int x,int y,int w,int h);
	void				Resize(size_t NewWidth, size_t NewHeight);
	void				SetFormat(SoyPixelsFormat::Type NewFormat);
	void				LoadFile(const std::string& Filename, std::function<void(const std::string&, const ArrayBridge<uint8_t>&)> OnMetaFound);
	void				SetLinearFilter(bool LinearFilter);
	void				Copy(SoyImageProxy& That);
	void				GetRgba8(std::function<void(ArrayBridge<uint8_t>&&)> OnPixelArray, bool AllowBgraAsRgba);
	void				GetPixelArrayTyped(std::function<void(ArrayBridge<uint8_t>&&)> OnBuffer8, std::function<void(ArrayBridge<uint16_t>&&)> OnBuffer16, std::function<void(ArrayBridge<float>&&)> OnBufferFloat);

	//	we consider version 0 uninitisalised
	size_t				GetLatestVersion() const;

	void				GetTexture(Opengl::TContext& Context,std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError);
	Opengl::TTexture&	GetTexture();
	std::shared_ptr<Opengl::TTexture>		GetTexturePtr();
	void				SetOpenglTexture(const Opengl::TAsset& Texture);
	void				OnOpenglTextureChanged(Opengl::TContext& Context);
	void				ReadOpenglPixels(SoyPixelsFormat::Type Format);
	void				SetOpenglLastPixelReadBuffer(std::shared_ptr<Array<uint8_t>> PixelBuffer);

	//	should probably always use this locking function to deal with pixels for multithread safety.
	//	callback should return true if it modified pixels
	void				GetPixels(std::function<bool(SoyPixelsImpl&)> PixelsCallback);
	SoyPixelsImpl&		GetPixels();// __deprecated;
	void				GetPixels(SoyPixelsImpl& CopyTarget);	//	safely copy pixels
	void				SetPixels(const SoyPixelsImpl& NewPixels);
	void				SetPixels(std::shared_ptr<SoyPixelsImpl> NewPixels);
	void				OnPixelsChanged();	//	increase version
	void				SetPixelsMeta(SoyPixelsMeta Meta);		//	if this meta is different to latest, we consider "no data" latest, but have valid meta

	void				SetPixelBuffer(std::shared_ptr<TPixelBuffer> NewPixels);
	void				GetPixelBufferPixels(std::function<void(const ArrayBridge<SoyPixelsImpl*>&,float3x3&)> Callback);	//	lock & unlock pixels for processing

	
	void				SetSokolImage(uint32_t Handle,std::function<void()> Free);	//	set handle, no version
	void				OnSokolImageChanged();	//	increase version
	void				OnSokolImageUpdated();	//	now matching latest version
	bool				HasSokolImage();
	uint32_t			GetSokolImage(bool& LatestVersion);

	
public:
	std::string			mName = "UninitialisedName";	//	for debug

protected:
	std::recursive_mutex				mPixelsLock;			//	not sure if we need it for the others?
	std::shared_ptr<SoyPixelsImpl>		mPixels;
	size_t								mPixelsVersion = 0;			//	opengl texture changed

	std::shared_ptr<Opengl::TTexture>	mOpenglTexture;
	std::function<void()>				mOpenglTextureDealloc;
	size_t								mOpenglTextureVersion = 0;	//	pixels changed
	std::shared_ptr<SoyPixels>			mOpenglClientStorage;	//	gr: apple specific client storage for texture. currently kept away from normal pixels for safety, but merge later

	uint32_t							mSokolImage = 0;
	size_t								mSokolImageVersion = 0;	//	pixels changed
	std::function<void()>				mSokolImageFree;
	
public:
	//	temporary caching system for immediate mode glReadPixels
	std::shared_ptr<Array<uint8_t>>		mOpenglLastPixelReadBuffer;
	size_t								mOpenglLastPixelReadBufferVersion = 0;

protected:
	//	abstracted pixel buffer from media
	std::shared_ptr<TPixelBuffer>		mPixelBuffer;
	size_t								mPixelBufferVersion = 0;
	SoyPixelsMeta						mPixelBufferMeta;
	
	//	texture options
	bool								mLinearFilter = false;
	bool								mRepeating = false;
};


