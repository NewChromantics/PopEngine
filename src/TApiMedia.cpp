#include "TApiMedia.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoyFilesystem.h"
#include "SoyLib/src/SoyMedia.h"
#include "MagicEnum/include/magic_enum.hpp"


//	video decoding
#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "Libs/PopH264_Osx.framework/Headers/PopH264DecoderInstance.h"
#include "Libs/PopH264_Osx.framework/Headers/PopH264.h"
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

#if defined(TARGET_IOS)
int     x264_encoder_encode( x264_t *, x264_nal_t **pp_nal, int *pi_nal, x264_picture_t *pic_in, x264_picture_t *pic_out )
{
	return 0;
}
void    x264_encoder_close( x264_t * )	{}
int     x264_encoder_delayed_frames( x264_t * ){	return 0;	}
int     x264_param_default_preset( x264_param_t *, const char *preset, const char *tune )
{
	return 0;
}
int     x264_param_apply_profile( x264_param_t *, const char *profile )
{
	return 0;
}
int x264_picture_alloc( x264_picture_t *pic, int i_csp, int i_width, int i_height )
{
	return 0;
}
x264_t *x264_encoder_open_155( x264_param_t * )
{
	return nullptr;
}

#endif


class PopCameraDevice::TInstance
{
public:
	TInstance(const std::string& Name,std::function<void()> OnNewFrame);
	~TInstance();

	TFrame			PopLastFrame();

protected:
	TDevice&		GetDevice();

protected:
	int32_t			mHandle = 0;
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
	DEFINE_BIND_FUNCTIONNAME(WaitForNextFrame);
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
	PopCameraDevice_EnumCameraDevices(DeviceNamesBuffer, std::size(DeviceNamesBuffer));

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



PopCameraDevice::TInstance::TInstance(const std::string& Name, std::function<void()> OnNewFrame)
{
	mHandle = PopCameraDevice_CreateCameraDevice(Name.c_str());
	if ( mHandle <= 0 )
	{
		std::stringstream Error;
		Error << "Failed to create PopCameraDevice named " << Name << ". Error: " << mHandle;
		throw Soy::AssertException(Error.str());
	}

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


PopCameraDevice::TFrame PopCameraDevice::TInstance::PopLastFrame()
{
	PopCameraDevice::TFrame Frame;
	Frame.mTime = SoyTime(true);

	//	this should throw if the device has gone
	auto& Device = GetDevice();

	//	get meta so we know what buffers to allocate
	const int MetaValuesSize = 100;
	int MetaValues[MetaValuesSize];
	PopCameraDevice_GetMeta(mHandle, MetaValues, MetaValuesSize);
	
	//	dont have meta yet
	auto PlaneCount = MetaValues[MetaIndex::PlaneCount];
	if (PlaneCount == 0)
		throw Soy::AssertException("PopCameraDevice_GetMeta 0 planes");

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

	auto AllocPlane = [&]()->std::shared_ptr<SoyPixelsImpl>&
	{
		//auto& Plane = Frame.mPlanes.PushBack();
		if (!Frame.mPlane0)
			return Frame.mPlane0;
		if (!Frame.mPlane1)
			return Frame.mPlane1;
		if (!Frame.mPlane2)
			return Frame.mPlane2;
		return Frame.mPlane3;
	};
	//auto& PlanePixels = Frame.mPlanes;
	Array<uint8_t*> PlanePixelsBytes;
	Array<size_t> PlanePixelsByteSize;
	for ( auto i=0;	i<PlaneCount;	i++ )
	{
		auto& Meta = PlaneMeta[i];
		if ( !Meta.IsValid() )
			continue;

		auto& Plane = AllocPlane();
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
		std::size(FrameMeta)
	);

	//	no new frame
	if (Result == 0)
		throw Soy::AssertException("No new frame");

	return Frame;
}




void TPopCameraDeviceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
	
	//	param 1 is format so we can specify format, streamindex, depth, rgb etc etc
	auto Format = SoyPixelsFormat::Invalid;
	if (!Params.IsArgumentUndefined(1))
	{
		auto FormatString = Params.GetArgumentString(1);
		Format = magic_enum::enum_cast<SoyPixelsFormat::Type>(FormatString).value();
	}

	bool OnlyLatestFrame = true;
	if (!Params.IsArgumentUndefined(2))
	{
		OnlyLatestFrame = Params.GetArgumentBool(2);
	}

	//	todo: frame buffer [plane]pool
	auto OnNewFrame = std::bind(&TPopCameraDeviceWrapper::OnNewFrame, this);
	mInstance.reset(new PopCameraDevice::TInstance(DeviceName,OnNewFrame));
}


void TPopCameraDeviceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::WaitForNextFrame>(&TPopCameraDeviceWrapper::WaitForNextFrame);
}


void TPopCameraDeviceWrapper::WaitForNextFrame(Bind::TCallback& Params)
{
	auto Promise = mFrameRequests.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	FlushPendingFrames();
}


Bind::TObject FrameToObject(Bind::TLocalContext& Context,PopCameraDevice::TFrame& Frame)
{
	auto FrameObject = Context.mGlobalContext.CreateObjectInstance(Context);
	FrameObject.SetInt("TimeMs",Frame.mTime.GetTime());

	if ( Frame.mMeta.length() )
	{
		if (Frame.mMeta[0] == '{')
		{
			//	turn string into json object
			FrameObject.SetObjectFromString("Meta", Frame.mMeta);
		}
		else
		{
			FrameObject.SetString("Meta", Frame.mMeta);
		}
	}

	
	Array<Bind::TObject> PlaneImages;
	
	auto AddImage = [&](std::shared_ptr<SoyPixelsImpl>& Pixels)
	{
		if (!Pixels)
			return;
		auto ImageObject = Context.mGlobalContext.CreateObjectInstance(Context, TImageWrapper::GetTypeName());
		auto& Image = ImageObject.This<TImageWrapper>();
		Image.mName = "Media output frame";
		Image.SetPixels(Pixels);
		PlaneImages.PushBack(ImageObject);
	};
	AddImage(Frame.mPlane0);
	AddImage(Frame.mPlane1);
	AddImage(Frame.mPlane2);
	AddImage(Frame.mPlane3);
	

	FrameObject.SetArray("Planes", GetArrayBridge(PlaneImages));

	return FrameObject;
}

void TPopCameraDeviceWrapper::FlushPendingFrames()
{
	if (mFrames.IsEmpty())
		return;
	if (!mFrameRequests.HasPromises())
		return;

	auto Flush = [this](Bind::TLocalContext& Context) mutable
	{
		Soy::TScopeTimerPrint Timer("TPopCameraDeviceWrapper::FlushPendingFrames::Flush", 3);
		
		PopCameraDevice::TFrame PoppedFrame;
		{
			std::lock_guard<std::mutex> Lock(mFramesLock);
			PoppedFrame = mFrames.PopAt(0);
		}
		auto FrameObject = FrameToObject(Context,PoppedFrame);
		mFrameRequests.Resolve(FrameObject);
	};
	auto& Context = mFrameRequests.GetContext();
	Context.Queue(Flush);
}

void TPopCameraDeviceWrapper::OnNewFrame()
{
	PopCameraDevice::TFrame Frame = mInstance->PopLastFrame();

	{
		std::lock_guard<std::mutex> Lock(mFramesLock);
		mFrames.PushBack(Frame);
	}
	FlushPendingFrames();
}



PopH264::TInstance::TInstance()
{
	//	decoder now
	auto Decoder = PopH264::Mode_Software;
	mHandle = PopH264_CreateInstance(Decoder);
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

	//	trigger dll load
#if defined(TARGET_WINDOWS)
	Soy::TRuntimeLibrary Dll("x264.dll");
#endif

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

