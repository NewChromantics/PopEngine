#include "SoyMovieDecoder.h"
#include <RemoteArray.h>
#include <Build/PopMovieTextureOsxFramework.framework/Headers/PopMovieTextureOsxFramework.h>


void TMovieDecoderContainer::GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas)
{
	for ( int m=0;	m<mMovies.GetSize();	m++ )
	{
		Metas.PushBack( mMovies[m]->GetMeta() );
	}
}

std::shared_ptr<TVideoDevice> TMovieDecoderContainer::AllocDevice(const TVideoDeviceMeta& Meta,std::stringstream& Error)
{
	//	todo: look for existing
	
	TVideoDecoderParams DecoderParams;
	DecoderParams.mFilename = Meta.mName;
	DecoderParams.mDecodeAsFormat = SoyPixelsFormat::RGBA;
	//DecoderParams.mPushBlockSleepMs = 30000;
	
	try
	{
		std::shared_ptr<TMovieDecoder> Movie( new TMovieDecoder( DecoderParams, Meta.mSerial, nullptr ) );
		mMovies.PushBack( Movie );
		return Movie;
	}
	catch ( std::exception& e )
	{
		Error << "Failed to create movie decoder: " << e.what();
		return nullptr;
	}
}

TVideoDeviceMeta GetDecoderMeta(const TVideoDecoderParams& Params,const std::string& Serial)
{
	TVideoDeviceMeta Meta( Serial, Params.mFilename );
	Meta.mVideo = true;
	Meta.mTimecode = true;
	//Meta.mConnected
	return Meta;
}

TMovieDecoder::TMovieDecoder(const TVideoDecoderParams& Params,const std::string& Serial,std::shared_ptr<Opengl::TContext> OpenglContext) :
TVideoDevice	( GetDecoderMeta(Params,Serial) ),
SoyWorkerThread	( Params.mFilename, SoyWorkerWaitMode::Wake ),
mSerial			( Serial )
{
	mDecoder = Platform::AllocDecoder( Params, nullptr );
	mDecoder->StartMovie();
	WakeOnEvent( mDecoder->mOnFrameDecoded );
	
	Start();
}

TVideoDeviceMeta TMovieDecoder::GetMeta() const
{
	if ( !mDecoder )
		return TVideoDeviceMeta();
	
	return GetDecoderMeta( mDecoder->mParams, mSerial );
}

bool TMovieDecoder::CanSleep()
{
	if ( !mDecoder )
		return true;
	
	auto NextFrameTime = mDecoder->GetNextPixelBufferTime();
	
	//	got a frame to read, don't sleep!
	if ( NextFrameTime.IsValid() )
		return false;
	
	return true;
}

bool TMovieDecoder::Iteration()
{
	if ( !mDecoder )
		return true;
	
	//	pop pixels
	auto NextFrameTime = mDecoder->GetNextPixelBufferTime();
	auto PixelBuffer = mDecoder->PopPixelBuffer( NextFrameTime );
	if ( !PixelBuffer )
		return true;
	
	Array<SoyPixelsImpl*> Pixels;
	PixelBuffer->Lock( GetArrayBridge(Pixels) );
	if ( Pixels.IsEmpty() )
		return true;
	
	static bool DoNewFrameLock = true;
	if ( DoNewFrameLock )
	{
		SoyPixelsImpl& NewFramePixels = LockNewFrame();
		NewFramePixels.Copy( *Pixels[0] );
		PixelBuffer->Unlock();
		UnlockNewFrame( NextFrameTime );
	}
	else
	{
		//	send new frame
		OnNewFrame( *Pixels[0], NextFrameTime );
		PixelBuffer->Unlock();
	}
	
	std::this_thread::sleep_for( std::chrono::milliseconds(100) );
	
	return true;
}


