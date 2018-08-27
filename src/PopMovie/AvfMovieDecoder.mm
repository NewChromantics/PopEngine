#import "AvfMovieDecoder.h"
#import "AvfVideoCapture.h"
#import "AvfPixelBuffer.h"
#include <sstream>
#include <SoyAvf.h>
#include <SoyOpenglContext.h>

#if defined(TARGET_IOS)
#import <OpenGLES/ES2/gl.h>
#import <CoreVideo/CVOpenGLESTextureCache.h>
#endif

#include <SoyFilesystem.h>


//	resolve the best/compatible formats to use, this changes DecodeAsFormat to reflect the actual format
void GetAvailiblePixelFormats(ArrayBridge<std::pair<id,bool>>&& PixelFormats)
{
	//	prefer the fastest now...
	Array<id> Formats;
	Formats.PushBack( @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) );
	Formats.PushBack( @(kCVPixelFormatType_32BGRA) );
	Formats.PushBack( @(kCVPixelFormatType_24RGB) );
	Formats.PushBack( @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) );
	
	//	gr: just trying to find ONE that works on es2
	//	gr: not formely supported, but we COULD do
	Formats.PushBack( @(kCVPixelFormatType_32ARGB) );
	Formats.PushBack( @(kCVPixelFormatType_32ABGR) );
	Formats.PushBack( @(kCVPixelFormatType_32RGBA) );
	Formats.PushBack( @(kCVPixelFormatType_OneComponent8) );
	Formats.PushBack( @(kCVPixelFormatType_TwoComponent8) );
	Formats.PushBack( @(kCVPixelFormatType_422YpCbCr8_yuvs) );
	Formats.PushBack( @(kCVPixelFormatType_422YpCbCr8FullRange) );
	Formats.PushBack( @(kCVPixelFormatType_30RGB) );
	
	Formats.PushBack( nil );	//	special "pick best" format (nil applies to the whole output settings, not just the format)
	
	for ( int opengl=0;	opengl<=1;	opengl++ )
	{
		bool OpenglCompat = (opengl==0) ? true : false;
		for ( int f=0;	f<Formats.GetSize();	f++ )
		{
			PixelFormats.PushBack( std::make_pair( Formats[f], OpenglCompat ) );
		}
	}
}


std::string GetAVAssetReaderStatusString(AVAssetReaderStatus Status)
{
#define CASE(e)	case (e): return #e
	switch ( Status )
	{
			CASE(AVAssetReaderStatusUnknown);
			CASE(AVAssetReaderStatusReading);
			CASE(AVAssetReaderStatusCompleted);
			CASE(AVAssetReaderStatusFailed);
			CASE(AVAssetReaderStatusCancelled);
		default:
		{
			std::stringstream Err;
			Err << "Unknown AVAssetReaderStatus: " << Status;
			return Err.str();
		}
	}
#undef CASE
}

std::ostream& operator<<(std::ostream &out,const AVAssetReaderStatus& in)
{
	out << GetAVAssetReaderStatusString(in);
	return out;
}



std::shared_ptr<TMediaExtractor> Platform::AllocVideoDecoder(TMediaExtractorParams Params,std::shared_ptr<Opengl::TContext> OpenglContext)
{
	return std::shared_ptr<TMediaExtractor>( new AvfDecoderPlayer( Params, OpenglContext ) );
}



AvfDecoder::AvfDecoder(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext) :
	AvfMediaExtractor	( Params, OpenglContext )
{
}


void AvfDecoder::Shutdown()
{
	//	gr: shutdown the parent (frames) first as the renderer will dangle and then get freed up in a frame
	//	todo: Better link between them so the CFPixelBuffer's never have ownership
	//VideoDecoder::ReleaseFrames();
	
	//	deffer all opengl shutdown to detach the renderer
	PopWorker::DeferDelete( mOpenglContext, mRenderer );
	
	//	no longer need the context
	mOpenglContext.reset();
}
/*
AvfMovieDecoder::AvfMovieDecoder(const TVideoDecoderParams& Params,std::map<size_t,std::shared_ptr<TPixelBufferManager>>& PixelBufferManagers,std::map<size_t,std::shared_ptr<TAudioBufferManager>>& AudioBufferManagers,std::shared_ptr<Opengl::TContext>& OpenglContext) :
	AvfAssetDecoder		( Params, PixelBufferManagers, AudioBufferManagers, OpenglContext )
{
}

AvfMovieDecoder::~AvfMovieDecoder()
{
	//	call parent cleanup first
	Shutdown();

	DeleteReader();
	DeleteAsset();
}

bool AvfMovieDecoder::HasFatalError(std::string& Error)
{
	if ( !mCaughtDecodingException.empty() )
	{
		Error = mCaughtDecodingException;
		return true;
	}
	
	return TVideoDecoder::HasFatalError( Error );
}

void AvfMovieDecoder::GetStreamMeta(ArrayBridge<TStreamMeta>&& StreamMetas)
{
	if ( !mAsset )
		return;
	
	StreamMetas.Copy( mAsset->mStreams );
}
*/

bool AvfAssetDecoder::Iteration()
{
	//	gr: osx 10.11 sdk warns you can't mix obj-c and c++ try/catch (though works fine on IOS 8/9 sdk)
	//		hence awkward split
	@try
	{
		try
		{
			CreateReader();
			WaitForReaderNextFrame();
		}
		catch (std::exception& e)
		{
			DeleteReader();	//	AvfAssetDecoder didnt have this
			std::Debug << "AvfMovieDecoder exception: " << e.what() << std::endl;
			mCaughtDecodingException = e.what();
		}
	}
	@catch (NSException* e)
	{
		std::stringstream Error;
		Error << "AvfMovieDecoder NSException " << Soy::NSErrorToString( e );
		std::Debug << Error.str() << std::endl;
		mCaughtDecodingException = Error.str();
	}
	@catch (...)
	{
		std::Debug << "AvfMovieDecoder unknown exception" << std::endl;
		mCaughtDecodingException = "Unknown exception during decode";
	}
	
	//	don't want to overload a failing system (eg. 404) so sleep, but allow retry (maybe pointless?)
	if ( !mCaughtDecodingException.empty() )
	{
		static int SleepMs = 5000;
		std::this_thread::sleep_for( std::chrono::milliseconds( SleepMs ) );
	}
	
	return true;
}
/*
void AvfMovieDecoder::CreateAsset()
{
	if ( mAsset )
		return;

	auto TrackFilter = [this](const TStreamMeta& Stream)
	{
		if ( !mParams.mExtractAudioStreams )
		{
			if ( SoyMediaFormat::IsAudio( Stream.mCodec ) )
				return false;
		}
		return true;
	};
	
	mAsset.reset( new AvfAsset( mParams.mFilename, TrackFilter ) );
}

void AvfMovieDecoder::CreateReader()
{
	if ( mReader )
		return;

	//	do this first, let it throw if it fails (not pixel-format problem)
	CreateAsset();
	CreateReaderObject();

	//	try all the formats
	Array<std::pair<id,bool>> PixelFormats;
	GetAvailiblePixelFormats( GetArrayBridge( PixelFormats ) );
	
	//	try them all
	for ( int f=0;	f<PixelFormats.GetSize();	f++ )
	{
		id Format = PixelFormats[f].first;
		auto Opengl = PixelFormats[f].second;
		
		//	try and create
		try
		{
			CreateAsset();
			CreateReaderObject();
			if ( !CreateReader( Format, Opengl ) )
				continue;
			if ( !mReader )
				throw Soy::AssertException("No error, but reader not created");
			return;
		}
		catch ( std::exception& e )
		{
			mReader.Release();
			auto FormatName = Avf::GetPixelFormatString( Format );
			std::Debug << "Failed to create reader with format: " << FormatName << " " << (Opengl ? "with opengl" : "without opengl") << std::endl;
			continue;
		}
	}

	//	out of formats
	throw Soy::AssertException("Failed to find compatbile format");
}

bool AvfMovieDecoder::CreateReader(id PixelFormat,bool OpenglCompatibility)
{
	CreateAsset();
	
	//	create a reader
	//	let this throw on fatal error
	CreateReaderObject();
	
	//	create track - catch errors and fail this combo
	try
	{
		CreateVideoTrack( PixelFormat, OpenglCompatibility );
	}
	catch (std::exception& e)
	{
		DeleteReader();
		auto FormatName = Avf::GetPixelFormatString( PixelFormat );
		std::Debug << "Failed to create video track: " << e.what() << " with " << FormatName << " " << (OpenglCompatibility ? "with opengl compatibility" : "without opengl compatibility" ) << std::endl;
		return false;
	}

	//	start reading, if this fails we need to destroy the reader
	//	initial seek
	//	gr: cannot do this once we've started!
	SoyTime InitialSeekTime( GetPlayerTime() + mParams.mPixelBufferParams.mPreSeek );
	std::Debug << "Reader initial seek to " << InitialSeekTime << std::endl;
	auto StartTime = Soy::Platform::GetTime( InitialSeekTime );
	CMTimeRange TimeRange = CMTimeRangeMake( StartTime, kCMTimePositiveInfinity );
	mReader.mObject.timeRange = TimeRange;
	
	//	error with track setup
	if ( [mReader.mObject startReading] == NO )
	{
		auto* StartError = mReader.mObject.error;
		AVAssetReaderStatus Status = mReader.mObject.status;
		std::stringstream Error;
		Error << "Failed to startReading video; " << Soy::NSErrorToString( StartError );
		Error << " Status: " << Status;
		
		//	gr: also need to clean up the reader as now;
		//		it has an output attached
		//		startReading will never succeed again
		DeleteReader();
		DeleteAsset();
		
		std::Debug << Error.str() << std::endl;
		return false;
	}
	
	std::Debug << "Created and started video track: " << Avf::GetPixelFormatString( PixelFormat ) << " " << (OpenglCompatibility ? "with opengl compatibility" : "without opengl compatibility" ) << std::endl;
	

	return true;
}

void AvfMovieDecoder::DeleteReader()
{
	//	stop reader
	if ( mReader )
	{
		mVideoTrackOutputLock.lock();
		[mReader.mObject cancelReading];
		mVideoTrackOutputLock.unlock();
	}
	
	mVideoTrackOutput.Release();
	mReader.Release();
	
	DeleteAsset();
}

void AvfMovieDecoder::DeleteAsset()
{
	if ( mReader )
		DeleteReader();

	mAsset.reset();
}

void AvfMovieDecoder::CreateReaderObject()
{
	if ( mReader )
		return;
	
	if ( !Soy::Assert( mAsset != nullptr, "Asset expected" ) )
		return;
	
	//	create reader
	auto* Asset = mAsset->mAsset.mObject;
	NSError *error = nil;
	ObjcPtr<AVAssetReader> pReader( [AVAssetReader assetReaderWithAsset:Asset error:&error] );

	if ( !pReader )
	{
		std::stringstream Error;
		Error << "Failed to allocate reader: " << Soy::NSErrorToString(error);
		throw Soy::AssertException( Error.str() );
	}
	
	//auto* Reader = pReader.mObject;
	mReader.Retain( pReader );

	//	gr: why does this need an extra retain?
#if !defined(ARC_ENABLED)
	[mReader.mObject retain];
#endif
}



void AvfMovieDecoder::CreateVideoTrack(id PixelFormat,bool OpenglCompatibility)
{
	if ( mVideoTrackOutput )
		return;
	if ( !Soy::Assert( mAsset != nullptr, "Asset expected" ) )
		return;
	if ( !Soy::Assert( mReader != nullptr, "Reader expected" ) )
		return;
	
	auto* Reader = mReader.mObject;
	auto* VideoTrack = mVideoTrack.mObject;

	//	create track output
	BOOL OpenglCompatibilityObj = OpenglCompatibility ? YES : NO;
		
	//	configure output
	NSMutableDictionary *VideoOutputSettings = nil;
		
	if ( PixelFormat != nil )
	{
		VideoOutputSettings = [NSMutableDictionary dictionary];
		[VideoOutputSettings setObject:PixelFormat forKey:(id)kCVPixelBufferPixelFormatTypeKey];
		
		//	ensure GLES compatibilibty for fast copy...
	#if defined(TARGET_IOS)
		[VideoOutputSettings setObject:[NSNumber numberWithBool:OpenglCompatibilityObj] forKey:(id)kCVPixelBufferOpenGLESCompatibilityKey];
	#elif defined(TARGET_OSX)
		//	gr: is this causing an issue with ES 2?
		[VideoOutputSettings setObject:[NSNumber numberWithBool:OpenglCompatibilityObj] forKey:(id)kCVPixelBufferOpenGLCompatibilityKey];
	#endif
		//[VideoOutputSettings setObject:[NSNumber numberWithBool:YES] forKey:(id)kCVPixelBufferAvfurfacePropertiesKey];
	}
		
	AVAssetReaderTrackOutput* VideoTrackOutput = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:VideoTrack outputSettings:VideoOutputSettings];
	if ( !VideoTrackOutput )
		throw Soy::AssertException("Failed to create AVAssetReaderTrackOutput");
	
	//	turn off copy-sample-data if possible
	@try
	{
		if( [VideoTrackOutput respondsToSelector:@selector(alwaysCopiesSampleData)] )
			VideoTrackOutput.alwaysCopiesSampleData = (mParams.mAvfCopyBuffer) ? YES : NO;
	}
	@catch(NSException* e)
	{
		std::stringstream Error;
		Error << "Failed to set alwaysCopiesSampleData; " << Soy::NSErrorToString( e ) << std::endl;
		throw Soy::AssertException( Error.str() );
	}

	if ( ![Reader canAddOutput:VideoTrackOutput] )
		throw Soy::AssertException("cannot add output");

	[Reader addOutput:VideoTrackOutput];
	mVideoTrackOutput.Retain( VideoTrackOutput );

	//	debug the format that DID come out if we passed pick-any format
	if ( PixelFormat == nil )
	{
		auto UsedOutputSettings = mVideoTrackOutput.mObject.outputSettings;
		if ( UsedOutputSettings )
		{
			std::stringstream OutputSettingsStr;
			for( NSString* Key in UsedOutputSettings )
			{
				OutputSettingsStr << Soy::NSStringToString( Key ) << "=";
				
				@try
				{
					NSString* Value = [[UsedOutputSettings objectForKey:Key] description];
					OutputSettingsStr << Soy::NSStringToString( Value );
				}
				@catch (NSException* e)
				{
					OutputSettingsStr << "<unkown value " << Soy::NSErrorToString( e );
				}
				OutputSettingsStr<< ", ";
			}
			std::Debug << "AVAssetReader used settings: " << OutputSettingsStr.str() << std::endl;
		}
		else
		{
			std::Debug << "Warning: cannot find VideoTrackOutput output settings" << std::endl;
		}
	}
	
	
}



bool AvfMovieDecoder::WaitForReaderNextFrame()
{
	if ( !mReader )
		return false;

	auto* Reader = mReader.mObject;
	
	//	grab next sample, if this fails, fall through and do more handling
	//	when copyNextSample fails, status updates to error or completed
	if ( Reader.status == AVAssetReaderStatusReading )
	{
		mVideoTrackOutputLock.lock();
		CMSampleBufferRef sampleBufferRef = [mVideoTrackOutput.mObject copyNextSampleBuffer];
		mVideoTrackOutputLock.unlock();
		
		//	gr: fix this if we keep this class
		size_t StreamIndex = 0;
		
		OnSampleBuffer( sampleBufferRef, StreamIndex, false );
		return true;
	}
	
	switch ( Reader.status )
	{
			//	not started
		case AVAssetReaderStatusUnknown:
			//	more data to come
		case AVAssetReaderStatusReading:
			return true;

		//	this was last frame
		case AVAssetReaderStatusCompleted:
			return true;

		case AVAssetReaderStatusFailed:
		{
			std::stringstream Error;
			Error << "Reader Status=Failed: " << Soy::NSErrorToString(Reader.error);
			throw Soy::AssertException( Error.str() );
			return false;
		}
			
		case AVAssetReaderStatusCancelled:
		default:
			return false;
	}
}


bool AvfMovieDecoder::CopyBuffer(CMSampleBufferRef Buffer,std::stringstream& Error)
{
	//	gr: same as popcapture's sample handler;
	//	void AVCaptureSessionWrapper::handleSampleBuffer(CMSampleBufferRef sampleBuffer)

	CMTime currentSampleTime = CMSampleBufferGetOutputPresentationTimeStamp(Buffer);
	CVImageBufferRef movieFrame = CMSampleBufferGetImageBuffer(Buffer);
	
	auto Height = CVPixelBufferGetHeight(movieFrame);
	auto Width = CVPixelBufferGetWidth(movieFrame);
	
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	
	// Upload to texture
	bool LockBuffer = !mParams.mCopyBuffer;
	if ( LockBuffer && CVPixelBufferLockBaseAddress(movieFrame, 0) != kCVReturnSuccess )
	{
		Error << "failed to lock new sample buffer";
		return false;
	}
	auto* Pixels = static_cast<char*>( CVPixelBufferGetBaseAddress(movieFrame) );
	if ( Pixels )
	{
		auto DataSize = CVPixelBufferGetDataSize(movieFrame);
		auto rowSize = CVPixelBufferGetBytesPerRow(movieFrame);
		
		mBufferMeta.mChannels = rowSize / Width;
		mBufferMeta.mWidth = Width;
		mBufferMeta.mHeight = Height;
		mBuffer.resize( DataSize );
		memcpy( mBuffer.data(), Pixels, DataSize );
		mBufferChanged = true;
	}
	else
	{
		Error << "locked pixels null";
	}
	if ( LockBuffer )
		CVPixelBufferUnlockBaseAddress(movieFrame, 0);

	return true;
}
*/

/*
SoyPixelsMeta CFPixelBuffer::GetMeta()
{
	auto& Buffer = mSample;
	CVImageBufferRef movieFrame = CMSampleBufferGetImageBuffer(Buffer);

	auto Height = CVPixelBufferGetHeight(movieFrame);
	auto Width = CVPixelBufferGetWidth(movieFrame);
	auto rowSize = CVPixelBufferGetBytesPerRow(movieFrame);
	
	SoyPixelsMeta Meta;
	Meta.mChannels = rowSize / Width;
	Meta.mWidth = Width;
	Meta.mHeight = Height;
	return Meta;
}
*/







AvfDecoderPlayer::AvfDecoderPlayer(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext) :
	AvfAssetDecoder		( Params, OpenglContext )
{
	
}

AvfDecoderPlayer::~AvfDecoderPlayer()
{
	
}


void AvfDecoderPlayer::CreateReader()
{
	if ( mPlayer )
		return;
	
	//	alloc asset
	auto Url = ::Platform::GetUrl( mParams.mFilename );
	mPlayerItem.Retain( [AVPlayerItem playerItemWithURL:Url] );
	AVAsset* Asset = [mPlayerItem.mObject asset];
	
	//[self addDidPlayToEndTimeNotificationForPlayerItem:item];
	
	//	make player
	mPlayer.Retain( [[AVPlayer alloc]initWithPlayerItem:mPlayerItem.mObject] );
	
	//	add video output
	//NSDictionary *pixBuffAttributes = @{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)};
	NSDictionary *pixBuffAttributes = @{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) };
	mPlayerVideoOutput.Retain( [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:pixBuffAttributes] );

	//	add the output sink
	[mPlayerItem.mObject addOutput:mPlayerVideoOutput.mObject];

	//	stop auto-playing of video & audio
	//	gr: has no effect, audio still playing in background
	mPlayerVideoOutput.mObject.suppressesPlayerRendering = YES;

	# define ONE_FRAME_DURATION 0.03
	[mPlayerVideoOutput.mObject requestNotificationOfMediaDataChangeWithAdvanceInterval:ONE_FRAME_DURATION];
	

	//	create delegate
	mPlayerDelegate.Retain( [[AvfPlayerDelegate alloc] initWithParent:this] );

	//	todo: make a queue to move work off main thread
	static auto _myVideoOutputQueue = dispatch_queue_create("myVideoOutputQueue", DISPATCH_QUEUE_SERIAL);
	auto Queue = _myVideoOutputQueue;
	[mPlayerVideoOutput.mObject setDelegate:mPlayerDelegate.mObject queue:Queue];
	
	
	//	wait for loading to finish - throws on error
	WaitForLoad();
	
	//	ready to load, refresh meta
	mDuration = Soy::Platform::GetTime( mPlayerItem.mObject.duration );

	
	//	attempt to start
	[mPlayer.mObject play];
	
	//	initialise the seek
	//SoyTime InitialSeekTime( GetPlayerTime().mTime + mParams.mPixelBufferParams.mPreSeek.mTime );
	SoyTime InitialSeekTime( mParams.mInitialTime );
	std::Debug << "Reader initial seek to " << InitialSeekTime << std::endl;
	auto StartTime = Soy::Platform::GetTime( InitialSeekTime );

	__block AvfDecoderPlayer& This = *this;
	__block SoyTime SeekedTime = Soy::Platform::GetTime(StartTime);
	auto OnSeekCompleted = ^(BOOL Finished)
	{
		This.OnSeekCompleted( SeekedTime, Finished );
	};

	
	static bool DoSeek = false;
	if ( DoSeek )
	{
		[mPlayer.mObject seekToTime:StartTime completionHandler:OnSeekCompleted];
	}

}

void AvfDecoderPlayer::DeleteReader()
{
	
}



void AvfDecoderPlayer::WaitForLoad()
{
	Soy::Assert( mPlayerItem, "Player item expected" );
	Soy::Assert( mPlayerDelegate, "Player delegate expected" );

	auto* PlayerItem = mPlayerItem.mObject;
	auto* PlayerDelegate = mPlayerDelegate.mObject;
	
	//	add an observer to delegate, then wait for it to be triggered
	Soy::TSemaphore Semaphore;
	
	//	use delegate to observe load to finish
	[PlayerItem addObserver:PlayerDelegate forKeyPath:@"status" options:0 context:&Semaphore];
	auto DeleteObserver = [&PlayerItem,&PlayerDelegate,&Semaphore]
	{
		//	changed, remove observer
		[PlayerItem removeObserver:PlayerDelegate forKeyPath:@"status" context:&Semaphore];
	};

	try
	{
		Semaphore.Wait();
		DeleteObserver();
	}
	catch (std::exception& e)
	{
		DeleteObserver();
		throw;
	}
	
	
	//	evaluate new status and throw if we didn't succeed
	auto Status = [mPlayer.mObject status];
	switch ( Status )
	{
		case AVPlayerStatusReadyToPlay:
			//	hurrah!
			return;
			
		default:
		case AVPlayerStatusUnknown:
			throw Soy::AssertException("Player status is unknown");
			
		case AVPlayerStatusFailed:
		{
			auto Err = [mPlayer.mObject error];
			std::stringstream Error;
			Error << "Player failed to setup: " << Soy::NSErrorToString(Err);
			throw Soy::AssertException( Error.str() );
		}
	};

}


SoyTime AvfDecoderPlayer::GetCurrentTime()
{
	if ( !mPlayer )
		return SoyTime();
	
	auto Time = Soy::Platform::GetTime( mPlayer.mObject.currentTime );
	
	return Time;
}

std::shared_ptr<TMediaPacket> AvfDecoderPlayer::ReadNextPacket()
{
	throw Soy::AssertException("Implement this");
}

bool AvfDecoderPlayer::WaitForReaderNextFrame()
{
	Soy::Assert( mPlayer!=nullptr, "Expected video player" );
	Soy::Assert( mPlayerVideoOutput!=nullptr, "Expected video player output" );
	

	//auto RequestTime = Platform::GetTimeInterval(mPlayerTime);
	auto RequestTime = 0;//CACurrentMediaTime();	//	quartz!
	//auto RequestTime = Platform::GetTimeInterval( Platform::GetTime( mPlayer.mObject.currentTime ) );
	
	//	resolve the nearest frame to the time we want
	auto FrameTime = [mPlayerVideoOutput.mObject itemTimeForHostTime:RequestTime];
	//FrameTime = Platform::GetTime( mPlayerTime );
	

	//	do we have a pixel buffer for that time?
	if ( ![mPlayerVideoOutput.mObject hasNewPixelBufferForItemTime:FrameTime] )
	{
		static int SleepMs = 1;
		/*
		static bool DebugNoMatch = false;
		if ( DebugNoMatch )
			std::Debug << "No matching frame. Current time: " << GetCurrentTime() << " requesting " << Soy::Platform::GetTime(RequestTime) << "/" << GetPlayerTime() << " got " << Soy::Platform::GetTime(FrameTime) << std::endl;
		*/
		std::this_thread::sleep_for( std::chrono::milliseconds(SleepMs) );
		return false;
	}
	
	//	grab pixel buffer
	CVPixelBufferRef PixelBuffer = [mPlayerVideoOutput.mObject copyPixelBufferForItemTime:FrameTime itemTimeForDisplay:nil];
	if ( !PixelBuffer )
	{
		std::Debug << "Failed to get pixel buffer for " << RequestTime << " (" << Soy::Platform::GetTime(FrameTime) << ")" << std::endl;
		return false;
	}
	
	size_t StreamIndex = 0;
	//std::Debug << "Got pixel buffer; Current time: " << GetCurrentTime() << " requesting " << Soy::Platform::GetTime(RequestTime) << "/" << GetPlayerTime() << " got " << Soy::Platform::GetTime(FrameTime) << std::endl;
	static bool Retain = true;
	OnSampleBuffer( PixelBuffer, Soy::Platform::GetTime(FrameTime), StreamIndex, Retain );
	return true;
}



void AvfDecoderPlayer::OnSeekCompleted(SoyTime RequestedSeekTime,bool Finished)
{
	std::Debug << "Seek to " << RequestedSeekTime << " finished(" << (Finished?"true":"false") << " current time now " << GetCurrentTime() << std::endl;
	Wake();
}



std::shared_ptr<Platform::TMediaFormat>	GetStreamFormat(size_t StreamIndex)
{
	throw Soy::AssertException("implement me");
}


CVPixelBuffer::~CVPixelBuffer()
{
	//	release
	if ( mSample )
	{
		//auto RetainCount = CFGetRetainCount( mSample.mObject );
		CVBufferRelease( mSample.mObject );
		mSample.Release();
	}
}


CVImageBufferRef CVPixelBuffer::LockImageBuffer()
{
	return mSample.mObject;
}

void CVPixelBuffer::UnlockImageBuffer()
{
}


@implementation AvfPlayerDelegate

- (id)initWithParent:(AvfDecoderPlayer*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
	}
	return self;
}

- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)sender
{
	std::Debug << "outputMediaDataWillChange" << std::endl;
}

- (void)outputSequenceWasFlushed:(AVPlayerItemOutput *)output
{
	std::Debug << "outputSequenceWasFlushed" << std::endl;
}


- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
	//	is this the status changed we're looking for?
	if ([keyPath isEqualToString:@"status"])
	{
		//	context is has-changed-bool
		Soy::Assert( context != nullptr, "Expected context");
		Soy::TSemaphore& Semaphore = *reinterpret_cast<Soy::TSemaphore*>(context);
		Semaphore.OnCompleted();
	}
}

@end
