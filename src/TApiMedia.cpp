#include "TApiMedia.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoyFilesystem.h"

#include "PopMovie/AvfVideoCapture.h"
#include "PopMovie/AvfMovieDecoder.h"
#include "SoyDecklink/SoyDecklink.h"

#include "Libs/PopH264Framework.framework/Headers/PopH264DecoderInstance.h"


namespace ApiMedia
{
	const char Namespace[] = "Pop.Media";
	
	void	EnumDevices(Bind::TCallback& Params);
}

const char EnumDevices_FunctionName[] = "EnumDevices";

const char MediaSource_TypeName[] = "Source";
const char Free_FunctionName[] = "Free";
const char GetNextFrame_FunctionName[] = "GetNextFrame";

const char AvcDecoder_TypeName[] = "AvcDecoder";
const char Decode_FunctionName[] = "decode";

const char FrameTimestampKey[] = "Time";

void ApiMedia::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<EnumDevices_FunctionName>( ApiMedia::EnumDevices, Namespace );

	Context.BindObjectType<TMediaSourceWrapper>();
	Context.BindObjectType<TAvcDecoderWrapper>();
}



void ApiMedia::EnumDevices(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();

	auto DoEnumDevices = [&]
	{
		try
		{
			Array<std::string> DeviceNames;
			auto EnumDevice = [&](const std::string& Name)
			{
				DeviceNames.PushBack(Name);
			};
			
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
			
			auto OnCompleted = [=](Bind::TContext& Context)
			{
				Promise.Resolve( GetArrayBridge(DeviceNames) );
			};
			
			//	queue the completion, doesn't need to be done instantly
			Params.mContext.Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
			
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Bind::TContext& Context)
			{
				Promise.Reject( ExceptionString );
			};
			Params.mContext.Queue( OnError );
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
	
	std::stringstream Error;
	Error << "Failed to allocate a device matching " << Params.mFilename;
	throw Soy::AssertException(Error.str());
}


void TMediaSourceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
	auto SinglePlaneOutput = !Params.IsArgumentUndefined(1) ? Params.GetArgumentBool(1) : false;
	auto HasFilterCallback = !Params.IsArgumentUndefined(2);
	auto MaxBufferSize = !Params.IsArgumentUndefined(3) ? Params.GetArgumentInt(3) : 10;
	
	if ( HasFilterCallback )
	{
		auto FilterCallbackFunc = Params.GetArgumentFunction(2);
		mOnFrameFilter = Params.mContext.CreatePersistent( FilterCallbackFunc );
	}
	
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
	ExtractorParams.mDiscardOldFrames = false;
	
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
	//	do filter here
	if ( mOnFrameFilter )
	{
		throw Soy::AssertException("Todo: re-implement frame filter callback");
		/*
		//	When we're stuck with non-async stuff in js (posenet)
		//	lets do an immediate "reject frame" option
		//	and if the isolate is yeilded (sleep()) then this can execute now
		bool AllowFrame = true;
		auto FilterRunner = [this,&AllowFrame](Local<Context> context)
		{
			auto& Isolate = *context->GetIsolate();
			
			BufferArray<Local<Value>,2> Args;
			
			auto FuncHandle = mOnFrameFilter->GetLocal(Isolate);
			auto ThisHandle = v8::Local<v8::Object>();
			
			auto AllowHandle = mContainer.ExecuteFunc( context, FuncHandle, ThisHandle, GetArrayBridge(Args) );
			auto AllowBoolHandle = v8::SafeCast<v8::Boolean>(AllowHandle);
			AllowFrame = AllowBoolHandle->BooleanValue();
		};
		mContainer.RunScoped( FilterRunner );
		
		if ( !AllowFrame )
		{
			//	discard the frame by popping it
			auto PacketBuffer = this->mExtractor->GetStreamBuffer(StreamIndex);
			auto FramePacket = PacketBuffer->PopPacket();
			FramePacket.reset();
			return;
		}
		 */
	}
	
	//	notify that there's a new frame
	auto Runner = [this](Bind::TContext& Context)
	{
		try
		{
			auto This = GetHandle();
			auto Func = This.GetFunction("OnNewFrame");
			Bind::TCallback Callback(Context);
			Callback.SetThis( This );
			Context.Execute( Callback );
		}
		catch(std::exception& e)
		{
			std::Debug << "OnNewFrame Exception: " << e.what() << std::endl;
		}
	};
	mContext.Queue( Runner );
}



void TMediaSourceWrapper::GetNextFrame(Bind::TCallback& Params)
{
	auto& This = Params.This<TMediaSourceWrapper>();

	//	grab frame
	auto StreamIndex = 0;
	auto PacketBuffer = This.mExtractor->GetStreamBuffer(StreamIndex);
	auto FramePacket = PacketBuffer->PopPacket();
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
	
	
	if ( Params.IsArgumentArray(0) )
	{
		BufferArray<SoyPixelsImpl*,5> Planes;
		//	ref counted by js, but need to cleanup if we throw...
		BufferArray<TImageWrapper*,5> Images;
		float3x3 Transform;
		PixelBuffer->Lock( GetArrayBridge(Planes), Transform );
		try
		{
			//	make an image for every plane
			for ( auto p=0;	p<Planes.GetSize();	p++ )
			{
				auto* Plane = Planes[p];
				auto ImageObject = Params.mContext.CreateObjectInstance( TImageWrapper::GetObjectTypeName() );
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
		
		auto PlaneArray = Params.GetArgumentArray(0);
		for ( auto i=0;	i<Images.GetSize();	i++ )
		{
			auto& Image = *Images[i];
			auto ImageHandle = Image.GetHandle();
			PlaneArray.Set( i, ImageHandle );
		}
		
		//	create a dumb object with meta to return
		auto FrameHandle = Params.mContext.CreateObjectInstance();
		FrameHandle.SetArray("Planes", PlaneArray );
		SetTime( FrameHandle );
		Params.Return( FrameHandle );
		return;
	}

	
	auto ImageObject = Params.mContext.CreateObjectInstance( TImageWrapper::GetObjectTypeName() );
	auto& Image = ImageObject.This<TImageWrapper>();
	Image.mName = "MediaSource Frame";
	Image.SetPixelBuffer(PixelBuffer);

	auto ImageHandle = Image.GetHandle();
	SetTime( ImageObject );
	Params.Return(ImageObject);
}


void TMediaSourceWrapper::Free(Bind::TCallback& Params)
{
	auto& This = Params.This<TMediaSourceWrapper>();
	This.mExtractor.reset();
}


void TAvcDecoderWrapper::Construct(Bind::TCallback& Params)
{
	mDecoder.reset( new TDecoderInstance );
}

void TAvcDecoderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<Decode_FunctionName>( Decode );
}

void TAvcDecoderWrapper::Decode(Bind::TCallback& Params)
{
	auto& This = Params.This<TAvcDecoderWrapper>();
	Array<uint8_t> PacketBytes;
	Params.GetArgumentArray( 0, GetArrayBridge(PacketBytes) );
	
	bool ExtractPlanes = false;
	if ( !Params.IsArgumentUndefined(1) )
		ExtractPlanes = Params.GetArgumentBool(1);
	
	auto Promise = Params.mContext.CreatePromise();
	auto& Context = Params.mContext;
	
	auto GetImageObjects = [&](std::shared_ptr<SoyPixelsImpl>& Frame,int32_t FrameTime,Array<Bind::TObject>& PlaneImages)
	{
		Array<std::shared_ptr<SoyPixelsImpl>> PlanePixelss;
		Frame->SplitPlanes( GetArrayBridge(PlanePixelss) );
		
		for ( auto p=0;	p<PlanePixelss.GetSize();	p++)
		{
			auto& PlanePixels = *PlanePixelss[p];
			
			auto PlaneImageObject = Context.CreateObjectInstance( TImageWrapper::GetObjectTypeName() );
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
	
	TFrame Frame;
	Array<Bind::TObject> Frames;
	while ( This.mDecoder->PopFrame(Frame) )
	{
		auto FrameImageObject = Context.CreateObjectInstance( TImageWrapper::GetObjectTypeName() );
		auto& FrameImage = FrameImageObject.This<TImageWrapper>();
		FrameImage.SetPixels( Frame.mPixels );
		FrameImageObject.SetInt("Time", Frame.mFrameNumber);

		Array<Bind::TObject> FramePlanes;
		if ( ExtractPlanes )
		{
			GetImageObjects( Frame.mPixels, Frame.mFrameNumber, FramePlanes );
			FrameImageObject.SetArray("Planes", GetArrayBridge(FramePlanes) );
		}
	}
	Params.Return( GetArrayBridge(Frames) );
}


