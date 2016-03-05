#include "SoyMovieDecoder.h"
#include <RemoteArray.h>



PopVideoDecoderBase::PopVideoDecoderBase(const TVideoDecoderParams& Params)
{
	mInstance = PopMovie::Alloc( Params );
	Soy::Assert( mInstance!=nullptr, "Failed to create movie instance");
}

PopVideoDecoderBase::~PopVideoDecoderBase()
{
	if ( mInstance )
	{
		PopMovie::Free( mInstance->GetRef() );
	}
}
	

PopVideoDecoder::PopVideoDecoder(const TVideoDecoderParams& Params) :
	PopVideoDecoderBase	( Params ),
	SoyWorkerThread		( std::string("TVideoDecoder ") + Params.mFilename, SoyWorkerWaitMode::Sleep ),
	mStreamIndex		( 0 )
{
	Start();
}

PopVideoDecoder::~PopVideoDecoder()
{
	WaitToFinish();
}

bool PopVideoDecoder::Iteration()
{
	try
	{
		//	defer initial time until after first frame?
		if ( !mInitialTime.IsValid() )
			mInitialTime = SoyTime( true );
		
		//	update time
		SoyTime Now(true);
		Now -= mInitialTime;
		
		mInstance->SetTimestamp( Now );

		auto HandleFrame = [this](SoyPixelsImpl& Frame,SoyTime Time)
		{
			auto FrameAndTime = std::make_pair( &Frame, Time );
			mOnFrame.OnTriggered( FrameAndTime );
		};
		
		mInstance->UpdateTexture( HandleFrame, mStreamIndex );
	}
	catch (std::exception& e)
	{
		std::stringstream Error;
		Error << e.what();
		mOnError.OnTriggered( Error.str() );
	}
	
	//	detect eof
	
	return true;
}
