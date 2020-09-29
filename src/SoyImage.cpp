#include "SoyImage.h"
#include "SoyDebug.h"
#include "SoyImage.h"
#if defined(ENABLE_OPENGL)
#include "SoyOpengl.h"
#include "SoyOpenglContext.h"
#endif
#include <SoyPng.h>
#include <SoyMedia.h>



SoyImage::~SoyImage()
{
	Free();
}

void SoyImage::Flip()
{
	auto& This = Params.This<TImageWrapper>();
	
	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	auto& Pixels = This.GetPixels();
	Pixels.Flip();
	This.OnPixelsChanged();
}

void SoyImage::LoadFile(const std:string& Filename)
{
	auto OnMetaFound = [&](const std::string& Section,const ArrayBridge<uint8_t>& Data)
	{
		auto This = Params.ThisObject();
		
		//	add meta if it's not there
		if ( !This.HasMember("Meta") )
			This.SetObjectFromString("Meta","{}");
		
		//	set this section as a meta
		auto ThisMeta = This.GetObject("Meta");
		if ( This.HasMember(Section) )
			std::Debug << "Overwriting image meta section " << Section << std::endl;
	
		ThisMeta.SetArray( Section, Data );
	};
	
	DoLoadFile( Filename, OnMetaFound );
}

void SoyImage::DoLoadFile(const std::string& Filename,std::function<void(const std::string&,const ArrayBridge<uint8_t>&)> OnMetaFound)
{
	//	gr: feels like this function should be a generic soy thing
	
	//	load file
	Array<char> Bytes;
	Soy::FileToArray( GetArrayBridge(Bytes), Filename );
	TStreamBuffer BytesBuffer;
	BytesBuffer.Push( GetArrayBridge(Bytes) );

	//	alloc pixels
	auto& Heap = GetContext().GetImageHeap();
	std::shared_ptr<SoyPixels> NewPixels( new SoyPixels(Heap) );
	
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


void SoyImage::SetLinearFilter(bool LinearFilter)
{
	//	for now, only allow this pre-creation
	//	what we could do, is queue an opengl job. but if we're IN a job now, it'll set it too late
	//	OR, queue it to be called before next GetTexture()
	if ( mOpenglTexture != nullptr )
		throw Soy::AssertException("Cannot change linear filter setting if texture is already created");

	mLinearFilter = LinearFilter;
}


void SoyImage::Copy(SoyImage& That)
{
	auto& This = *this;

	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	std::lock_guard<std::recursive_mutex> ThatLock(That.mPixelsLock);

	auto& ThisPixels = This.GetPixels();
	auto& ThatPixels = That.GetPixels();

	ThisPixels.Copy(ThatPixels);
	This.OnPixelsChanged();
}


void SoyImage::Resize(size_t NewWidth,size_t NewHeight)
{
	std::lock_guard<std::recursive_mutex> ThisLock(mPixelsLock);
	
	auto& ThisPixels = GetPixels();
	
	ThisPixels.ResizeFastSample( NewWidth, NewHeight );
	OnPixelsChanged();
}



void TImageWrapper::Clip(int x,int y,int w,int h)
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


void TImageWrapper::SetFormat(SoyPixelsFormat::Type NewFormat)
{
	//	gr: currently only handling pixels
	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	if ( This.mPixelsVersion != This.GetLatestVersion() )
		throw Soy::AssertException("Image.SetFormat only works on pixels at the moment, and that's not the latest version");

	auto& Pixels = This.GetPixels();
	Pixels.SetFormat(NewFormat);
	This.OnPixelsChanged();
}


//	gr: just destruct?
void TImageWrapper::Free()
{
	std::lock_guard<std::recursive_mutex> ThisLock(mPixelsLock);

	//	clear pixels
	mPixels.reset();
	mPixelsVersion = 0;
	

	
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


void SoyImage::GetRgba8(std::function<void(ArrayBridge<uint8_t>&&)> OnPixelArray,bool AllowBgraAsRgba)
{
	auto& Heap = prmem::gHeap;
	
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
		ConvertedPixels.reset( new SoyPixels(CurrentPixels,Heap) );
		ConvertedPixels->SetFormat( SoyPixelsFormat::RGBA );
		pPixels = ConvertedPixels.get();
	}	
	auto& Pixels = *pPixels;
	
	auto& PixelsArray = Pixels.GetPixelsArray();
	
	OnPixelArray(GetArrayBridge(PixelsArray));
}


void SoyImage::GetPixelBuffer(std::function<void(ArrayBridge<uint8_t>&&)> OnBuffer8,std::function<void(ArrayBridge<uint16_t>&&)> OnBuffer16,std::function<void(ArrayBridge<float>&&)> OnBufferFloat)
{
	Soy::TScopeTimerPrint Timer(__func__,5);
	
	auto& Pixels = This.GetPixels();
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


void SoyImage::GetPixelBufferPixels(std::function<void(const ArrayBridge<SoyPixelsImpl*>&,float3x3&)> Callback)
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


std::shared_ptr<Opengl::TTexture> SoyImage::GetTexturePtr()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	/*auto& Texture = */GetTexture();
	return mOpenglTexture;
}

void SoyImage::GetTexture(Opengl::TContext& Context,std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError)
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

Opengl::TTexture& SoyImage::GetTexture()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	if ( !mOpenglTexture )
		throw Soy::AssertException("Image missing opengl texture. Accessing before generating.");
	
	if ( mOpenglTextureVersion != GetLatestVersion() )
		throw Soy::AssertException("Opengl texture is out of date");
	
	return *mOpenglTexture;
}



void SoyImage::GetPixels(SoyPixelsImpl& CopyTarget)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Pixels = GetPixels();
	CopyTarget.Copy(Pixels);
}

SoyPixelsMeta SoyImage::GetMeta()
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

SoyPixelsImpl& SoyImage::GetPixels()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Heap = GetContext().GetImageHeap();

	if ( mPixelsVersion < GetLatestVersion() && mPixelBufferVersion == GetLatestVersion() )
	{
		//	grab pixels from image buffer
		auto CopyPixels = [&](const ArrayBridge<SoyPixelsImpl*>& Pixels,float3x3& Transform)
		{
			mPixels.reset( new SoyPixels(Heap) );
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
		mPixels.reset( new SoyPixels(Heap) );
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

size_t SoyImage::GetLatestVersion() const
{
	size_t MaxVersion = mPixelsVersion;
	if ( mOpenglTextureVersion > MaxVersion )
		MaxVersion = mOpenglTextureVersion;
	
	if ( mPixelBufferVersion > MaxVersion )
		MaxVersion = mPixelBufferVersion;
	
	return MaxVersion;
}

void SoyImage::SetOpenglTexture(const Opengl::TAsset& Texture)
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


void SoyImage::OnOpenglTextureChanged(Opengl::TContext& Context)
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



void SoyImage::OnPixelsChanged()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto LatestVersion = GetLatestVersion();
	mPixelsVersion = LatestVersion+1;
}

void SoyImage::SetPixels(const SoyPixelsImpl& NewPixels)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Heap = GetContext().GetImageHeap();
	mPixels.reset( new SoyPixels(NewPixels,Heap) );
	OnPixelsChanged();
}

void SoyImage::SetPixels(std::shared_ptr<SoyPixelsImpl> NewPixels)
{
	//if ( NewPixels->GetFormat() != SoyPixelsFormat::RGB )
	//	std::Debug << "Setting image to pixels: " << NewPixels->GetMeta() << std::endl;
	
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixels = NewPixels;
	OnPixelsChanged();
}

void SoyImage::SetPixelBuffer(std::shared_ptr<TPixelBuffer> NewPixels)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixelBuffer = NewPixels;
	mPixelBufferVersion = GetLatestVersion()+1;
}

void SoyImage::ReadOpenglPixels(SoyPixelsFormat::Type Format)
{
#if defined(ENABLE_OPENGL)
	//	gr: this needs to be in the opengl thread!
	//Context.IsInThread

	if (!mOpenglTexture)
		throw Soy::AssertException("Trying to ReadOpenglPixels with no texture");

	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	auto& Heap = GetContext().GetImageHeap();

	//	warning in case we haven't actually updated
	if (mPixelsVersion >= mOpenglTextureVersion)
		std::Debug << "Warning, overwriting newer/same pixels(v" << mPixelsVersion << ") with gl texture (v" << mOpenglTextureVersion << ")";
	//	if we have no pixels, alloc
	if (mPixels == nullptr)
		mPixels.reset(new SoyPixels(Heap));

	auto Flip = false;

	mPixels->GetMeta().DumbSetFormat(Format);
	mPixels->GetPixelsArray().SetSize(mPixels->GetMeta().GetDataSize());

	mOpenglTexture->Read(*mPixels, Format, Flip);
	mPixelsVersion = mOpenglTextureVersion;
#else
	throw Soy::AssertException("Opengl not enabled");
#endif
}

void SoyImage::SetOpenglLastPixelReadBuffer(std::shared_ptr<Array<uint8_t>> PixelBuffer)
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

void SoyImage::SetSokolImage(uint32_t Handle)
{
	mSokolImage = Handle;
	mSokolImageVersion = 0;
}

void SoyImage::OnSokolImageChanged()
{
	if ( !HasSokolImage() )
	throw Soy::AssertException("Sokol image changed, but no handle");
	mSokolImageVersion = GetLatestVersion()+1;
}

void SoyImage::OnSokolImageUpdated()
{
	if ( !HasSokolImage() )
	throw Soy::AssertException("Sokol image updated, but no handle");
	mSokolImageVersion = GetLatestVersion();
}

bool SoyImage::HasSokolImage()
{
	return mSokolImage != 0;
}

uint32_t SoyImage::GetSokolImage(bool& LatestVersion)
{
	LatestVersion = mSokolImageVersion == GetLatestVersion();
	return mSokolImage;
}


