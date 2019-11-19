#include "TApiMedia.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoyFilesystem.h"
#include "SoyLib/src/SoyMedia.h"


//	video decoding
#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "Libs/PopH264Framework.framework/Headers/PopH264DecoderInstance.h"
#include "Libs/PopH264Framework.framework/Headers/PopH264.h"
#elif defined(TARGET_WINDOWS)
#pragma comment(lib,"PopH264.lib")
#include "Libs/PopH264/PopH264.h"
#include "Libs/PopH264/PopH264DecoderInstance.h"
#endif

#if defined(TARGET_WINDOWS)
#include "Soylib/src/SoyRuntimeLibrary.h"
#endif

#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "Libs/PopCameraDeviceFramework.framework/Headers/TCameraDevice.h"
#include "Libs/PopCameraDeviceFramework.framework/Headers/PopCameraDevice.h"
#elif defined(TARGET_WINDOWS)
#pragma comment(lib,"PopCameraDevice.lib")
#include "Libs/PopCameraDevice/PopCameraDevice.h"
#include "Libs/PopCameraDevice/TCameraDevice.h"
#endif


//	video capture
namespace PopCameraDevice
{
	//	put C funcs into namespace
	void	EnumDevices(std::function<void(const std::string&)> EnumDevice);
	void	LoadDll();

	namespace MetaIndex
	{
		enum Type
		{
			PlaneCount = 0,

			Plane0_Width,
			Plane0_Height,
			Plane0_ComponentCount,
			Plane0_SoyPixelsFormat,
			Plane0_PixelDataSize,

			Plane1_Width,
			Plane1_Height,
			Plane1_ComponentCount,
			Plane1_SoyPixelsFormat,
			Plane1_PixelDataSize,

			Plane2_Width,
			Plane2_Height,
			Plane2_ComponentCount,
			Plane2_SoyPixelsFormat,
			Plane2_PixelDataSize,
		};
	};
}



#if defined(TARGET_WINDOWS)
#include "Libs/x264/include/x264.h"
//#pragma comment(lib,"libx264.lib")
#elif defined(TARGET_OSX)||defined(TARGET_IOS)
#include "Libs/x264/osx/x264.h"
#endif
namespace X264
{
	void	IsOkay(int Result, const char* Context);
	void	Log(void *data, int i_level, const char *psz, va_list args);

	class TPacket;
}




//	gr: wrapper for PopCameraDevice's C interface
//	gr: until we add a callback in the dll, this just keeps checking for frames
//	gr: this thread could wait and wake up once the frame is popped
class PopCameraDevice::TInstance
{
public:
	TInstance(const std::string& Name);
	~TInstance();

	bool			HasFrame() { return !mLastPlanes.IsEmpty(); }
	bool			PopLastFrame(ArrayBridge<std::shared_ptr<SoyPixelsImpl>>&& Pixels, SoyTime& Time,std::string& Meta);

protected:
	void			OnNewFrame();
	TDevice&		GetDevice();

public:
	std::function<void()>	mOnNewFrame;

protected:
	int32_t		mHandle = 0;

	std::recursive_mutex	mLastPlanesLock;
	SoyTime					mLastPlanesTime;
	std::string				mLastFrameMeta;
	//	gr: turn this into a TPixelBuffer
	Array<std::shared_ptr<SoyPixelsImpl>> mLastPlanes;
};



class PopH264::TInstance
{
public:
	TInstance();
	~TInstance();

	TDecoderInstance&	GetDecoder();
	void				PushData(ArrayBridge<uint8_t>&& Data, int32_t FrameNumber);

protected:
	int32_t		mHandle = 0;
};


class X264::TPacket
{
public:
	std::shared_ptr<Array<uint8_t>>	mData;
	uint32_t						mTime = 0;
};

class X264::TInstance
{
public:
	TInstance(size_t PresetValue);
	~TInstance();

	void			PushFrame(const SoyPixelsImpl& Pixels, int32_t FrameTime);
	TPacket			PopPacket();
	bool			HasPackets() {	return !mPackets.IsEmpty();	}
	std::string		GetVersion() const;

private:
	void			AllocEncoder(const SoyPixelsMeta& Meta);
	void			OnOutputPacket(const TPacket& Packet);

protected:
	SoyPixelsMeta	mPixelMeta;	//	invalid until we've pushed first frame
	x264_t*			mHandle = nullptr;
	x264_param_t	mParam;
	x264_picture_t	mPicture;
	Array<TPacket>	mPackets;
	std::mutex		mPacketsLock;

public:
	std::function<void()>	mOnOutputPacket;
};


namespace ApiMedia
{
	const char Namespace[] = "Pop.Media";
	
	DEFINE_BIND_TYPENAME(Source);
	DEFINE_BIND_TYPENAME(AvcDecoder);
	DEFINE_BIND_TYPENAME(H264Encoder);
	DEFINE_BIND_FUNCTIONNAME(EnumDevices);
	DEFINE_BIND_FUNCTIONNAME(Free);
	DEFINE_BIND_FUNCTIONNAME(GetNextFrame);
	DEFINE_BIND_FUNCTIONNAME(Decode);
	DEFINE_BIND_FUNCTIONNAME(Encode);
	DEFINE_BIND_FUNCTIONNAME(GetNextPacket);

	const char FrameTimestampKey[] = "Time";

	void	EnumDevices(Bind::TCallback& Params);
}



void ApiMedia::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<BindFunction::EnumDevices>( ApiMedia::EnumDevices, Namespace );

	Context.BindObjectType<TPopCameraDeviceWrapper>( Namespace );

	Context.BindObjectType<TAvcDecoderWrapper>(Namespace);
	Context.BindObjectType<TH264EncoderWrapper>(Namespace);
}


void ApiMedia::EnumDevices(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise( Params.mLocalContext, __FUNCTION__);

	auto DoEnumDevices = [&]
	{
		try
		{
			Array<std::string> DeviceNames;
			auto EnumDevice = [&](const std::string& Name)
			{
				DeviceNames.PushBack(Name);
			};
			PopCameraDevice::EnumDevices(EnumDevice);
			
						
			//	gr: don't bother queuing, assume Resolve/Reject is always queued
			Promise.Resolve( Params.mLocalContext, GetArrayBridge(DeviceNames) );
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
			
			//	queue the error callback
			std::string ExceptionString(e.what());
			Promise.Reject( Params.mLocalContext, ExceptionString );
		}
	};
	
	//	immediate... if this is slow, put it on a thread
	DoEnumDevices();
	
	Params.Return(Promise);
}



void TAvcDecoderWrapper::Construct(Bind::TCallback& Params)
{
	mDecoder.reset( new PopH264::TInstance );
	std::string ThreadName("TAvcDecoderWrapper::DecoderThread");
	mDecoderThread.reset( new SoyWorkerJobThread(ThreadName) );
	mDecoderThread->Start();
}

void TAvcDecoderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::Decode>( Decode );
}

void TAvcDecoderWrapper::Decode(Bind::TCallback& Params)
{
	auto& This = Params.This<TAvcDecoderWrapper>();
	Array<uint8_t> PacketBytes;
	Params.GetArgumentArray( 0, GetArrayBridge(PacketBytes) );
	
	bool UseFrameBuffer = false;
	bool ExtractImage = true;
	bool _ExtractPlanes = false;
	if ( !Params.IsArgumentUndefined(1) )
	{
		if ( Params.IsArgumentNull(1) )
			ExtractImage = false;
		else
			_ExtractPlanes = Params.GetArgumentBool(1);
	}
	
	if ( !Params.IsArgumentUndefined(2) )
	{
		UseFrameBuffer = Params.GetArgumentBool(2);
	}

	//	process async
	auto Promise = Params.mContext.CreatePromise(Params.mLocalContext, __func__);
	Params.Return( Promise );
	
	auto* pThis = &This;
	
	auto Resolve = [=](Bind::TLocalContext& Context)
	{
		auto GetImageObjects = [&](std::shared_ptr<SoyPixelsImpl>& Frame,int32_t FrameTime,Array<Bind::TObject>& PlaneImages)
		{
			Array<std::shared_ptr<SoyPixelsImpl>> PlanePixelss;
			Frame->SplitPlanes( GetArrayBridge(PlanePixelss) );
		
			auto& mFrameBuffers = pThis->mFrameBuffers;
			while ( UseFrameBuffer && mFrameBuffers.GetSize() < PlanePixelss.GetSize() )
			{
				auto ImageObject = Context.mGlobalContext.CreateObjectInstance( Context, TImageWrapper::GetTypeName() );
				auto& Image = ImageObject.This<TImageWrapper>();
				std::stringstream Name;
				Name << "AVC buffer image plane #" << mFrameBuffers.GetSize();
				Image.mName = Name.str();

				//	gr: make this an option somewhere! really we should pass in the target
				Image.DoSetLinearFilter(true);

				Bind::TPersistent ImageObjectPersistent( Context, ImageObject, Name.str() );
				mFrameBuffers.PushBack( ImageObjectPersistent );
			}
			
			for ( auto p=0;	p<PlanePixelss.GetSize();	p++)
			{
				//	re-use shared ptr
				//auto& PlanePixels = PlanePixelss[p];
				auto& PlanePixels = *PlanePixelss[p];
				
				Bind::TObject PlaneImageObject;
				if ( UseFrameBuffer )
				{
					PlaneImageObject = mFrameBuffers[p].GetObject( Context );
				}
				else
				{
					PlaneImageObject = Context.mGlobalContext.CreateObjectInstance( Context, TImageWrapper::GetTypeName() );
					std::stringstream PlaneName;
					PlaneName << "Frame" << FrameTime << "Plane" << p;
					auto& PlaneImage = PlaneImageObject.This<TImageWrapper>();
					PlaneImage.mName = PlaneName.str();
				}

				auto& PlaneImage = PlaneImageObject.This<TImageWrapper>();
				PlaneImage.SetPixels( PlanePixels );
				
				PlaneImages.PushBack( PlaneImageObject );
			}
		};
		
		auto& Decoder = This.mDecoder->GetDecoder();
		PopH264::TFrame Frame;
		Array<Bind::TObject> Frames;
		
		while ( Decoder.PopFrame(Frame) )
		{
			auto ExtractPlanes = _ExtractPlanes;

			//	because YUV_8_8_8 cannot be expressed into a texture properly,
			//	force plane extraction for this format
			Array<Bind::TObject> FramePlanes;
			if ( Frame.mPixels->GetFormat() == SoyPixelsFormat::Yuv_8_8_8_Full )
				ExtractPlanes = true;

			//std::Debug << "Popping frame" << std::endl;
			auto ObjectTypename = ( ExtractImage && !ExtractPlanes ) ? TImageWrapper::GetTypeName() : std::string();
			auto FrameImageObject = Context.mGlobalContext.CreateObjectInstance( Context, ObjectTypename );
			
			if ( ExtractImage && !ExtractPlanes )
			{
				auto& FrameImage = FrameImageObject.This<TImageWrapper>();
				FrameImage.SetPixels( Frame.mPixels );
			}
			
 			if ( ExtractImage && ExtractPlanes )
			{
				GetImageObjects( Frame.mPixels, Frame.mFrameNumber, FramePlanes );
				FrameImageObject.SetArray("Planes", GetArrayBridge(FramePlanes) );
			}
			
			//	set meta
			FrameImageObject.SetInt("Time", Frame.mFrameNumber);
			FrameImageObject.SetInt("DecodeDuration", Frame.mDecodeDuration.count() );
			Frames.PushBack( FrameImageObject );
		}
		
		Promise.Resolve( Context, GetArrayBridge(Frames) );
	};

	auto* pContext = &Params.mContext;
	auto Decode = [=]()
	{
		auto& Context = *pContext;
		
		try
		{
			auto& Decoder = *This.mDecoder;
			//	push data into queue
			Decoder.PushData(GetArrayBridge(PacketBytes), 0);
			Context.Queue( Resolve );
		}
		catch(std::exception& e)
		{
			std::string Error( e.what() );
			auto DoReject = [=](Bind::TLocalContext& Context)
			{
				Promise.Reject( Context, Error );
			};
			Context.Queue( DoReject );
		}
	};

	//	on decoder thread, run decode
	This.mDecoderThread->PushJob(Decode);
}


void PopCameraDevice::LoadDll()
{
#if defined(TARGET_WINDOWS)
	//	current bodge
	static std::shared_ptr<Soy::TRuntimeLibrary> Dll;
	if ( Dll )
		return;
	const char* Filename = "PopCameraDevice.dll";
	Dll.reset(new Soy::TRuntimeLibrary(Filename));
#endif
}

void PopCameraDevice::EnumDevices(std::function<void(const std::string&)> EnumDevice)
{
	LoadDll();
	char DeviceNamesBuffer[2000] = {0};
	PopCameraDevice_EnumCameraDevices(DeviceNamesBuffer, sizeofarray(DeviceNamesBuffer));

	//	now split
	auto SplitChar = DeviceNamesBuffer[0];
	std::string DeviceNames(&DeviceNamesBuffer[1]);
	auto EnumMatch = [&](const std::string& Part)
	{
		EnumDevice(Part);
		return true;
	};
	Soy::StringSplitByString(EnumMatch, DeviceNames, SplitChar, false);
}



PopCameraDevice::TInstance::TInstance(const std::string& Name)
{
	mHandle = PopCameraDevice_CreateCameraDevice(Name.c_str());
	if ( mHandle <= 0 )
	{
		std::stringstream Error;
		Error << "Failed to create PopCameraDevice named " << Name << ". Error: " << mHandle;
		throw Soy::AssertException(Error.str());
	}


	auto OnNewFrame = [this]()
	{
		this->OnNewFrame();
	};
	auto& Device = GetDevice();
	Device.mOnNewFrame = OnNewFrame;
}


PopCameraDevice::TInstance::~TInstance()
{
	PopCameraDevice_FreeCameraDevice(mHandle);
}

PopCameraDevice::TDevice& PopCameraDevice::TInstance::GetDevice()
{
	auto* DevicePointer = PopCameraDevice_GetDevicePtr( mHandle );
	if ( !DevicePointer )
	{
		std::stringstream Error;
		Error << "Device is null for instance " << mHandle;
		throw Soy::AssertException(Error.str());
	}
	return *DevicePointer;
}


void PopCameraDevice::TInstance::OnNewFrame()
{
	//	this should throw if the device has gone
	auto& Device = GetDevice();

	//	get meta so we know what buffers to allocate
	const int MetaValuesSize = 100;
	int MetaValues[MetaValuesSize];
	PopCameraDevice_GetMeta(mHandle, MetaValues, MetaValuesSize);
	
	//	dont have meta yet
	auto PlaneCount = MetaValues[MetaIndex::PlaneCount];
	if ( PlaneCount == 0 )
		return;

	//	reuse temp buffers
	const int MaxPlaneCount = 3;
	if ( PlaneCount > MaxPlaneCount )
	{
		std::stringstream Error;
		Error << "Got " << PlaneCount << " when max is " << MaxPlaneCount;
		throw Soy::AssertException(Error);
	}

	
	SoyPixelsMeta PlaneMeta[MaxPlaneCount];
	PlaneMeta[0] = SoyPixelsMeta(MetaValues[MetaIndex::Plane0_Width], MetaValues[MetaIndex::Plane0_Height], static_cast<SoyPixelsFormat::Type>(MetaValues[MetaIndex::Plane0_SoyPixelsFormat]));
	PlaneMeta[1] = SoyPixelsMeta(MetaValues[MetaIndex::Plane1_Width], MetaValues[MetaIndex::Plane1_Height], static_cast<SoyPixelsFormat::Type>(MetaValues[MetaIndex::Plane1_SoyPixelsFormat]));
	PlaneMeta[2] = SoyPixelsMeta(MetaValues[MetaIndex::Plane2_Width], MetaValues[MetaIndex::Plane2_Height], static_cast<SoyPixelsFormat::Type>(MetaValues[MetaIndex::Plane2_SoyPixelsFormat]));

	
	Array<std::shared_ptr<SoyPixelsImpl>> PlanePixels;
	Array<uint8_t*> PlanePixelsBytes;
	Array<size_t> PlanePixelsByteSize;
	for ( auto i=0;	i<PlaneCount;	i++ )
	{
		auto& Meta = PlaneMeta[i];
		if ( !Meta.IsValid() )
			continue;

		auto& Plane = PlanePixels.PushBack();
		Plane.reset(new SoyPixels(Meta));
		auto& Array = Plane->GetPixelsArray();
		PlanePixelsBytes.PushBack(Array.GetArray());
		PlanePixelsByteSize.PushBack(Array.GetDataSize());
	}

	while ( PlanePixelsBytes.GetSize() < MaxPlaneCount )
	{
		PlanePixelsBytes.PushBack(nullptr);
		PlanePixelsByteSize.PushBack(0);
	}
	
	char FrameMeta[1000] = { 0 };

	//	check for a new frame
	auto Result = PopCameraDevice_PopFrameAndMeta(mHandle,
		PlanePixelsBytes[0], PlanePixelsByteSize[0],
		PlanePixelsBytes[1], PlanePixelsByteSize[1],
		PlanePixelsBytes[2], PlanePixelsByteSize[2],
		FrameMeta,
		sizeofarray(FrameMeta)
	);

	//	no new frame
	if ( Result == 0 )
		return;

	//	save plane pixels for pop
	{
		std::lock_guard<std::recursive_mutex> Lock(mLastPlanesLock);
		mLastPlanes.Copy(PlanePixels);
		mLastPlanesTime = SoyTime(true);
		mLastFrameMeta = FrameMeta;
	}

	//	notify
	this->mOnNewFrame();
}

bool PopCameraDevice::TInstance::PopLastFrame(ArrayBridge<std::shared_ptr<SoyPixelsImpl> >&& Pixels, SoyTime& Time,std::string& Meta)
{
	std::lock_guard<std::recursive_mutex> Lock(mLastPlanesLock);
	if ( mLastPlanes.IsEmpty() )
		return false;

	Pixels.Copy(mLastPlanes);
	Time = mLastPlanesTime;
	Meta = mLastFrameMeta;

	mLastPlanes.Clear();
	mLastPlanesTime = SoyTime();
	mLastFrameMeta.clear();
	return true;
}





void TPopCameraDeviceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
	//auto SinglePlaneOutput = !Params.IsArgumentUndefined(1) ? Params.GetArgumentBool(1) : false;
	//auto MaxBufferSize = !Params.IsArgumentUndefined(2) ? Params.GetArgumentInt(2) : 10;

	auto OnFrameExtracted = [=]()
	{
		//std::Debug << "Got stream[" << StreamIndex << "] frame at " << Time << std::endl;
		this->OnNewFrame();
	};

	mExtractor.reset(new PopCameraDevice::TInstance(DeviceName));
	mExtractor->mOnNewFrame = OnFrameExtracted;
}


void TPopCameraDeviceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::GetNextFrame>( GetNextFrame );
}


//	returns a promise that will be invoked when there's frames in the buffer
void TPopCameraDeviceWrapper::GetNextFrame(Bind::TCallback& Params)
{
	auto& This = Params.This<TPopCameraDeviceWrapper>();

	TFrameRequestParams Request;

	if ( Params.IsArgumentArray(0) )
		Request.mSeperatePlanes = true;
	else if ( Params.IsArgumentObject(0) )
	{
		auto DestinationImage = Params.GetArgumentObject(0);
		Request.mDestinationImage = Bind::TPersistent( Params.mLocalContext, DestinationImage, "Destination Image" );
		auto& IsImageCheck = Request.mDestinationImage.GetObject( Params.mLocalContext ).This<TImageWrapper>();
	}
	else if ( !Params.IsArgumentUndefined(0) )
		Request.mSeperatePlanes = Params.GetArgumentBool(0);

	if ( !Params.IsArgumentUndefined(1) )
		Request.mStreamIndex = Params.GetArgumentInt(1);

	if ( !Params.IsArgumentUndefined(2) )
		Request.mLatestFrame = Params.GetArgumentBool(2);

	auto Promise = This.AllocFrameRequestPromise( Params.mLocalContext, Request );
	Params.Return( Promise );

	//	if there are frames waiting, trigger
	if ( This.mExtractor->HasFrame() )
	{
		This.OnNewFrame();
	}
}


void TPopCameraDeviceWrapper::OnNewFrame()
{
	//	trigger all our requests, no more callback
	auto Runner = [this](Bind::TLocalContext& Context)
	{
		Bind::TObject Frame;

		//	call immedate callback if there is one
		auto Device = this->GetHandle(Context);
		if ( Device.HasMember("OnNewFrame") )
		{
			try
			{
				auto OnNewFrameFunc = Device.GetFunction("OnNewFrame");
				if ( !Frame.mThis )
					Frame = PopFrame(Context, mFrameRequestParams);

				Bind::TCallback Call(Context);
				Call.SetArgumentObject(0, Frame);
				OnNewFrameFunc.Call(Call);
			} catch ( std::exception& e )
			{
				std::Debug << "Calling OnNewFrame exception: " << e.what() << std::endl;
			}
		}

		if ( !mFrameRequests.HasPromises() )
			return;

		try
		{
			if ( !Frame.mThis )
				Frame = PopFrame( Context, mFrameRequestParams );
			Bind::TPersistent FramePersistent( Context, Frame, "Frame" );

			auto HandlePromise = [&](Bind::TLocalContext& Context,Bind::TPromise& Promise)
			{
				//	gr: queue these resolves
				//		they invoke render's which seem to cause some problem, but I'm not sure what
				auto Resolve = [=](Bind::TLocalContext& Context)
				{
					auto FrameObject = FramePersistent.GetObject( Context );
					Promise.Resolve( Context, FrameObject );
				};
				Context.mGlobalContext.Queue(Resolve);
			};
			mFrameRequests.Flush( HandlePromise );
		}
		catch(std::exception& e)
		{
			std::string Error(e.what());
			auto DoReject = [=](Bind::TLocalContext& Context)
			{
				mFrameRequests.Reject( Error );
			};
			Context.mGlobalContext.Queue(DoReject);
		}
	};
	GetContext().Queue( Runner );
}


Bind::TObject TPopCameraDeviceWrapper::PopFrame(Bind::TLocalContext& Context,TFrameRequestParams& Params)
{
	Array<std::shared_ptr<SoyPixelsImpl>> Planes;
	SoyTime FrameTime;
	std::string FrameMeta;
	if ( !mExtractor->PopLastFrame( GetArrayBridge(Planes), FrameTime, FrameMeta) )
		throw Soy::AssertException("No frame packet buffered");

	//	set all metas
	auto SetMeta = [&](Bind::TObject& Object)
	{
		if ( FrameTime.IsValid() )
		{
			Object.SetInt( ApiMedia::FrameTimestampKey, FrameTime.mTime );
		}

		if (FrameMeta.length())
		{
			if (FrameMeta[0] == '{')
			{
				//	turn string into json object
				Object.SetObjectFromString("Meta", FrameMeta);
			}
			else
			{
				Object.SetString("Meta", FrameMeta);
			}
		}
	};
	
	if ( Params.mDestinationImage )
	{
		auto& Dest = Params.mDestinationImage;
		auto Object = Dest.GetObject( Context );
		SetMeta(Object);
		auto& Image = Object.This<TImageWrapper>();
		auto& Plane = Planes[0];
		Image.SetPixels( Plane );
		return Object;
	}
	else if ( Params.mSeperatePlanes )
	{
		//BufferArray<SoyPixelsImpl*,5> Planes;
		//	ref counted by js, but need to cleanup if we throw...
		BufferArray<TImageWrapper*,5> Images;
		float3x3 Transform;
		//PixelBuffer->Lock(GetArrayBridge(Planes), Transform);
		try
		{
			//	gr: worried this might be stalling OS/USB bus
			Soy::TScopeTimerPrint Timer("PixelBuffer->Lock/Unlock duration",2);
			//	make an image for every plane
			for ( auto p=0;	p<Planes.GetSize();	p++ )
			{
				auto& Plane = Planes[p];
				auto ImageObject = Context.mGlobalContext.CreateObjectInstance( Context, TImageWrapper::GetTypeName() );
				auto& Image = ImageObject.This<TImageWrapper>();
				Image.SetPixels( *Plane );
				Images.PushBack( &Image );
			}
			//PixelBuffer->Unlock();
		}
		catch(std::exception& e)
		{
			std::Debug << "Possible memleak with plane images x" << Images.GetSize() << "..." << std::endl;
			//PixelBuffer->Unlock();
			throw;
		}

		//	gr: old setup filled this array. need to really put that back?
		//		should we just have a split planes() func for the image...
		Array<Bind::TObject> ImageHandles;
		
		//auto PlaneArray = Params.GetArgumentArray(0);
		for ( auto i=0;	i<Images.GetSize();	i++ )
		{
			auto& Image = *Images[i];
			auto ImageHandle = Image.GetHandle(Context);
			ImageHandles.PushBack( ImageHandle );
		}

		//	create a dumb object with meta to return
		auto FrameHandle = Context.mGlobalContext.CreateObjectInstance( Context );
		FrameHandle.SetArray("Planes", GetArrayBridge(ImageHandles) );
		SetMeta( FrameHandle );
		return FrameHandle;
	}


	auto ImageObject = Context.mGlobalContext.CreateObjectInstance( Context, TImageWrapper::GetTypeName() );
	auto& Image = ImageObject.This<TImageWrapper>();
	Image.mName = "MediaSource Frame";
	Image.SetPixels( Planes[0] );

	auto ImageHandle = Image.GetHandle( Context );
	SetMeta( ImageObject );
	return ImageObject;
}


Bind::TPromise TPopCameraDeviceWrapper::AllocFrameRequestPromise(Bind::TLocalContext& Context,const TFrameRequestParams& Params)
{
	mFrameRequestParams = Params;
	return mFrameRequests.AddPromise( Context );
}


PopH264::TInstance::TInstance()
{
	mHandle = PopH264_CreateInstance();
	if ( mHandle <= 0 )
	{
		std::stringstream Error;
		Error << "Failed to create PopH264 instance. Error=" << mHandle;
		throw Soy::AssertException(Error.str());
	}

}


PopH264::TInstance::~TInstance()
{
	PopH264_DestroyInstance(mHandle);
}

PopH264::TDecoderInstance& PopH264::TInstance::GetDecoder()
{
	auto* DecoderPointer = PopH264_GetInstancePtr( mHandle );
	if ( !DecoderPointer )
	{
		std::stringstream Error;
		Error << "Decoder is null for instance " << mHandle;
		throw Soy::AssertException(Error.str());
	}
	return *DecoderPointer;
}


void PopH264::TInstance::PushData(ArrayBridge<uint8_t>&& Data, int32_t FrameNumber)
{
	auto* DataPtr = Data.GetArray();
	auto DataSize = Data.GetDataSize();
	auto Result = PopH264_PushData( mHandle, DataPtr, DataSize, FrameNumber);
	if ( Result != 0 )
	{
		std::stringstream Error;
		Error << "PopH264_PushData(handle=" << mHandle << ") returned " << Result;
		throw Soy::AssertException(Error.str());
	}
}




void TH264EncoderWrapper::Construct(Bind::TCallback& Params)
{
	int PresetValue = 5;
	try
	{
		PresetValue = Params.GetArgumentInt(0);
		if ( PresetValue < 0 || PresetValue	> 9 )
		{
			std::stringstream Error;
			Error << "Preset " << PresetValue << " out of range";
			throw Soy::AssertException(Error);
		}
	}
	catch(std::exception& e)
	{
		std::stringstream Error;
		Error << "Expected arg0 as preset between 0..9 (ultrafast...placebo); " << e.what();
		throw Soy::AssertException(Error);
	}
	mEncoder.reset(new X264::TInstance(PresetValue));

	mEncoder->mOnOutputPacket = [&]()
	{
		this->OnPacketOutput();
	};

	mEncoderThread.reset(new SoyWorkerJobThread("H264 encoder"));
	mEncoderThread->Start();
	
	
	//	set meta (can we do this on the static object/constructor?)
	Params.ThisObject().SetString("Version", mEncoder->GetVersion() );
}

void TH264EncoderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::Encode>(&TH264EncoderWrapper::Encode);
	Template.BindFunction<ApiMedia::BindFunction::GetNextPacket>(&TH264EncoderWrapper::GetNextPacket);
}

void TH264EncoderWrapper::Encode(Bind::TCallback& Params)
{
	auto& Frame = Params.GetArgumentPointer<TImageWrapper>(0);
	auto FrameTime = Params.GetArgumentInt(1);

	if (mEncoderThread)
	{
		std::shared_ptr<SoyPixels> PixelCopy( new SoyPixels() );
		{
			Soy::TScopeTimerPrint Timer("Copy pixels for thread", 1);
			Frame.GetPixels(*PixelCopy);
		}
		auto Encode = [=]()mutable
		{
			mEncoder->PushFrame(*PixelCopy, FrameTime);
		};
		mEncoderThread->PushJob(Encode);
	}
	else
	{
		auto& Pixels = Frame.GetPixels();
		mEncoder->PushFrame(Pixels, FrameTime);
	}
}

void TH264EncoderWrapper::GetNextPacket(Bind::TCallback& Params)
{
	auto Promise = mNextPacketPromises.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	//	flush in case there's data availible
	OnPacketOutput();
}

void TH264EncoderWrapper::OnPacketOutput()
{
	//	no promises
	if (!mNextPacketPromises.HasPromises())
		return;

	auto NextPacket = mEncoder->PopPacket();
	//	no packets yet!
	if (!NextPacket.mData)
		return;

	auto& PacketData = *NextPacket.mData;

	auto Resolve = [&](Bind::TLocalContext& Context,Bind::TPromise& Promise)
	{
		auto Packet = Context.mGlobalContext.CreateObjectInstance(Context);
		Packet.SetInt("Time", NextPacket.mTime);
		Packet.SetArray("Data", GetArrayBridge(PacketData));

		Promise.Resolve(Context, Packet);
	};
	mNextPacketPromises.Flush(Resolve);
}


void X264::IsOkay(int Result, const char* Context)
{
	if (Result == 0)
		return;

	std::stringstream Error;
	Error << "X264 error " << Result << " (" << Context << ")";
	throw Soy::AssertException(Error);
}


X264::TInstance::TInstance(size_t PresetValue)
{
	if ( PresetValue > 9 )
		throw Soy_AssertException("Expecting preset value <= 9");
	
	//	todo: tune options. takes , seperated values
	const char* Tune = nullptr;
	auto* PresetName = x264_preset_names[PresetValue];
	auto Result = x264_param_default_preset(&mParam, PresetName, Tune);
	IsOkay(Result,"x264_param_default_preset");
}

X264::TInstance::~TInstance()
{

}


std::string X264::TInstance::GetVersion() const
{
	std::stringstream Version;
	Version << "x264 " << X264_POINTVER;
	return Version.str();
}

void X264::Log(void *data, int i_level, const char *psz, va_list args)
{
	std::stringstream Debug;
	Debug << "x264 ";

	switch (i_level)
	{
	case X264_LOG_ERROR:	Debug << "Error: ";	break;
	case X264_LOG_WARNING:	Debug << "Warning: ";	break;
	case X264_LOG_INFO:		Debug << "Info: ";	break;
	case X264_LOG_DEBUG:	Debug << "Debug: ";	break;
	default:				Debug << "???: ";	break;
	}
	
	auto temp = std::vector<char>{};
	auto length = std::size_t{ 63 };
	while (temp.size() <= length)
	{
		temp.resize(length + 1);
		//va_start(args, psz);
		const auto status = std::vsnprintf(temp.data(), temp.size(), psz, args);
		//va_end(args);
		if (status < 0)
			throw std::runtime_error{ "string formatting error" };
		length = static_cast<std::size_t>(status);
	}
	auto FormattedString = std::string{ temp.data(), length };
	Debug << FormattedString;
	//msg_GenericVa(p_enc, i_level, MODULE_STRING, psz, args);
	std::Debug << Debug.str() << std::endl;
}

void X264::TInstance::AllocEncoder(const SoyPixelsMeta& Meta)
{
	//	todo: change PPS if content changes
	if (mPixelMeta.IsValid())
	{
		if (mPixelMeta == Meta)
			return;
		std::stringstream Error;
		Error << "H264 encoder pixel format changing from " << mPixelMeta << " to " << Meta << ", currently unsupported";
		throw Soy_AssertException(Error);
	}

	//	do final configuration & alloc encoder
	mParam.i_csp = X264_CSP_I420;
	mParam.i_width = size_cast<int>(Meta.GetWidth());
	mParam.i_height = size_cast<int>(Meta.GetHeight());
	mParam.b_vfr_input = 0;
	mParam.b_repeat_headers = 1;
	mParam.b_annexb = 1;
	mParam.p_log_private = reinterpret_cast<void*>(&X264::Log);
	mParam.i_log_level = X264_LOG_INFO;

	auto Profile = "baseline";
	auto Result = x264_param_apply_profile(&mParam, Profile);
	IsOkay(Result, "x264_param_apply_profile");

	Result = x264_picture_alloc(&mPicture, mParam.i_csp, mParam.i_width, mParam.i_height);
	IsOkay(Result, "x264_picture_alloc");

	mHandle = x264_encoder_open(&mParam);
	if (!mHandle)
		throw Soy::AssertException("Failed to open x264 encoder");

	mPixelMeta = Meta;
}

void X264::TInstance::PushFrame(const SoyPixelsImpl& Pixels,int32_t FrameTime)
{
	AllocEncoder(Pixels.GetMeta());

	//	need planes
	SoyPixels YuvPixels(Pixels);
	YuvPixels.SetFormat(SoyPixelsFormat::Yuv_8_8_8_Ntsc);
	BufferArray<std::shared_ptr<SoyPixelsImpl>, 3> Planes;
	YuvPixels.SplitPlanes(GetArrayBridge(Planes));

	auto& LumaPlane = *Planes[0];
	auto& ChromaUPlane = *Planes[1];
	auto& ChromaVPlane = *Planes[2];

	//	checks from example code https://github.com/jesselegg/x264/blob/master/example.c
	//	gr: look for proper validation funcs
	auto Width = Pixels.GetWidth();
	auto Height = Pixels.GetHeight();
	int LumaSize = Width * Height;
	int ChromaSize = LumaSize / 4;
	int ExpectedBufferSizes[] = { LumaSize, ChromaSize, ChromaSize };

	for (auto i = 0; i < 3; i++)
	{
		auto* OutPlane = mPicture.img.plane[i];
		auto& InPlane = *Planes[i];
		auto& InPlaneArray = InPlane.GetPixelsArray();
		auto OutSize = ExpectedBufferSizes[i];
		auto InSize = InPlaneArray.GetDataSize();
		if (OutSize != InSize)
		{
			std::stringstream Error;
			Error << "Copying plane " << i << " for x264, but plane size mismatch " << InSize << " != " << OutSize;
			throw Soy_AssertException(Error);
		}
		memcpy(OutPlane, InPlaneArray.GetArray(), InSize );
	}

	mPicture.i_pts = FrameTime;
	x264_picture_t OutputPicture;

	//	gr: currently, decoder needs to have nal packets split
	auto OnNalPacket = [&](FixedRemoteArray<uint8_t>& Data)
	{
		TPacket OutputPacket;
		OutputPacket.mData.reset(new Array<uint8_t>());
		OutputPacket.mTime = FrameTime;
		OutputPacket.mData->PushBackArray(Data);
		OnOutputPacket(OutputPacket);
	};

	auto Encode = [&](x264_picture_t* InputPicture)
	{
		x264_nal_t* Nals = nullptr;
		int NalCount = 0;

		auto FrameSize = x264_encoder_encode(mHandle, &Nals, &NalCount, InputPicture, &OutputPicture);
		if (FrameSize < 0)
			throw Soy::AssertException("x264_encoder_encode error");

		//	processed, but no data output
		if (FrameSize == 0)
			return;

		//	process each nal
		auto TotalNalSize = 0;
		for (auto n = 0; n < NalCount; n++)
		{
			auto& Nal = Nals[n];
			auto NalSize = Nal.i_payload;
			auto PacketArray = GetRemoteArray(Nal.p_payload, NalSize);
			OnNalPacket(PacketArray);
			TotalNalSize += NalSize;
		}
		if (TotalNalSize != FrameSize)
			throw Soy::AssertException("NALs output size doesn't match frame size");
	};
	Encode(&mPicture);

	//	flush any other frames
	//	gr: this is supposed to only be called at the end of the stream...
	//		if DelayedFrameCount non zero, we may haveto call multiple times before nal size is >0
	//		so just keep calling until we get 0
	//	maybe add a safety iteration check
	//	gr: need this on OSX (latest x264) but on windows (old build) every subsequent frame fails
	//	gr: this was backwards? brew (old 2917) DID need to flush?
	if (X264_REV < 2969)
	{
		while (true)
		{
			auto DelayedFrameCount = x264_encoder_delayed_frames(mHandle);
			if (DelayedFrameCount == 0)
				break;

			Encode(nullptr);
		}
	}

	//	output the final part as one packet for the time
	//OnOutputPacket(OutputPacket);
}


void X264::TInstance::OnOutputPacket(const TPacket& Packet)
{
	{
		std::lock_guard<std::mutex> Lock(mPacketsLock);
		mPackets.PushBack(Packet);
	}

	if (mOnOutputPacket)
		mOnOutputPacket();
}


X264::TPacket X264::TInstance::PopPacket()
{
	std::lock_guard<std::mutex> Lock(mPacketsLock);
	if (mPackets.IsEmpty())
		return TPacket();

	auto Packet = mPackets.PopAt(0);
	return Packet;
}

