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
	//DecoderParams.mPushBlockSleepMs = 30000;
	DecoderParams.mPixelBufferParams.mPreSeek = SoyTime(0000ull);
	DecoderParams.mPixelBufferParams.mDebugFrameSkipping = false;
	DecoderParams.mPixelBufferParams.mPopFrameSync = false;
	DecoderParams.mPixelBufferParams.mAllowPushRejection = false;
	DecoderParams.mForceNonPlanarOutput = true;
	
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
	mDecoder = PopMovieDecoder::AllocDecoder( Params );
	mDecoder->StartMovie();
	
	auto OnDecoded = [this](const SoyTime& Time)
	{
		this->Wake();
	};
	
	//WakeOnEvent( mDecoder->GetPixelBufferManager().mOnFrameDecoded );
	auto StreamIndex = mDecoder->GetVideoStreamIndex(0);
	mDecoder->GetPixelBufferManager(StreamIndex).mOnFrameDecoded.AddListener( OnDecoded );
/*
	//	decode every frame we find
	auto AutoIncrementTime = [this](SoyTime& Timecode)
	{
		//	move decoder along to decode this frame
		mDecoder->SetPlayerTime( Timecode );
	};
	mDecoder->GetPixelBufferManager().mOnFrameFound.AddListener( AutoIncrementTime );
	*/
	SoyTime Future( 99999999ull );
	mDecoder->SetPlayerTime( Future );
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
	
	auto StreamIndex = mDecoder->GetVideoStreamIndex(0);
	auto NextFrameTime = mDecoder->GetPixelBufferManager(StreamIndex).GetNextPixelBufferTime();
	
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
	auto StreamIndex = mDecoder->GetVideoStreamIndex(0);
	auto& Buffer = mDecoder->GetPixelBufferManager(StreamIndex);
	auto NextFrameTime = Buffer.GetNextPixelBufferTime();
	auto PixelBuffer = Buffer.PopPixelBuffer( NextFrameTime );
	if ( !PixelBuffer )
		return true;
	
	auto PushFrame = [&](SoyPixelsImpl& Pixels)
	{
		static bool DoNewFrameLock = true;
		if ( DoNewFrameLock )
		{
			SoyPixelsImpl& NewFramePixels = LockNewFrame();
			NewFramePixels.Copy( Pixels );
			UnlockNewFrame( NextFrameTime );
		}
		else
		{
			//	send new frame
			OnNewFrame( Pixels, NextFrameTime );
		}
	};
	
	
	Array<SoyPixelsImpl*> Pixels;
	PixelBuffer->Lock( GetArrayBridge(Pixels) );
	if ( Pixels.IsEmpty() )
	{
		std::this_thread::sleep_for( std::chrono::milliseconds(10) );
		return true;
	}
	PushFrame( *Pixels[0] );
	PixelBuffer->Unlock();
	
	return true;
}


