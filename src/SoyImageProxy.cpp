#include "SoyImageProxy.h"
#include "SoyDebug.h"
#include "SoyImage.h"
#if defined(ENABLE_OPENGL)
#include "SoyOpengl.h"
#include "SoyOpenglContext.h"
#endif
#include <SoyPng.h>
#include <SoyMedia.h>
#include <SoyFilesystem.h>
#include <SoyStream.h>
#include <SoyImage.h>	//	soy image functions!


//	we'll make this a pool or something later
Array<SoyImageProxy*> SoyImageProxy::Pool;


SoyImageProxy::SoyImageProxy()
{
	Pool.PushBack(this);
}

SoyImageProxy::~SoyImageProxy()
{
	Free();
	Pool.Remove(this);
}

void SoyImageProxy::Flip()
{
	std::lock_guard<std::recursive_mutex> ThisLock(mPixelsLock);
	auto& Pixels = GetPixels();
	Pixels.Flip();
	OnPixelsChanged();
}


void SoyImageProxy::LoadFile(const std::string& Filename,std::function<void(const std::string&,const ArrayBridge<uint8_t>&)> OnMetaFound)
{
	//	gr: feels like this function should be a generic soy thing
	
	//	load file
	Array<char> Bytes;
	Soy::FileToArray( GetArrayBridge(Bytes), Filename );
	TStreamBuffer BytesBuffer;
	BytesBuffer.Push( GetArrayBridge(Bytes) );

	//	alloc pixels
	std::shared_ptr<SoyPixels> NewPixels( new SoyPixels );
	
	if ( Soy::StringEndsWith( Filename, Png::FileExtensions, false ) )
	{
		Png::Read( *NewPixels, BytesBuffer, OnMetaFound );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Jpeg::FileExtensions, false ) )
	{
		Jpeg::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Gif::FileExtensions, false ) )
	{
		Gif::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Tga::FileExtensions, false ) )
	{
		Tga::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Bmp::FileExtensions, false ) )
	{
		Bmp::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Psd::FileExtensions, false ) )
	{
		Psd::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}

	throw Soy::AssertException( std::string("Unhandled image file extension of ") + Filename );
}


void SoyImageProxy::SetLinearFilter(bool LinearFilter)
{
	//	for now, only allow this pre-creation
	//	what we could do, is queue an opengl job. but if we're IN a job now, it'll set it too late
	//	OR, queue it to be called before next GetTexture()
	if ( mOpenglTexture != nullptr )
		throw Soy::AssertException("Cannot change linear filter setting if texture is already created");

	mLinearFilter = LinearFilter;
}


void SoyImageProxy::Copy(SoyImageProxy& That)
{
	auto& This = *this;

	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	std::lock_guard<std::recursive_mutex> ThatLock(That.mPixelsLock);

	auto& ThisPixels = This.GetPixels();
	auto& ThatPixels = That.GetPixels();

	ThisPixels.Copy(ThatPixels);
	This.OnPixelsChanged();
}


void SoyImageProxy::Resize(size_t NewWidth,size_t NewHeight)
{
	std::lock_guard<std::recursive_mutex> ThisLock(mPixelsLock);
	
	auto& ThisPixels = GetPixels();
	
	ThisPixels.ResizeFastSample( NewWidth, NewHeight );
	OnPixelsChanged();
}



void SoyImageProxy::Clip(int x,int y,int w,int h)
{
	Soy::TScopeTimerPrint Timer(__func__,5);

	if ( x < 0 || y < 0 || w <= 0 || h <= 0 )
	{
		std::stringstream Error;
		Error << "Clip( " << x << ", " << y << ", " << w << ", " << h << ") out of bounds";
		throw Soy::AssertException(Error.str());
	}

	std::lock_guard<std::recursive_mutex> ThisLock(mPixelsLock);
	
	auto& ThisPixels = GetPixels();
	
	ThisPixels.Clip( x, y, w, h );
	OnPixelsChanged();
}


void SoyImageProxy::SetFormat(SoyPixelsFormat::Type NewFormat)
{
	//	gr: currently only handling pixels
	std::lock_guard<std::recursive_mutex> ThisLock(mPixelsLock);
	if ( mPixelsVersion != GetLatestVersion() )
		throw Soy::AssertException("Image.SetFormat only works on pixels at the moment, and that's not the latest version");

	auto& Pixels = GetPixels();
	Pixels.SetFormat(NewFormat);
	OnPixelsChanged();
}


//	gr: just destruct?
void SoyImageProxy::Free()
{
	std::lock_guard<std::recursive_mutex> ThisLock(mPixelsLock);

	//	clear pixels
	mPixels.reset();
	mPixelsVersion = 0;


	if ( mSokolImage )
	{
		mSokolImageFree();
		mSokolImage = 0;
		mSokolImageVersion = 0;
	}

	
	//if ( mOpenglTexture )
	//	mOpenglTexture->mAutoRelease = false;
	

	//	clear gl
	if ( mOpenglTextureDealloc )
	{
		mOpenglTextureDealloc();
		mOpenglTextureDealloc = nullptr;
	}
	mOpenglTexture.reset();
	mOpenglTextureVersion = 0;
	
	mOpenglLastPixelReadBuffer.reset();
	mOpenglLastPixelReadBufferVersion = 0;
	
	//	clear pixel buffer
	mPixelBuffer.reset();
	mPixelBufferMeta = SoyPixelsMeta();
	mPixelBufferVersion = 0;
}


void SoyImageProxy::GetRgba8(std::function<void(ArrayBridge<uint8_t>&&)> OnPixelArray,bool AllowBgraAsRgba)
{
	Soy::TScopeTimerPrint Timer(__func__,5);
	
	//	gr: this func will probably need to return a promise if reading from opengl etc (we want it to be async anyway!)
	auto& CurrentPixels = GetPixels();
	
	//	convert pixels if they're in the wrong format
	std::shared_ptr<SoyPixels> ConvertedPixels;
	SoyPixelsImpl* pPixels = nullptr;
	if ( CurrentPixels.GetFormat() == SoyPixelsFormat::RGBA )
	{
		pPixels = &CurrentPixels;
	}
	else if ( AllowBgraAsRgba && CurrentPixels.GetFormat() == SoyPixelsFormat::BGRA )
	{
		pPixels = &CurrentPixels;
	}
	else
	{
		Soy::TScopeTimerPrint Timer("GetRgba8 conversion to RGBA", 5 );
		ConvertedPixels.reset( new SoyPixels(CurrentPixels) );
		ConvertedPixels->SetFormat( SoyPixelsFormat::RGBA );
		pPixels = ConvertedPixels.get();
	}	
	auto& Pixels = *pPixels;
	
	auto& PixelsArray = Pixels.GetPixelsArray();
	
	OnPixelArray(GetArrayBridge(PixelsArray));
}


void SoyImageProxy::GetPixelArrayTyped(std::function<void(ArrayBridge<uint8_t>&&)> OnBuffer8,std::function<void(ArrayBridge<uint16_t>&&)> OnBuffer16,std::function<void(ArrayBridge<float>&&)> OnBufferFloat)
{
	Soy::TScopeTimerPrint Timer(__func__,5);
	
	auto& Pixels = GetPixels();
	auto& PixelsArray = Pixels.GetPixelsArray();
	
	//	for float & 16 bit formats, convert to their proper javascript type
	auto ComponentSize = SoyPixelsFormat::GetBytesPerChannel(Pixels.GetFormat());
	if ( ComponentSize == 4 )
	{
		auto Pixelsf = GetArrayBridge(PixelsArray).GetSubArray<float>(0, PixelsArray.GetSize() / ComponentSize);
		OnBufferFloat(GetArrayBridge(Pixelsf));
	}
	else if (ComponentSize == 2)
	{
		auto Pixels16 = GetArrayBridge(PixelsArray).GetSubArray<uint16_t>(0, PixelsArray.GetSize() / ComponentSize);
		OnBuffer16(GetArrayBridge(Pixels16));
	}
	else
	{
		OnBuffer8(GetArrayBridge(PixelsArray));
	}
}


void SoyImageProxy::GetPixelBufferPixels(std::function<void(const ArrayBridge<SoyPixelsImpl*>&,float3x3&)> Callback)
{
	if ( !mPixelBuffer )
		throw Soy::AssertException("Can't get pixel buffer pixels with no pixelbuffer");
	
	if ( mPixelBufferVersion != GetLatestVersion() )
		throw Soy::AssertException("Trying to get pixel buffer pixels that are out of date");

	//	lock pixels
	BufferArray<SoyPixelsImpl*,2> Textures;
	float3x3 Transform;
	mPixelBuffer->Lock( GetArrayBridge(Textures), Transform );
	try
	{
		Callback( GetArrayBridge(Textures), Transform );
		mPixelBuffer->Unlock();
	}
	catch(std::exception& e)
	{
		mPixelBuffer->Unlock();
		throw;
	}
}


std::shared_ptr<Opengl::TTexture> SoyImageProxy::GetTexturePtr()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	/*auto& Texture = */GetTexture();
	return mOpenglTexture;
}

void SoyImageProxy::GetTexture(Opengl::TContext& Context,std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError)
{
	//std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	//	already created & current version
	if ( mOpenglTexture != nullptr )
	{
		if ( mOpenglTextureVersion == GetLatestVersion() )
		{
			OnTextureLoaded();
			return;
		}
	}
	
	if ( !mPixels && !mPixelBuffer )
		throw Soy::AssertException("Trying to get opengl texture when we have no pixels/pixelbuffer");

#if defined(ENABLE_OPENGL)
	auto* pContext = &Context;
	auto AllocAndOrUpload = [=]
	{
		//	gr: this will need to be on the context's thread
		//		need to fail here if we're not
		try
		{
			std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
			Soy::TScopeTimerPrint Timer("TImageWrapper::GetTexture::Alloc/Upload", 10 );

			auto AllocTexture = [&](const SoyPixelsMeta& Meta)
			{
				auto TextureSlot = pContext->mCurrentTextureSlot++;
				if ( mOpenglTexture == nullptr )
				{
					//std::Debug << "Creating new opengl texture " << Meta << " in slot " << TextureSlot << std::endl;
					mOpenglTexture.reset( new Opengl::TTexture( Meta, GL_TEXTURE_2D, TextureSlot ) );
					//std::Debug << "<<< " << mOpenglTexture->mTexture.mName << std::endl;
					//this->mOpenglClientStorage.reset( new SoyPixels );
					//mOpenglTexture->mClientBuffer = this->mOpenglClientStorage;
				
					//mOpenglTexture->mAutoRelease = false;
					
					//	alloc the deffered delete func
					mOpenglTextureDealloc = [this,pContext]
					{
						//	gr: this context can be deleted here...
						pContext->QueueDelete(mOpenglTexture);
					};
				}
				mOpenglTexture->Bind(TextureSlot);
				mOpenglTexture->SetFilter( mLinearFilter );
				mOpenglTexture->SetRepeat( mRepeating );
			};

			SoyGraphics::TTextureUploadParams UploadParams;
			UploadParams.mAllowClientStorage = false;
			
			if ( GetLatestVersion() == mPixelsVersion )
			{
				AllocTexture( mPixels->GetMeta() );
				mOpenglTexture->Write( *mPixels, UploadParams );
				mOpenglTextureVersion = mPixelsVersion;
			}
			else if ( GetLatestVersion() == mPixelBufferVersion )
			{
				auto CopyPixels = [&](const ArrayBridge<SoyPixelsImpl*>& Textures,float3x3& Transform)
				{
					auto& Pixels = *Textures[0];
					mPixelBufferMeta = Pixels.GetMeta();
					AllocTexture( mPixelBufferMeta );
					mOpenglTexture->Write( Pixels, UploadParams );
					mOpenglTextureVersion = mPixelBufferVersion;
				};
				GetPixelBufferPixels(CopyPixels);
			}
			else
			{
				throw Soy::AssertException("Don't know where to get meta for new opengl texture");
			}
			OnTextureLoaded();
		}
		catch(std::exception& e)
		{
			OnError( e.what() );
		}
	};
	
	if ( Context.IsLockedToThisThread() )
	{
		AllocAndOrUpload();
	}
	else
	{
		Context.PushJob( AllocAndOrUpload );
	}
#else
	throw Soy::AssertException("Opengl not enabled in build");
#endif
}

Opengl::TTexture& SoyImageProxy::GetTexture()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	if ( !mOpenglTexture )
		throw Soy::AssertException("Image missing opengl texture. Accessing before generating.");
	
	if ( mOpenglTextureVersion != GetLatestVersion() )
		throw Soy::AssertException("Opengl texture is out of date");
	
	return *mOpenglTexture;
}



void SoyImageProxy::GetPixels(SoyPixelsImpl& CopyTarget)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Pixels = GetPixels();
	CopyTarget.Copy(Pixels);
}

void SoyImageProxy::SetPixelsMeta(SoyPixelsMeta Meta)
{
	bool MetaIsSame = false;
	try
	{
		auto CurrentMeta = GetMeta();
		MetaIsSame = CurrentMeta == Meta;
	}
	catch(std::exception& e)
	{
		//	probably have no backing buffer
	}
	
	if( MetaIsSame )
		return;
	
	//	gr: if we have this combination, i need to store an extra meta field :/
	if ( mPixelBuffer )
	{
		auto LatestVersion = GetLatestVersion();
		if ( mPixelBufferVersion == LatestVersion )
			throw Soy::AssertException("SoyImageProxy::SetMeta to new meta, but we cannot re-use the pixel buffer meta member. Needs more code!");
	}
	
	mPixelBufferMeta = Meta;
	mPixelBufferVersion = GetLatestVersion()+1;
}

SoyPixelsMeta SoyImageProxy::GetMeta()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	auto LatestVersion = GetLatestVersion();

	//	opengl may have been released!
	if ( mOpenglTextureVersion == LatestVersion && mOpenglTexture )
	{
#if defined(ENABLE_OPENGL)
		return mOpenglTexture->GetMeta();
#else
		throw Soy::AssertException("opengl not enabled in build");
#endif
	}
	
	if ( mPixelsVersion == LatestVersion && mPixels )
		return mPixels->GetMeta();

	if ( mPixelBufferVersion == LatestVersion )
	{
		if ( mPixelBufferMeta.IsValid() )
			return mPixelBufferMeta;
	}
	
	if ( mPixelBufferVersion == LatestVersion && mPixelBuffer )
	{	
		//	need to fetch the pixels to get the meta :/
		//	so we need to read the pixels, then get the meta from there
		BufferArray<SoyPixelsImpl*,2> Textures;
		float3x3 Transform;
		mPixelBuffer->Lock( GetArrayBridge(Textures), Transform );
		try
		{
			//	gr: there's a chance here, some TPixelBuffers unlock and then release the data.
			//		need to not let that happen
			mPixelBufferMeta = Textures[0]->GetMeta();
			mPixelBuffer->Unlock();
			return mPixelBufferMeta;
		}
		catch(std::exception& e)
		{
			mPixelBuffer->Unlock();
			throw;
		}
	}
	
	if ( mPixelsVersion == LatestVersion && mPixels )
		return mPixels->GetMeta();
	
	throw Soy::AssertException("Don't know where to get meta from");
}

SoyPixelsImpl& SoyImageProxy::GetPixels()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	if ( mPixelsVersion < GetLatestVersion() && mPixelBufferVersion == GetLatestVersion() )
	{
		//	if we have no pixel buffer, we just have meta and nothing else.
		//	make a new buffer!
		if ( !mPixelBuffer )
		{
			mPixels.reset( new SoyPixels(mPixelBufferMeta) );
			mPixelsVersion = mPixelBufferVersion;
			return *mPixels;
		}		
		
		//	grab pixels from image buffer, make the pixels the latest and return that reference
		auto CopyPixels = [&](const ArrayBridge<SoyPixelsImpl*>& Pixels,float3x3& Transform)
		{
			mPixels.reset( new SoyPixels );
			mPixels->Copy( *Pixels[0] );
			mPixelsVersion = mPixelBufferVersion;
		};
		this->GetPixelBufferPixels( CopyPixels );
		return *mPixels;
	}
	

	if ( mPixelsVersion < GetLatestVersion() )
	{
		std::stringstream Error;
		Error << "Image pixels(v" << mPixelsVersion <<") are out of date (v" << GetLatestVersion() << ")";
		throw Soy::AssertException(Error.str());
	}
	
	//	is latest and not allocated, this is okay, lets just alloc
	if ( mPixelsVersion == 0 && mPixels == nullptr )
	{
		mPixels.reset( new SoyPixels );
		mPixelsVersion = 1;
	}
	
	if ( mPixels == nullptr )
	{
		std::stringstream Error;
		Error << "Image pixels(v" << mPixelsVersion <<") latest, but null?";
		throw Soy::AssertException(Error.str());
	}
	
	return *mPixels;
}

size_t SoyImageProxy::GetLatestVersion() const
{
	size_t MaxVersion = mPixelsVersion;
	if ( mOpenglTextureVersion > MaxVersion )
		MaxVersion = mOpenglTextureVersion;
	
	if (mPixelBufferVersion > MaxVersion)
		MaxVersion = mPixelBufferVersion;

	if (mSokolImageVersion > MaxVersion)
		MaxVersion = mSokolImageVersion;

	return MaxVersion;
}

void SoyImageProxy::SetOpenglTexture(const Opengl::TAsset& Texture)
{
#if defined(ENABLE_OPENGL)
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	//	todo: delete old texture
	mOpenglTexture.reset(new Opengl::TTexture(Texture.mName, SoyPixelsMeta(), GL_TEXTURE_2D));
	auto LatestVersion = GetLatestVersion();
	mOpenglTextureVersion = LatestVersion + 1;
#else
	throw Soy::AssertException("Opengl not supported");
#endif
}


void SoyImageProxy::OnOpenglTextureChanged(Opengl::TContext& Context)
{
#if defined(ENABLE_OPENGL)
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	if ( !mOpenglTexture )
		throw Soy::AssertException("OnOpenglChanged with null texture");

	//	is now latest version
	auto LatestVersion = GetLatestVersion();
	mOpenglTextureVersion = LatestVersion+1;
	auto TextureSlot = Context.mCurrentTextureSlot++;
	mOpenglTexture->Bind(TextureSlot);
	mOpenglTexture->RefreshMeta();
#else
	throw Soy::AssertException("Opengl not enabled");
#endif
}



void SoyImageProxy::OnPixelsChanged()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto LatestVersion = GetLatestVersion();
	mPixelsVersion = LatestVersion+1;
	
	if ( !mPixels )
	{
		std::Debug << __PRETTY_FUNCTION__ << " changed but no pixels (mPixelBufferMeta=" << mPixelBufferMeta << ")" << std::endl;
		return;
	}
	
	auto PixelsMeta = mPixels->GetMeta();
	if ( PixelsMeta != mPixelBufferMeta )
	{
		if ( mPixelBufferMeta.IsValid() )
		{
			std::Debug << "Pixel meta has changed from " << mPixelBufferMeta << " to " << PixelsMeta << std::endl;
		}
	}
}

void SoyImageProxy::SetPixels(const SoyPixelsImpl& NewPixels)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixels.reset( new SoyPixels(NewPixels) );
	OnPixelsChanged();
}

void SoyImageProxy::SetPixels(std::shared_ptr<SoyPixelsImpl> NewPixels)
{
	//if ( NewPixels->GetFormat() != SoyPixelsFormat::RGB )
	//	std::Debug << "Setting image to pixels: " << NewPixels->GetMeta() << std::endl;
	
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixels = NewPixels;
	OnPixelsChanged();
}

void SoyImageProxy::SetPixelBuffer(std::shared_ptr<TPixelBuffer> NewPixels)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixelBuffer = NewPixels;
	mPixelBufferVersion = GetLatestVersion()+1;
}

void SoyImageProxy::ReadOpenglPixels(SoyPixelsFormat::Type Format)
{
#if defined(ENABLE_OPENGL)
	//	gr: this needs to be in the opengl thread!
	//Context.IsInThread

	if (!mOpenglTexture)
		throw Soy::AssertException("Trying to ReadOpenglPixels with no texture");

	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	//	warning in case we haven't actually updated
	if (mPixelsVersion >= mOpenglTextureVersion)
		std::Debug << "Warning, overwriting newer/same pixels(v" << mPixelsVersion << ") with gl texture (v" << mOpenglTextureVersion << ")";
	//	if we have no pixels, alloc
	if (mPixels == nullptr)
		mPixels.reset(new SoyPixels);

	auto Flip = false;

	mPixels->GetMeta().DumbSetFormat(Format);
	mPixels->GetPixelsArray().SetSize(mPixels->GetMeta().GetDataSize());

	mOpenglTexture->Read(*mPixels, Format, Flip);
	mPixelsVersion = mOpenglTextureVersion;
#else
	throw Soy::AssertException("Opengl not enabled");
#endif
}

void SoyImageProxy::SetOpenglLastPixelReadBuffer(std::shared_ptr<Array<uint8_t>> PixelBuffer)
{
#if defined(ENABLE_OPENGL)
	Soy::TScopeTimerPrint Timer(__func__, 5);
	if (GetLatestVersion() != mOpenglTextureVersion)
	{
		std::stringstream Error;
		Error << __func__ << " expected opengl (" << mOpenglTextureVersion << ") to be latest version (" << GetLatestVersion() << ")";
		throw Soy::AssertException(Error.str());
	}

	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	mOpenglLastPixelReadBuffer = PixelBuffer;
	mOpenglLastPixelReadBufferVersion = mOpenglTextureVersion;
#else
	throw Soy::AssertException("Opengl not enabled");
#endif
}

void SoyImageProxy::SetSokolImage(uint32_t Handle,std::function<void()> Free)
{
	mSokolImage = Handle;
	mSokolImageVersion = 0;
	mSokolImageFree = Free;
}

void SoyImageProxy::OnSokolImageChanged()
{
	if ( !HasSokolImage() )
		throw Soy::AssertException("Sokol image changed, but no handle");
	mSokolImageVersion = GetLatestVersion()+1;
}

void SoyImageProxy::OnSokolImageUpdated()
{
	if ( !HasSokolImage() )
		throw Soy::AssertException("Sokol image updated, but no handle");
	mSokolImageVersion = GetLatestVersion();
}

bool SoyImageProxy::HasSokolImage()
{
	return mSokolImage != 0;
}

uint32_t SoyImageProxy::GetSokolImage(bool& LatestVersion)
{
	LatestVersion = mSokolImageVersion == GetLatestVersion();
	//	throw here if not latest?
	return mSokolImage;
}


