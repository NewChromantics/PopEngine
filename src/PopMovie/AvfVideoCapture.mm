#include "AvfVideoCapture.h"
#import <CoreVideo/CoreVideo.h>
#import <AVFoundation/AVFoundation.h>
//#import <Accelerate/Accelerate.h>
#include "SoyDebug.h"
#include "SoyScope.h"
#include "SoyString.h"
#include "SortArray.h"
#include "RemoteArray.h"
#include "SoyPixels.h"
#include "AvfPixelBuffer.h"
#include "SoyAvf.h"
#include "SoyOpenglContext.h"

namespace AvfCapture
{
	class TDeviceMeta;
	void	EnumDevices(ArrayBridge<TDeviceMeta>&& Devices,std::function<bool(const TDeviceMeta&)> Filter=nullptr);
}

class AvfCapture::TDeviceMeta
{
public:
	TDeviceMeta() :
		mDevice	( nullptr )
	{
	}
	TDeviceMeta(AVCaptureDevice& Device);
	
	AVCaptureDevice*	mDevice = nullptr;	//	gr: not sure of the lifetime of this :/
	std::string			mName;
	std::string			mSerial;
	std::string			mVendor;
	bool				mIsConnected = false;
	bool				mIsSuspended = false;
	bool				mHasAudio = false;
	bool				mHasVideo = false;
};


AvfCapture::TDeviceMeta::TDeviceMeta(AVCaptureDevice& Device) :
	mDevice	( &Device )
{
	mName = Soy::NSStringToString( [&Device localizedName] );
	mSerial = Soy::NSStringToString( [&Device uniqueID] );
#if defined(TARGET_IOS)
	mVendor = "ios";
#else
	mVendor = Soy::NSStringToString( [&Device manufacturer] );
#endif
	mIsConnected = YES == [mDevice isConnected];
	mHasVideo = YES == [mDevice hasMediaType:AVMediaTypeVideo];
	mHasAudio = YES == [mDevice hasMediaType:AVMediaTypeAudio];
	
	//	asleep, eg. macbook camera when lid is down
#if defined(TARGET_IOS)
	mIsSuspended = false;
#else
	mIsSuspended = false;
	try
	{
		mIsSuspended = YES == [mDevice isSuspended];
	}
	catch(...)
	{
		//	unsupported method on this device. case grahams: DK2 Left&Right webcam throws exception here
	}
#endif
}

/*
TVideoDeviceMeta GetDeviceMeta(AVCaptureDevice* Device)
{
	//	gr: allow this for failed-to-init devices
	if ( !Device )
//	if ( !Soy::Assert( Device, "Device expected") )
		return TVideoDeviceMeta();
	
	TVideoDeviceMeta Meta;
	Meta.mName = std::string([[Device localizedName] UTF8String]);
	Meta.mSerial = std::string([[Device uniqueID] UTF8String]);
	Meta.mVendor = std::string([[Device manufacturer] UTF8String]);
	Meta.mConnected = YES == [Device isConnected];
	Meta.mVideo = YES == [Device hasMediaType:AVMediaTypeVideo];
	Meta.mAudio = YES == [Device hasMediaType:AVMediaTypeAudio];
	Meta.mText = YES == [Device hasMediaType:AVMediaTypeText];
	Meta.mClosedCaption = YES == [Device hasMediaType:AVMediaTypeClosedCaption];
	Meta.mSubtitle = YES == [Device hasMediaType:AVMediaTypeSubtitle];
	Meta.mTimecode = YES == [Device hasMediaType:AVMediaTypeTimecode];
	//		Meta.mTimedMetadata = YES == [Device hasMediaType:AVMediaTypeTimedMetadata];
	Meta.mMetadata = YES == [Device hasMediaType:AVMediaTypeMetadata];
	Meta.mMuxed = YES == [Device hasMediaType:AVMediaTypeMuxed];
	
	return Meta;
}
*/

@class VideoCaptureProxy;




@interface VideoCaptureProxy : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
	AvfVideoCapture*	mParent;
	size_t				mStreamIndex;
}

- (id)initWithVideoCapturePrivate:(AvfVideoCapture*)parent;

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
- (void)captureOutput:(AVCaptureOutput *)captureOutput didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;

- (void)onVideoError:(NSNotification *)notification;

@end



@implementation VideoCaptureProxy

- (void)onVideoError:(NSNotification *)notification
{
	//	gr: handle this properly - got it when disconnecting USB hub
	/*
	Exception Name: NSInvalidArgumentException
	Description: -[NSError UTF8String]: unrecognized selector sent to instance 0x618000847350
	User Info: (null)
	*/
	try
	{
		NSString* Error = notification.userInfo[AVCaptureSessionErrorKey];
		auto ErrorStr = Soy::NSStringToString( Error );
		std::Debug << "Video error: "  << ErrorStr << std::endl;
	}
	catch(...)
	{
		std::Debug << "Some video error" << std::endl;
	}
}

- (id)initWithVideoCapturePrivate:(AvfVideoCapture*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
		mStreamIndex = 0;
	}
	return self;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
	static bool DoRetain = true;
	mParent->OnSampleBuffer( sampleBuffer, mStreamIndex, DoRetain );
}


- (void)captureOutput:(AVCaptureOutput *)captureOutput didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
	std::Debug << "dropped sample" << std::endl;
}

@end


/*
void AvfVideoCapture::GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas)
{
	NSArray* Devices = [AVCaptureDevice devices];
	
	for (id Device in Devices)
	{
		Metas.PushBack( GetDeviceMeta( Device ));
	}

}
 */


std::shared_ptr<TMediaExtractor> Platform::AllocCaptureExtractor(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext)
{
	return std::make_shared<AvfVideoCapture>( Params, OpenglContext );
}

void Platform::EnumCaptureDevices(std::function<void(const std::string&)> Append)
{
	Array<AvfCapture::TDeviceMeta> Metas;
	EnumDevices( GetArrayBridge( Metas ) );
	for ( int i=0;	i<Metas.GetSize();	i++ )
	{
		//	gr: this is friendly, but I need to use serial if it's not unique :/
		//	maybe append both?
		Append( Metas[i].mName );
	}
}

void AvfCapture::EnumDevices(ArrayBridge<TDeviceMeta>&& DeviceMetas,std::function<bool(const TDeviceMeta&)> Filter)
{
	if ( Filter == nullptr )
	{
		Filter = [](const TDeviceMeta& Meta)
		{
			return Meta.mHasVideo;
		};
	}
	
	NSArray* Devices = [AVCaptureDevice devices];
	for ( AVCaptureDevice* Device in Devices )
	{
		if ( !Device )
			continue;
		TDeviceMeta Meta( *Device );
		
		if ( !Filter(Meta) )
			continue;
		
		DeviceMetas.PushBack( Meta );
	}
}


AvfVideoCapture::AvfVideoCapture(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext) :
	AvfMediaExtractor		( Params, OpenglContext ),
	mQueue					( nullptr ),
	mDiscardOldFrames		( Params.mDiscardOldFrames ),
	mForceNonPlanarOutput	( Params.mForceNonPlanarOutput )
{
	bool KeepOldFrames = !mDiscardOldFrames;
	Run( Params.mFilename, TVideoQuality::High, KeepOldFrames );
	StartStream();
}

AvfVideoCapture::~AvfVideoCapture()
{
	//	stop avf
	StopStream();
	
	//	wait for everything to release
	TMediaExtractor::WaitToFinish();
	
	//	call parent to clear existing data
	Shutdown();

	//	release queue and proxy from output
	if ( mOutput )
	{
		//[mOutput setSampleBufferDelegate:mProxy queue: nil];
		[mOutput setSampleBufferDelegate:nil queue: nil];
		mOutput.Release();
	}
	
	//	delete queue
	if ( mQueue )
	{
		dispatch_release( mQueue );
		mQueue = nullptr;
	}
	
	mSession.Release();
	mProxy.Release();
	
	
}



void AvfVideoCapture::Shutdown()
{
	//	gr: shutdown the parent (frames) first as the renderer will dangle and then get freed up in a frame
	//	todo: Better link between them so the CFPixelBuffer's never have ownership
	{
		//ReleaseFrames();
		std::lock_guard<std::mutex> Lock( mPacketQueueLock );
		mPacketQueue.Clear();
	}
	
	//	deffer all opengl shutdown to detach the renderer
	PopWorker::DeferDelete( mOpenglContext, mRenderer );
	
	//	no longer need the context
	mOpenglContext.reset();
}



NSString* GetAVCaptureSessionQuality(TVideoQuality::Type Quality)
{
	switch ( Quality )
	{
		case TVideoQuality::Low:
			return AVCaptureSessionPresetLow;
			
		case TVideoQuality::Medium:
			return AVCaptureSessionPresetMedium;
			
		case TVideoQuality::High:
			return AVCaptureSessionPresetHigh;
			
		default:
			Soy::Assert( false, "Unhandled TVideoQuality Type" );
			return AVCaptureSessionPresetHigh;
	}
}



//	gr: make this generic across platforms!
class TSerialMatch
{
public:
	TSerialMatch() :
		mSerial				( "<invalid TSerialMatch>" ),
		mNameExact			( false ),
		mNameStarts			( false ),
		mNameContains		( false ),
		mVendorExact		( false ),
		mVendorStarts		( false ),
		mVendorContains		( false ),
		mSerialExact		( false ),
		mSerialStarts		( false ),
		mSerialContains		( false )
	{
	}
	
	TSerialMatch(const std::string& PartSerial,const std::string& Serial,const std::string& Name,const std::string& Vendor) :
		mSerial				( Serial ),
		mNameExact			( Soy::StringMatches( Name, PartSerial, false ) ),
		mNameStarts			( Soy::StringBeginsWith( Name, PartSerial, false ) ),
		mNameContains		( Soy::StringContains( Name, PartSerial, false ) ),
		mVendorExact		( Soy::StringMatches( Vendor, PartSerial, false ) ),
		mVendorStarts		( Soy::StringBeginsWith( Vendor, PartSerial, false ) ),
		mVendorContains		( Soy::StringContains( Vendor, PartSerial, false ) ),
		mSerialExact		( Soy::StringMatches( Serial, PartSerial, false ) ),
		mSerialStarts		( Soy::StringBeginsWith( Serial, PartSerial, false ) ),
		mSerialContains		( Soy::StringContains( Serial, PartSerial, false ) )
	{
		//	allow matching all serials
		if ( PartSerial == "*" )
			mNameContains = true;
	}
	
	size_t		GetScore()
	{
		size_t Score = 0;
		//	shift = priority
		Score |= mSerialExact << 9;
		Score |= mNameExact << 8;

		Score |= mSerialStarts << 7;
		Score |= mNameStarts << 6;
		Score |= mVendorExact << 5;
	
		Score |= mSerialContains << 4;
		Score |= mNameContains << 3;
		Score |= mVendorStarts << 2;
		Score |= mVendorContains << 1;
		return Score;
	}

	std::string	mSerial;
	bool		mNameExact;
	bool		mNameStarts;
	bool		mNameContains;
	bool		mVendorExact;
	bool		mVendorStarts;
	bool		mVendorContains;
	bool		mSerialExact;
	bool		mSerialStarts;
	bool		mSerialContains;
};


std::string FindFullSerial(const std::string& SerialNeedle,ArrayBridge<AvfCapture::TDeviceMeta>&& Devices)
{
	TSerialMatch BestMatch;
	std::stringstream AllDeviceNames;
	
	//	store rejection reasons in case we get no results, but some matches
	std::stringstream MatchRejection;
	
	for ( int i=0;	i<Devices.GetSize();	i++ )
	{
		auto& Meta = Devices[i];
		auto& Name = Meta.mName;
		auto& Serial = Meta.mSerial;
		auto& Vendor = Meta.mVendor;
		auto& Device = Meta.mDevice;
		
		AllDeviceNames << Name << "/" << Serial << " ";
		
		TSerialMatch Match( SerialNeedle, Serial, Name, Vendor );
		
		//	not a match
		if ( Match.GetScore() == 0 )
			continue;

		if ( !Meta.mIsConnected )
		{
			MatchRejection << Name << "/" << Serial << " not connected. ";
			continue;
		}
		if ( Meta.mIsSuspended )
		{
			MatchRejection << Name << "/" << Serial << " is suspended. ";
			continue;
		}
		
		//	already have better
		if ( Match.GetScore() < BestMatch.GetScore() )
			continue;
		BestMatch = Match;
	}
	
	if ( BestMatch.GetScore() == 0 )
	{
		std::stringstream Error;
		Error << "Failed to find matching serial from " << SerialNeedle << ". ";
		Error << MatchRejection.str();
		Error << " All devices: " << AllDeviceNames.str();
		throw Soy::AssertException( Error.str() );
	}
	
	return BestMatch.mSerial;
}


void AvfVideoCapture::Run(const std::string& Serial,TVideoQuality::Type DesiredQuality,bool KeepOldFrames)
{
	Array<AvfCapture::TDeviceMeta> DeviceMetas;
	
	auto Filter = [&](const AvfCapture::TDeviceMeta& Meta)
	{
		if ( !Meta.mHasVideo )
			return false;
		return true;
	};
	
	EnumDevices( GetArrayBridge(DeviceMetas), Filter );
	if ( DeviceMetas.IsEmpty() )
		throw Soy::AssertException("No AVCapture devices found");
	
	auto FullSerial = FindFullSerial( Serial, GetArrayBridge(DeviceMetas) );
	mProxy.Retain( [[VideoCaptureProxy alloc] initWithVideoCapturePrivate:this] );
	mSession.Retain( [[AVCaptureSession alloc] init] );
	auto& Session = mSession.mObject;
	auto& Proxy = mProxy.mObject;
	
	static bool MarkBeginConfig = false;
	if ( MarkBeginConfig )
		[Session beginConfiguration];

	//	try all the qualitys
	Array<TVideoQuality::Type> Qualitys;
	Qualitys.PushBack( DesiredQuality );
	Qualitys.PushBack( TVideoQuality::Low );
	Qualitys.PushBack( TVideoQuality::Medium );
	Qualitys.PushBack( TVideoQuality::High );
	
	for ( int i=0;	i<Qualitys.GetSize();	i++ )
	{
		auto Quality = Qualitys[i];
		auto QualityString = GetAVCaptureSessionQuality(Quality);
		
		if ( ![mSession canSetSessionPreset:QualityString] )
			continue;

		Session.sessionPreset = QualityString;
		break;
	}

	if ( !Session.sessionPreset )
		throw Soy::AssertException("Failed to set session quality");
	
	NSError* error = nil;
    
    // Find a suitable AVCaptureDevice
	//NSString* SerialString = [NSString stringWithCString:FullSerial.c_str() encoding:[NSString defaultCStringEncoding]];
	auto SerialString = Soy::StringToNSString( FullSerial );
	mDevice.Retain( [AVCaptureDevice deviceWithUniqueID:SerialString] );
	if ( !mDevice )
	{
		std::stringstream Error;
		Error << "Failed to get AVCapture Device with serial " << FullSerial;
		throw Soy::AssertException( Error.str() );
	}
	auto& Device = mDevice.mObject;
	
	AVCaptureDeviceInput* _input = [AVCaptureDeviceInput deviceInputWithDevice:Device error:&error];
	if ( !_input || ![Session canAddInput:_input])
	{
		throw Soy::AssertException("Cannot add AVCaptureDeviceInput");
	}
	[Session addInput:_input];
	
	mOutput.Retain( [[AVCaptureVideoDataOutput alloc] init] );
	auto& Output = mOutput.mObject;
	
	//	loop through formats for ones we can handle that are accepted
	//	https://developer.apple.com/library/mac/documentation/AVFoundation/Reference/AVCaptureVideoDataOutput_Class/#//apple_ref/occ/instp/AVCaptureVideoDataOutput/availableVideoCVPixelFormatTypes
	//	The first format in the returned list is the most efficient output format.
	Array<OSType> TryPixelFormats;
	{
		NSArray* AvailibleFormats = [Output availableVideoCVPixelFormatTypes];
		Soy::Assert( AvailibleFormats != nullptr, "availableVideoCVPixelFormatTypes returned null array" );

		//	filter pixel formats
		std::stringstream Debug;
		Debug << "Device " << FullSerial << " supports x" << AvailibleFormats.count << " formats: ";
	
		for (NSNumber* FormatCv in AvailibleFormats)
		{
			auto FormatInt = [FormatCv integerValue];
			OSType Format = static_cast<OSType>( FormatInt );

			auto FormatSoy = Avf::GetPixelFormat( Format );
			
			Debug << Avf::GetPixelFormatString( FormatCv ) << '/' << FormatSoy;
			
			if ( FormatSoy == SoyPixelsFormat::Invalid )
			{
				Debug << "(Not supported by soy), ";
				continue;
			}

			//	don't allow YUV formats
			if ( mForceNonPlanarOutput )
			{
				Array<SoyPixelsFormat::Type> Planes;
				SoyPixelsFormat::GetFormatPlanes( FormatSoy, GetArrayBridge(Planes) );
				if ( Planes.GetSize() > 1 )
				{
					Debug << "(Skipped due to planar format), ";
					continue;
				}
			}
			
			Debug << ", ";
			TryPixelFormats.PushBack( Format );
		}
		std::Debug << Debug.str() << std::endl;
	}

	bool AddedOutput = false;
	for ( int i=0;	i<TryPixelFormats.GetSize();	i++ )
	{
		OSType Format = TryPixelFormats[i];

		auto PixelFormat = Avf::GetPixelFormat( Format );
		
		//	should have alreayd filtered this
		if ( PixelFormat == SoyPixelsFormat::Invalid )
			continue;

		Output.alwaysDiscardsLateVideoFrames = KeepOldFrames ? NO : YES;
		Output.videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
								 [NSNumber numberWithInt:Format], kCVPixelBufferPixelFormatTypeKey, nil];
		if ( ![Session canAddOutput:Output] )
		{
			std::Debug << "Device " << FullSerial << " does not support pixel format " << PixelFormat << " (despite claiming it does)" << std::endl;
			continue;
		}
		
		//	compatible, add
		[Session addOutput:Output];
		std::Debug << "Device " << FullSerial << " added " << PixelFormat << " output" << std::endl;
		AddedOutput = true;
		break;
	}

	if ( !AddedOutput )
		throw Soy::AssertException("Could not find compatible pixel format");

	//	register for notifications from errors
	NSNotificationCenter *notify = [NSNotificationCenter defaultCenter];
	[notify addObserver: Proxy
			   selector: @selector(onVideoError:)
				   name: AVCaptureSessionRuntimeErrorNotification
				 object: Session];

	//	create a queue to handle callbacks
	//	gr: can this be used for the movie decoder? should this use a thread like the movie decoder?
	//	make our own queue, not the main queue
	//[_output setSampleBufferDelegate:_proxy queue:dispatch_get_main_queue()];
	if ( !mQueue )
		mQueue = dispatch_queue_create("camera_queue", NULL);
	
	if ( MarkBeginConfig )
		[Session commitConfiguration];
	
	[Output setSampleBufferDelegate:Proxy queue: mQueue];
}

void AvfVideoCapture::StartStream()
{
	if ( !mSession )
	{
		std::Debug << "Warning: tried to " << __func__ << " with no session" << std::endl;
		return;
	}

	auto& Session = mSession.mObject;
	if ( !Session.running )
		[ Session startRunning];
	
	bool IsRunning = Session.running;
	Soy::Assert( IsRunning, "Failed tostart running") ;
}

void AvfVideoCapture::StopStream()
{
	if ( !mSession )
		return;

	auto& Session = mSession.mObject;
	if ( Session.running)
		[Session stopRunning];
}


void AvfMediaExtractor::GetStreams(ArrayBridge<TStreamMeta>&& StreamMetas)
{
	for ( auto& Meta : mStreamMeta )
	{
		StreamMetas.PushBack( Meta.second );
	}
}


TStreamMeta AvfMediaExtractor::GetFrameMeta(CMSampleBufferRef SampleBuffer,size_t StreamIndex)
{
	auto Desc = CMSampleBufferGetFormatDescription( SampleBuffer );
	auto Meta = Avf::GetStreamMeta( Desc );
	Meta.mStreamIndex = StreamIndex;
	
	//CMTime CmTimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBufferRef);
	//SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
	
	//	new stream!
	if ( mStreamMeta.find(StreamIndex) == mStreamMeta.end() )
	{
		try
		{
			mStreamMeta[StreamIndex] = Meta;
			OnStreamsChanged();
		}
		catch(...)
		{
			mStreamMeta.erase( mStreamMeta.find(StreamIndex) );
			throw;
		}
	}
	
	return Meta;
}


TStreamMeta AvfMediaExtractor::GetFrameMeta(CVPixelBufferRef SampleBuffer,size_t StreamIndex)
{
	TStreamMeta Meta;
	Meta.mStreamIndex = StreamIndex;
	
	//CMTime CmTimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBufferRef);
	//SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
	
	//	new stream!
	if ( mStreamMeta.find(StreamIndex) == mStreamMeta.end() )
	{
		try
		{
			mStreamMeta[StreamIndex] = Meta;
			OnStreamsChanged();
		}
		catch(...)
		{
			mStreamMeta.erase( mStreamMeta.find(StreamIndex) );
			throw;
		}
	}
	
	return Meta;
}


void AvfMediaExtractor::QueuePacket(std::shared_ptr<TMediaPacket>& Packet)
{
	OnPacketExtracted( Packet->mTimecode, Packet->mMeta.mStreamIndex );
	
	{
		std::lock_guard<std::mutex> Lock( mPacketQueueLock );
	
		//	only save latest
		//	gr: check in case this causes too much stuttering and maybe keep 2
		if ( mParams.mDiscardOldFrames )
			mPacketQueue.Clear();

		mPacketQueue.PushBack( Packet );
	}
	
	//	wake up the extractor as we want ReadNextPacket to try again
	Wake();
}

std::shared_ptr<TMediaPacket> AvfMediaExtractor::ReadNextPacket()
{
	if ( mPacketQueue.IsEmpty() )
		return nullptr;
	
	std::lock_guard<std::mutex> Lock( mPacketQueueLock );
	return mPacketQueue.PopAt(0);
}

AvfMediaExtractor::AvfMediaExtractor(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext) :
	TMediaExtractor	( Params ),
	mOpenglContext	( OpenglContext ),
	mRenderer		( new AvfDecoderRenderer() )
{
	
}

void AvfMediaExtractor::OnSampleBuffer(CMSampleBufferRef sampleBufferRef,size_t StreamIndex,bool DoRetain)
{
	//	gr: I think stalling too long here can make USB bus crash (killing bluetooth, hid, audio etc)
	Soy::TScopeTimerPrint Timer("AvfMediaExtractor::OnSampleBuffer",5);
	
	//Soy::Assert( sampleBufferRef != nullptr, "Expected sample buffer ref");
	if ( !sampleBufferRef )
		return;
	
	//	callback on another thread, so need to catch exceptions
	try
	{
		CMTime CmTimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBufferRef);
		SoyTime Timestamp = Soy::Platform::GetTime(CmTimestamp);
		
		auto pPacket = std::make_shared<TMediaPacket>();
		auto& Packet = *pPacket;
		Packet.mMeta = GetFrameMeta( sampleBufferRef, StreamIndex );
		Packet.mTimecode = Timestamp;
		Packet.mPixelBuffer = std::make_shared<CFPixelBuffer>( sampleBufferRef, DoRetain, mRenderer, Packet.mMeta.mTransform );
		
		QueuePacket( pPacket );
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " exception; " << e.what() << std::endl;
		CFPixelBuffer StackRelease(sampleBufferRef,false,mRenderer,float3x3());
		return;
	}
}


void AvfMediaExtractor::OnSampleBuffer(CVPixelBufferRef sampleBufferRef,SoyTime Timestamp,size_t StreamIndex,bool DoRetain)
{
	if ( !sampleBufferRef )
		return;
	
	//	callback on another thread, so need to catch exceptions
	try
	{
		auto pPacket = std::make_shared<TMediaPacket>();
		auto& Packet = *pPacket;
		Packet.mMeta = GetFrameMeta( sampleBufferRef, StreamIndex );
		Packet.mTimecode = Timestamp;
		Packet.mPixelBuffer = std::make_shared<CVPixelBuffer>( sampleBufferRef, DoRetain, mRenderer, Packet.mMeta.mTransform );
		
		QueuePacket( pPacket );
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " exception; " << e.what() << std::endl;
		CVPixelBuffer StackRelease(sampleBufferRef,false,mRenderer,float3x3());
		return;
	}

}

/*
bool TVideoDevice_AvFoundation::GetOption(TVideoOption::Type Option,bool Default)
{
	Soy_AssertTodo();
	return Default;
}


bool TVideoDevice_AvFoundation::SetOption(TVideoOption::Type Option, bool Enable)
{
	auto* device = mWrapper ? mWrapper->mDevice : nullptr;
	if ( !device )
		return false;

	if ( !BeginConfiguration() )
		return false;

	//	gr: is this needed???
	//if (([device hasMediaType:AVMediaTypeVideo]) && ([device position] == AVCaptureDevicePositionBack))

	NSError* error = nil;
	[device lockForConfiguration:&error];
	
	bool Supported = false;
	if ( !error )
	{
		switch ( Option )
		{
			case TVideoOption::LockedExposure:
				Supported = setExposureLocked( Enable );
				break;
			
			case TVideoOption::LockedFocus:
				Supported = setFocusLocked( Enable );
				break;
			
			case TVideoOption::LockedWhiteBalance:
				Supported = setWhiteBalanceLocked( Enable );
				break;
				
			default:
				std::Debug << "tried to set video device " << GetSerial() << " unknown option " << Option << std::endl;
				Supported = false;
				break;
		}
	}
	
	//	gr: dont unlock if error?
	[device unlockForConfiguration];
	
	EndConfiguration();

	return Supported;
}

bool TVideoDevice_AvFoundation::setFocusLocked(bool Enable)
{
	auto* device = mWrapper->mDevice;
	if ( Enable )
	{
		if ( ![device isFocusModeSupported:AVCaptureFocusModeLocked] )
			return false;
		
		device.focusMode = AVCaptureFocusModeLocked;
		return true;
	}
	else
	{
		if ( ![device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])
			return false;
		
		device.focusMode = AVCaptureFocusModeContinuousAutoFocus;
		return true;
	}
}


bool TVideoDevice_AvFoundation::setWhiteBalanceLocked(bool Enable)
{
	auto* device = mWrapper->mDevice;
	if ( Enable )
	{
		if ( ![device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeLocked] )
			return false;
		
		device.whiteBalanceMode = AVCaptureWhiteBalanceModeLocked;
		return true;
	}
	else
	{
		if ( ![device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance])
			return false;
		
		device.whiteBalanceMode = AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance;
		return true;
	}
}



bool TVideoDevice_AvFoundation::setExposureLocked(bool Enable)
{
	auto* device = mWrapper->mDevice;
	if ( Enable )
	{
		if ( ![device isExposureModeSupported:AVCaptureExposureModeLocked] )
			return false;
		
		device.exposureMode = AVCaptureExposureModeLocked;
		return true;
	}
	else
	{
		if ( ![device isExposureModeSupported:AVCaptureExposureModeContinuousAutoExposure])
			return false;
		
		device.exposureMode = AVCaptureExposureModeContinuousAutoExposure;
		return true;
	}
}

bool TVideoDevice_AvFoundation::BeginConfiguration()
{
	auto* Session = mWrapper ? mWrapper->_session : nullptr;
	if ( !Soy::Assert( Session, "Expected session") )
		return false;
	
	if ( mConfigurationStackCounter == 0)
		[Session beginConfiguration];
	
	mConfigurationStackCounter++;
	return true;
}

bool TVideoDevice_AvFoundation::EndConfiguration()
{
	auto* Session = mWrapper ? mWrapper->_session : nullptr;
	if ( !Soy::Assert( Session, "Expected session") )
		return false;
	mConfigurationStackCounter--;
	
	if (mConfigurationStackCounter == 0)
		[Session commitConfiguration];
	return true;
}

TVideoDeviceMeta TVideoDevice_AvFoundation::GetMeta() const
{
	if ( !mWrapper )
		return TVideoDeviceMeta();
	
	return GetDeviceMeta( mWrapper->mDevice );
}


void SoyVideoContainer_AvFoundation::GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas)
{
	TVideoDevice_AvFoundation::GetDevices( Metas );
}


 */


