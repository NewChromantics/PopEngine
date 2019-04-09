#include "TApiMedia.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoyFilesystem.h"
#include "SoyLib\src\SoyMedia.h"

//	video capture
#if defined(TARGET_OSX)
#include "PopMovie/AvfVideoCapture.h"
#include "PopMovie/AvfMovieDecoder.h"
#include "SoyDecklink/SoyDecklink.h"
#endif


//	video decoding
#if defined(TARGET_OSX)
#include "Libs/PopH264Framework.framework/Headers/PopH264DecoderInstance.h"
#endif

#if defined(TARGET_WINDOWS)
#include "Soylib/src/SoyRuntimeLibrary.h"
namespace PopCameraDevice
{
	#include "Libs/PopCameraDevice/PopCameraDevice.h"
	void	EnumDevices(std::function<void(const std::string&)> EnumDevice);
	void	LoadDll();
}
#endif



namespace ApiMedia
{
	const char Namespace[] = "Pop.Media";
	
	DEFINE_BIND_TYPENAME(Source);
	
	void	EnumDevices(Bind::TCallback& Params);
	void	EnumPlatformDevices(std::function<void(const std::string& Name)> EnumDevice);
}

DEFINE_BIND_FUNCTIONNAME(EnumDevices);

DEFINE_BIND_TYPENAME(Source);
DEFINE_BIND_FUNCTIONNAME(Free);
DEFINE_BIND_FUNCTIONNAME(GetNextFrame);

DEFINE_BIND_TYPENAME(AvcDecoder);
DEFINE_BIND_FUNCTIONNAME(Decode);

const char FrameTimestampKey[] = "Time";

void ApiMedia::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<EnumDevices_FunctionName>( ApiMedia::EnumDevices, Namespace );

	Context.BindObjectType<TMediaSourceWrapper>( Namespace );
	Context.BindObjectType<TAvcDecoderWrapper>( Namespace );
}

void ApiMedia::EnumPlatformDevices(std::function<void(const std::string& Name)> EnumDevice)
{
#if defined(TARGET_OSX)
	try
	{
		::Platform::EnumCaptureDevices(EnumDevice);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}

	try
	{
		Decklink::EnumDevices(EnumDevice);
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
#endif

	PopCameraDevice::EnumDevices(EnumDevice);
}


void ApiMedia::EnumDevices(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise(__FUNCTION__);

	auto DoEnumDevices = [&]
	{
		try
		{
			Array<std::string> DeviceNames;
			auto EnumDevice = [&](const std::string& Name)
			{
				DeviceNames.PushBack(Name);
			};
			EnumPlatformDevices(EnumDevice);
			
						
			//	gr: don't bother queuing, assume Resolve/Reject is always queued
			Promise.Resolve( GetArrayBridge(DeviceNames) );
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
			
			//	queue the error callback
			std::string ExceptionString(e.what());
			Promise.Reject( ExceptionString );
		}
	};
	
	//	immediate... if this is slow, put it on a thread
	DoEnumDevices();
	
	Params.Return(Promise);
}


TMediaSourceWrapper::~TMediaSourceWrapper()
{
	std::Debug << __func__ << std::endl;
	if ( mExtractor )
	{
		std::Debug << __func__ << " stopping extractor" << std::endl;
		mExtractor->Stop();
		mExtractor.reset();
	}
	std::Debug << __func__ << " finished" << std::endl;
}

std::shared_ptr<TMediaExtractor> TMediaSourceWrapper::AllocExtractor(const TMediaExtractorParams& Params)
{
#if defined(TARGET_OSX)
	//	video extractor if it's a filename
	if ( ::Platform::FileExists(Params.mFilename) )
	{
		std::shared_ptr<Opengl::TContext> OpenglContext;
		
		auto Extractor = ::Platform::AllocVideoDecoder( Params, OpenglContext );
		if ( Extractor )
			return Extractor;
	}
	
	//	try decklink devices
	{
		try
		{
			auto Extractor = Decklink::AllocExtractor(Params);
			if ( Extractor )
				return Extractor;
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
		}
	}
	
	//	try platforms capture devices
	{
		auto Extractor = ::Platform::AllocCaptureExtractor( Params, nullptr );
		if ( Extractor )
			return Extractor;
	}
#endif

	std::stringstream Error;
	Error << "Failed to allocate a device matching " << Params.mFilename;
	throw Soy::AssertException(Error.str());
}


void TMediaSourceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
	auto SinglePlaneOutput = !Params.IsArgumentUndefined(1) ? Params.GetArgumentBool(1) : false;
	auto MaxBufferSize = !Params.IsArgumentUndefined(2) ? Params.GetArgumentInt(2) : 10;
	
	auto OnFrameExtracted = [=](const SoyTime Time,size_t StreamIndex)
	{
		//std::Debug << "Got stream[" << StreamIndex << "] frame at " << Time << std::endl;
		this->OnNewFrame(StreamIndex);
	};
	auto OnPrePushFrame = [](TPixelBuffer&,const TMediaExtractorParams&)
	{
		//	gr: do filter here!
		//std::Debug << "OnPrePushFrame" << std::endl;
	};

	//	create device
	TMediaExtractorParams ExtractorParams( DeviceName, DeviceName, OnFrameExtracted, OnPrePushFrame );
	ExtractorParams.mForceNonPlanarOutput = SinglePlaneOutput;
	ExtractorParams.mDiscardOldFrames = true;
	ExtractorParams.mExtractAudioStreams = false;
	
	mExtractor = AllocExtractor(ExtractorParams);
	mExtractor->AllocStreamBuffer(0,MaxBufferSize);
	mExtractor->Start(false);
}


void TMediaSourceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<Free_FunctionName>( Free );
	Template.BindFunction<GetNextFrame_FunctionName>( GetNextFrame );
}


void TMediaSourceWrapper::OnNewFrame(size_t StreamIndex)
{
	//	trigger all our requests, no more callback
	auto Runner = [this](Bind::TContext& Context)
	{
		if ( !mFrameRequests.HasPromises() )
			return;

		try
		{
			auto Frame = PopFrame( Context, mFrameRequestParams );
			Bind::TPersistent FramePersistent( Frame );
			
			auto HandlePromise = [&](Bind::TPromise& Promise)
			{
				//	gr: queue these resolves
				//		they invoke render's which seem to cause some problem, but I'm not sure what
				auto Resolve = [=](Bind::TContext& Context)
				{
					auto FrameObject = FramePersistent.GetObject();
					Promise.Resolve( FrameObject );
				};
				Context.Queue(Resolve);
			};
			mFrameRequests.Flush( HandlePromise );
		}
		catch(std::exception& e)
		{
			std::string Error(e.what());
			auto DoReject = [=](Bind::TContext& Context)
			{
				mFrameRequests.Reject( Error );
			};
			Context.Queue(DoReject);
		}
	};
	mContext.Queue( Runner );
}

Bind::TPromise TMediaSourceWrapper::AllocFrameRequestPromise(Bind::TContext& Context,const TFrameRequestParams& Params)
{
	mFrameRequestParams = Params;
	return mFrameRequests.AddPromise( Context );
}

//	returns a promise that will be invoked when there's frames in the buffer
void TMediaSourceWrapper::GetNextFrame(Bind::TCallback& Params)
{
	auto& This = Params.This<TMediaSourceWrapper>();
	
	TFrameRequestParams Request;
	
	if ( Params.IsArgumentArray(0) )
		Request.mSeperatePlanes = true;
	else if ( !Params.IsArgumentUndefined(0) )
		Request.mSeperatePlanes = Params.GetArgumentBool(0);
	
	if ( !Params.IsArgumentUndefined(1) )
		Request.mStreamIndex = Params.GetArgumentInt(1);
	
	if ( !Params.IsArgumentUndefined(2) )
		Request.mLatestFrame = Params.GetArgumentBool(2);

	auto Promise = This.AllocFrameRequestPromise( Params.mContext, Request );
	Params.Return( Promise );
	
	//	if there are frames waiting, trigger
	{
		auto StreamIndex = Request.mStreamIndex;
		auto PacketBuffer = This.mExtractor->GetStreamBuffer(StreamIndex);
		if ( PacketBuffer )
		{
			if ( PacketBuffer->HasPackets() )
			{
				//std::Debug << "GetNextFrame() frame already queued" << std::endl;
				This.OnNewFrame(StreamIndex);
			}
		}
	}
}

void TMediaSourceWrapper::PopFrame(Bind::TCallback& Params)
{
	auto& This = Params.This<TMediaSourceWrapper>();

	TFrameRequestParams Request;
	
	if ( Params.IsArgumentArray(0) )
		Request.mSeperatePlanes = true;
	else if ( !Params.IsArgumentUndefined(0) )
		Request.mSeperatePlanes = Params.GetArgumentBool(0);
	
	if ( !Params.IsArgumentUndefined(1) )
		Request.mStreamIndex = Params.GetArgumentInt(1);
	
	if ( !Params.IsArgumentUndefined(2) )
		Request.mLatestFrame = Params.GetArgumentBool(2);
	
	auto Frame = This.PopFrame( Params.mContext, Request );
	Params.Return( Frame );
}

Bind::TObject TMediaSourceWrapper::PopFrame(Bind::TContext& Context,const TFrameRequestParams& Params)
{
	//	grab frame
	auto StreamIndex = Params.mStreamIndex;
	auto PacketBuffer = mExtractor->GetStreamBuffer(StreamIndex);
	if ( !PacketBuffer )
	{
		std::stringstream Error;
		Error << "No packet buffer for stream " << StreamIndex;
		throw Soy::AssertException(Error.str());
	}
	
	auto FramePacket = PacketBuffer->PopPacket( Params.mLatestFrame );
	if ( !FramePacket )
		throw Soy::AssertException("No frame packet buffered");
	auto PixelBuffer = FramePacket->mPixelBuffer;
	if ( PixelBuffer == nullptr )
		throw Soy::AssertException("Missing Pixel buffer in frame");
	
	
	//	if the user provides an array, split planes now
	//	todo: switch this to a promise, but we also what to make use of pixelbuffers...
	//		but that [needs to] output multiple textures too...
	
	//	todo: add this for transform
	auto SetTime = [&](Bind::TObject& Object)
	{
		auto FrameTime = FramePacket->GetStartTime();
		if ( FrameTime.IsValid() )
		{
			Object.SetInt( FrameTimestampKey, FrameTime.mTime );
		}
	};
	
	
	if ( Params.mSeperatePlanes )
	{
		BufferArray<SoyPixelsImpl*,5> Planes;
		//	ref counted by js, but need to cleanup if we throw...
		BufferArray<TImageWrapper*,5> Images;
		float3x3 Transform;
		PixelBuffer->Lock( GetArrayBridge(Planes), Transform );
		try
		{
			//	gr: worried this might be stalling OS/USB bus
			Soy::TScopeTimerPrint Timer("PixelBuffer->Lock/Unlock duration",2);
			//	make an image for every plane
			for ( auto p=0;	p<Planes.GetSize();	p++ )
			{
				auto* Plane = Planes[p];
				auto ImageObject = Context.CreateObjectInstance( TImageWrapper::GetTypeName() );
				auto& Image = ImageObject.This<TImageWrapper>();
				Image.SetPixels( *Plane );
				Images.PushBack( &Image );
			}
			PixelBuffer->Unlock();
		}
		catch(std::exception& e)
		{
			std::Debug << "Possible memleak with plane images x" << Images.GetSize() << "..." << std::endl;
			PixelBuffer->Unlock();
			throw;
		}
		
		//	gr: old setup filled this array. need to really put that back?
		//		should we just have a split planes() func for the image...
		auto PlaneArray = Context.CreateArray(0);
		
		//auto PlaneArray = Params.GetArgumentArray(0);
		for ( auto i=0;	i<Images.GetSize();	i++ )
		{
			auto& Image = *Images[i];
			auto ImageHandle = Image.GetHandle();
			PlaneArray.Set( i, ImageHandle );
		}
		
		//	create a dumb object with meta to return
		auto FrameHandle = Context.CreateObjectInstance();
		FrameHandle.SetArray("Planes", PlaneArray );
		SetTime( FrameHandle );
		return FrameHandle;
	}

	
	auto ImageObject = Context.CreateObjectInstance( TImageWrapper::GetTypeName() );
	auto& Image = ImageObject.This<TImageWrapper>();
	Image.mName = "MediaSource Frame";
	Image.SetPixelBuffer(PixelBuffer);

	auto ImageHandle = Image.GetHandle();
	SetTime( ImageObject );
	return ImageObject;
}


void TMediaSourceWrapper::Free(Bind::TCallback& Params)
{
	auto& This = Params.This<TMediaSourceWrapper>();
	This.mExtractor.reset();
}


void TAvcDecoderWrapper::Construct(Bind::TCallback& Params)
{
	//	need to have TDecoderInstance for C lib
#if defined(TARGET_OSX)
	mDecoder.reset( new TDecoderInstance );
#else
	throw Soy::AssertException("TAvcDecoderWrapper unsupported");
#endif
}

void TAvcDecoderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<Decode_FunctionName>( Decode );
}

void TAvcDecoderWrapper::Decode(Bind::TCallback& Params)
{
#if defined(TARGET_OSX)
	auto& This = Params.This<TAvcDecoderWrapper>();
	Array<uint8_t> PacketBytes;
	Params.GetArgumentArray( 0, GetArrayBridge(PacketBytes) );
	
	bool ExtractPlanes = false;
	if ( !Params.IsArgumentUndefined(1) )
		ExtractPlanes = Params.GetArgumentBool(1);
	
	auto& Context = Params.mContext;
	
	auto GetImageObjects = [&](std::shared_ptr<SoyPixelsImpl>& Frame,int32_t FrameTime,Array<Bind::TObject>& PlaneImages)
	{
		Array<std::shared_ptr<SoyPixelsImpl>> PlanePixelss;
		Frame->SplitPlanes( GetArrayBridge(PlanePixelss) );
		
		for ( auto p=0;	p<PlanePixelss.GetSize();	p++)
		{
			auto& PlanePixels = *PlanePixelss[p];
			
			auto PlaneImageObject = Context.CreateObjectInstance( TImageWrapper::GetTypeName() );
			auto& PlaneImage = PlaneImageObject.This<TImageWrapper>();
			
			std::stringstream PlaneName;
			PlaneName << "Frame" << FrameTime << "Plane" << p;
			PlaneImage.mName = PlaneName.str();
			PlaneImage.SetPixels( PlanePixels );
			
			PlaneImages.PushBack( PlaneImageObject );
		}
	};
	
	//	this function is synchronous, so it should put stuff straight back in the queue
	//	the callback was handy though, so maybe go back to it
	This.mDecoder->PushData( PacketBytes.GetArray(), PacketBytes.GetDataSize(), 0 );
	//std::Debug << "Decoder meta=" << This.mDecoder->GetMeta() << std::endl;
	
	TFrame Frame;
	Array<Bind::TObject> Frames;
	while ( This.mDecoder->PopFrame(Frame) )
	{
		//std::Debug << "Popping frame" << std::endl;
		auto FrameImageObject = Context.CreateObjectInstance( TImageWrapper::GetTypeName() );
		auto& FrameImage = FrameImageObject.This<TImageWrapper>();
		FrameImage.SetPixels( Frame.mPixels );
		FrameImageObject.SetInt("Time", Frame.mFrameNumber);

		Array<Bind::TObject> FramePlanes;
		if ( ExtractPlanes )
		{
			GetImageObjects( Frame.mPixels, Frame.mFrameNumber, FramePlanes );
			FrameImageObject.SetArray("Planes", GetArrayBridge(FramePlanes) );
		}
		Frames.PushBack( FrameImageObject );
	}
	Params.Return( GetArrayBridge(Frames) );
#else
	throw Soy::AssertException("TAvcDecoderWrapper unsupported");
#endif
}


void PopCameraDevice::LoadDll()
{
	//	current bodge
#if defined(TARGET_WINDOWS)
	static std::shared_ptr<Soy::TRuntimeLibrary> Dll;
	if ( Dll )
		return;
	const char* Filename = "D:\\PopEngine\\src\\Libs\\PopCameraDevice\\PopCameraDevice.dll";
	Dll.reset(new Soy::TRuntimeLibrary(Filename));
#endif
}

void PopCameraDevice::EnumDevices(std::function<void(const std::string&)> EnumDevice)
{
#if defined(TARGET_WINDOWS)
	LoadDll();
	char DeviceNamesBuffer[1000];
	EnumCameraDevices(DeviceNamesBuffer, sizeofarray(DeviceNamesBuffer));
	//	now split
	auto SplitChar = DeviceNamesBuffer[0];
	std::string DeviceNames(&DeviceNamesBuffer[1]);
	auto EnumMatch = [&](const std::string& Part)
	{
		EnumDevice(Part);
		return true;
	};
	Soy::StringSplitByString(EnumMatch, DeviceNames, SplitChar, false);
#endif
}


//	gr: wrapper for PopCameraDevice's C interface
class PopCameraDevice::TDevice
{
public:
	TDevice(const std::string& Name);
	~TDevice();

	std::function<void(SoyPixelsImpl&)>	mOnNewFrame;
	int32_t		mHandle = 0;
};
