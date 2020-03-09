#include "TApiMedia.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoyFilesystem.h"
#include "SoyLib/src/SoyMedia.h"
#include "MagicEnum/include/magic_enum.hpp"
#include "Json11/json11.hpp"


//	video decoding
#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "Libs/PopH264_Osx.framework/Headers/PopH264DecoderInstance.h"
#include "Libs/PopH264_Osx.framework/Headers/PopH264.h"
#elif defined(TARGET_WINDOWS)
#pragma comment(lib,"PopH264.lib")
#include "Libs/PopH264/PopH264.h"
#endif

#if defined(TARGET_WINDOWS)
#include "Soylib/src/SoyRuntimeLibrary.h"
#endif

#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "Libs/PopCameraDeviceFramework.framework/Headers/PopCameraDevice.h"
#elif defined(TARGET_WINDOWS)
#pragma comment(lib,"PopCameraDevice.lib")
#include "Libs/PopCameraDevice/PopCameraDevice.h"
#endif


//	video capture
namespace PopCameraDevice
{
	//	put C funcs into namespace
	void	LoadDll();
}

namespace PopH264
{
	void	LoadDll();
}


class TNoFrameException : public std::exception
{
public:
	virtual const char* what() const __noexcept { return "No new frame"; }
};


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

	int		GetColourSpace(SoyPixelsFormat::Type Format);

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
	TInstance(const std::string& Name,const std::string& Format,std::function<void()> OnNewFrame);
	~TInstance();

	TFrame					PopLastFrame();

protected:
	int32_t					mHandle = 0;
	std::function<void()>	mOnNewFrame;
};



class PopH264::TInstance
{
public:
	TInstance();
	~TInstance();

	TDecoderInstance&	GetDecoder();
	void				PushData(ArrayBridge<uint8_t>&& Data, int32_t FrameNumber);
	PopCameraDevice::TFrame	PopLastFrame(bool SplitPlanes);	//	find a way to re-use the camera version

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
	void			FlushFrames();
	TPacket			PopPacket();
	bool			HasPackets() {	return !mPackets.IsEmpty();	}
	std::string		GetVersion() const;

private:
	void			AllocEncoder(const SoyPixelsMeta& Meta);
	void			OnOutputPacket(const TPacket& Packet);

	void			Encode(x264_picture_t* InputPicture);

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
	DEFINE_BIND_FUNCTIONNAME(EncodeFinished);
	DEFINE_BIND_FUNCTIONNAME(WaitForNextPacket);

	const char FrameTimestampKey[] = "Time";

	void	EnumDevices(Bind::TCallback& Params);

	Bind::TObject	FrameToObject(Bind::TLocalContext& Context, PopCameraDevice::TFrame& Frame);
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

	PopCameraDevice::LoadDll();

	auto DoEnumDevices = [&]
	{
		auto& LocalContext = Params.mLocalContext;
		try
		{
			//	we now return the json directly
			Array<char> JsonBuffer;
			JsonBuffer.SetSize(2000);
			PopCameraDevice_EnumCameraDevicesJson(JsonBuffer.GetArray(), JsonBuffer.GetDataSize());

			std::string Json(JsonBuffer.GetArray());
			auto Object = Bind::ParseObjectString(LocalContext.mLocalContext, Json);
			Promise.Resolve( LocalContext, Object);
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
			
			//	queue the error callback
			std::string ExceptionString(e.what());
			Promise.Reject( LocalContext, ExceptionString );
		}
	};
	
	//	immediate... if this is slow, put it on a thread
	DoEnumDevices();
	
	Params.Return(Promise);
}


void PopH264::LoadDll()
{
#if defined(TARGET_WINDOWS)
	//	current bodge
	static std::shared_ptr<Soy::TRuntimeLibrary> Dll;
	if (Dll)
		return;
	const char* Filename = "PopH264.dll";
	Dll.reset(new Soy::TRuntimeLibrary(Filename));
#endif
}


void TAvcDecoderWrapper::Construct(Bind::TCallback& Params)
{
	if (!Params.IsArgumentUndefined(0))
		mSplitPlanes = Params.GetArgumentBool(0);

	mDecoder.reset( new PopH264::TInstance );
	std::string ThreadName("TAvcDecoderWrapper::DecoderThread");
	mDecoderThread.reset( new SoyWorkerJobThread(ThreadName) );
	mDecoderThread->Start();
}

void TAvcDecoderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::Decode>(&TAvcDecoderWrapper::Decode );
	Template.BindFunction<ApiMedia::BindFunction::WaitForNextFrame>(&TAvcDecoderWrapper::WaitForNextFrame);
}

void TAvcDecoderWrapper::Decode(Bind::TCallback& Params)
{
	auto& This = Params.This<TAvcDecoderWrapper>();

	std::shared_ptr< Array<uint8_t>> pPacketBytes(new Array<uint8_t>());
	Params.GetArgumentArray(0, GetArrayBridge(*pPacketBytes));

	//	push data onto thread
	auto Decode = [this, pPacketBytes]() mutable
	{
		try
		{
			auto& Decoder = *mDecoder;
			auto& PacketBytes = *pPacketBytes;
			Decoder.PushData(GetArrayBridge(PacketBytes), 0);
			this->OnNewFrame();
		}
		catch (TNoFrameException&e)
		{
			//	OnNewFrame doesn't have a frame yet
			//std::Debug << __PRETTY_FUNCTION__ << " no frame yet" << std::endl;
		}
		catch (std::exception& e)
		{
			this->OnError(e.what());
		}
	};

	//	on decoder thread, run decode
	This.mDecoderThread->PushJob(Decode);
}

void TAvcDecoderWrapper::FlushPendingFrames()
{
	if (mFrames.IsEmpty() && mErrors.IsEmpty())
		return;
	if (!mFrameRequests.HasPromises())
		return;

	auto Flush = [this](Bind::TLocalContext& Context) mutable
	{
		Soy::TScopeTimerPrint Timer("TPopCameraDeviceWrapper::FlushPendingFrames::Flush", 3);

		//	pop frames before errors
		PopCameraDevice::TFrame PoppedFrame;
		std::string PoppedError;
		{
			std::lock_guard<std::mutex> Lock(mFramesLock);
			//	gr: sometimes, probbaly because there's no mutex on mFrames.IsEmpty() above
			//		we get have an empty mFrames (this lambda is probably executing) and we have zero frames, 
			//		abort the resolve
			if (mFrames.IsEmpty() && mErrors.IsEmpty() )
				return;

			if ( !mFrames.IsEmpty() )
			{
				PoppedFrame = mFrames.PopAt(0);
			}
			else
			{
				PoppedError = mErrors.PopAt(0);
			}
		}

		if (PoppedError.empty())
		{
			auto FrameObject = ApiMedia::FrameToObject(Context, PoppedFrame);
			mFrameRequests.Resolve(FrameObject);
		}
		else
		{
			mFrameRequests.Reject(PoppedError);
		}
	};
	auto& Context = mFrameRequests.GetContext();
	Context.Queue(Flush);
}

void TAvcDecoderWrapper::OnNewFrame()
{
	//	pop frame out
	auto Frame = mDecoder->PopLastFrame(mSplitPlanes);
	{
		std::lock_guard<std::mutex> Lock(mFramesLock);
		mFrames.PushBack(Frame);
		mErrors.Clear();
	}
	FlushPendingFrames();
}

void TAvcDecoderWrapper::OnError(const std::string& Error)
{
	{
		std::lock_guard<std::mutex> Lock(mFramesLock);
		mErrors.PushBack(Error);
	}
	FlushPendingFrames();
}

void TAvcDecoderWrapper::WaitForNextFrame(Bind::TCallback& Params)
{
	auto Promise = mFrameRequests.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	FlushPendingFrames();
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


PopCameraDevice::TInstance::TInstance(const std::string& Name,const std::string& Format, std::function<void()> OnNewFrame) :
	mOnNewFrame	( OnNewFrame )
{
	char ErrorBuffer[1000] = { 0 };
	mHandle = PopCameraDevice_CreateCameraDeviceWithFormat(Name.c_str(), Format.c_str(), ErrorBuffer, std::size(ErrorBuffer));

	if ( mHandle <= 0 )
	{
		std::string ErrorStr(ErrorBuffer);
		std::stringstream Error;
		Error << "Failed to create PopCameraDevice named " << Name << " (Handle=" << mHandle << "). Error: " << ErrorStr;
		throw Soy::AssertException(Error.str());
	}

	PopCameraDevice_OnNewFrame* OnNewFrameCallback = [](void* Meta)
	{
		auto* This = static_cast<PopCameraDevice::TInstance*>(Meta);
		auto OnNewFrame = This->mOnNewFrame;
		if (OnNewFrame)
			OnNewFrame();
		else
			std::Debug << "New frame ready" << std::endl;
	};

	PopCameraDevice_AddOnNewFrameCallback(mHandle, OnNewFrameCallback,this);
}


PopCameraDevice::TInstance::~TInstance()
{
	PopCameraDevice_FreeCameraDevice(mHandle);
}


void GetPixelMetasFromJson(ArrayBridge<SoyPixelsMeta>&& Metas, const std::string& JsonString)
{
	//std::Debug << __PRETTY_FUNCTION__ << "(" << JsonString << std::endl;
	std::string Error;
	auto JsonObject = json11::Json::parse(JsonString, Error);
	if (JsonObject == json11::Json())
		throw Soy::AssertException(std::string("JSON parse error: ") + Error);

	auto JsonPlanesNode = JsonObject["Planes"];
	if (!JsonPlanesNode.is_array())
		throw Soy::AssertException(std::string("Expecting Frame Meta JSON .Planes to be an array;") + JsonString);

	auto& JsonPlanes = JsonPlanesNode.array_items();
	auto ParsePlane = [&](const json11::Json& PlaneObject)
	{
		auto Width = PlaneObject["Width"].number_value();
		auto Height = PlaneObject["Height"].number_value();
		auto Channels = PlaneObject["Channels"].number_value();
		auto DataSize = PlaneObject["DataSize"].number_value();
		auto FormatString = PlaneObject["Format"].string_value();
		auto Format = SoyPixelsFormat::ToType(FormatString);
		SoyPixelsMeta Meta(Width, Height, Format);
		auto MetaSize = Meta.GetDataSize();
		if (MetaSize != DataSize)
		{
			std::stringstream Error;
			Error << "Meta size (" << MetaSize << "; " << Meta << ") doesn't match dictated plane size " << DataSize << ". Change code to pass plane size";
			throw Soy::AssertException(Error);
		}
		Metas.PushBack(Meta);
	};
	for (auto& PlaneObject : JsonPlanes)
	{
		ParsePlane(PlaneObject);
	}
}

PopCameraDevice::TFrame PopCameraDevice::TInstance::PopLastFrame()
{
	PopCameraDevice::TFrame Frame;
	Frame.mTime = SoyTime(true);


	Array<char> JsonBuffer(2000);
	//	gr: this is json now, so we need a good way to extract what we need...
	auto NextFrameNumber = PopCameraDevice_PeekNextFrame(mHandle, JsonBuffer.GetArray(), JsonBuffer.GetDataSize());
	if (NextFrameNumber < 0)
		throw TNoFrameException();

	std::string Json(JsonBuffer.GetArray());

	//	get plane count
	BufferArray<SoyPixelsMeta, 4> PlaneMetas;
	GetPixelMetasFromJson(GetArrayBridge(PlaneMetas), Json);
	auto PlaneCount = PlaneMetas.GetSize();

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
		auto& Meta = PlaneMetas[i];
		if ( !Meta.IsValid() )
			continue;

		auto& Plane = AllocPlane();
		Plane.reset(new SoyPixels(Meta));
		auto& Array = Plane->GetPixelsArray();
		PlanePixelsBytes.PushBack(Array.GetArray());
		PlanePixelsByteSize.PushBack(Array.GetDataSize());
	}

	while ( PlanePixelsBytes.GetSize() < 3 )
	{
		PlanePixelsBytes.PushBack(nullptr);
		PlanePixelsByteSize.PushBack(0);
	}
	
	char FrameMeta[1000] = { 0 };

	auto NewFrameTime = PopCameraDevice_PopNextFrame(
		mHandle,
		nullptr,	//	json buffer
		0,
		PlanePixelsBytes[0], PlanePixelsByteSize[0],
		PlanePixelsBytes[1], PlanePixelsByteSize[1],
		PlanePixelsBytes[2], PlanePixelsByteSize[2]
	);

	//	no new frame
	if (NewFrameTime < 0)
		throw Soy::AssertException("New frame post-peek invalid");

	return Frame;
}




void TPopCameraDeviceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
		
	//	param 1 is format so we can specify format, streamindex, depth, rgb etc etc
	std::string Format;
	if (!Params.IsArgumentUndefined(1))
	{
		Format = Params.GetArgumentString(1);
	}

	if (!Params.IsArgumentUndefined(2))
	{
		mOnlyLatestFrame = Params.GetArgumentBool(2);
	}

	//	todo: frame buffer [plane]pool
	auto OnNewFrame = std::bind(&TPopCameraDeviceWrapper::OnNewFrame, this);
	mInstance.reset(new PopCameraDevice::TInstance(DeviceName, Format,OnNewFrame));
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


Bind::TObject ApiMedia::FrameToObject(Bind::TLocalContext& Context,PopCameraDevice::TFrame& Frame)
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
			//	gr: sometimes, probbaly because there's no mutex on mFrames.IsEmpty() above
			//		we get have an empty mFrames (this lambda is probably executing) and we have zero frames, 
			//		abort the resolve
			if (mFrames.IsEmpty())
				return;

			if (mOnlyLatestFrame)
			{
				PoppedFrame = mFrames.PopBack();
				mFrames.Clear(false);
			}
			else
			{
				PoppedFrame = mFrames.PopAt(0);
			}
		}
		auto FrameObject = ApiMedia::FrameToObject(Context,PoppedFrame);
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
	PopH264::LoadDll();
	
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

PopCameraDevice::TFrame PopH264::TInstance::PopLastFrame(bool SplitPlanes)
{
	PopCameraDevice::TFrame Frame;
	Frame.mTime = SoyTime(true);
		

	Array<char> JsonBuffer(2000);
	//	gr: this is json now, so we need a good way to extract what we need...
	PopH264_PeekFrame(mHandle, JsonBuffer.GetArray(), JsonBuffer.GetDataSize());
	//if (NextFrameNumber < 0)
	//	throw TNoFrameException();

	std::string Json(JsonBuffer.GetArray());

	//	get plane count
	BufferArray<SoyPixelsMeta, 4> PlaneMetas;
	GetPixelMetasFromJson(GetArrayBridge(PlaneMetas), Json);
	auto PlaneCount = PlaneMetas.GetSize();

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
	for (auto i = 0; i < PlaneCount; i++)
	{
		auto& Meta = PlaneMetas[i];
		if (!Meta.IsValid())
			continue;

		auto& Plane = AllocPlane();
		Plane.reset(new SoyPixels(Meta));
		auto& Array = Plane->GetPixelsArray();
		PlanePixelsBytes.PushBack(Array.GetArray());
		PlanePixelsByteSize.PushBack(Array.GetDataSize());
	}

	while (PlanePixelsBytes.GetSize() < 3)
	{
		PlanePixelsBytes.PushBack(nullptr);
		PlanePixelsByteSize.PushBack(0);
	}

	char FrameMeta[1000] = { 0 };
	
	auto NewFrameTime = PopH264_PopFrame(
		mHandle,
		PlanePixelsBytes[0], PlanePixelsByteSize[0],
		PlanePixelsBytes[1], PlanePixelsByteSize[1],
		PlanePixelsBytes[2], PlanePixelsByteSize[2]
	);

	//	no new frame
	if (NewFrameTime < 0)
		throw TNoFrameException();
		//throw Soy::AssertException("New frame post-peek invalid");

	//	rejoin planes when possible
	//	gr: we should be able to make a joined format before copying from plugin
	//		and pass SoyPixelRemote buffers
	if (!SplitPlanes && PlaneCount>1)
	{
		try
		{
			if (PlaneCount == 2)
				Frame.mPlane0->AppendPlane(*Frame.mPlane1);
			else if (PlaneCount == 3)
				Frame.mPlane0->AppendPlane(*Frame.mPlane1, *Frame.mPlane2);
			Frame.mPlane1 = nullptr;
			Frame.mPlane2 = nullptr;
		}
		catch (std::exception& e)
		{
			std::Debug << "Failed to get merged format " << e.what() << std::endl;
		}
	}

	return Frame;
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
	Template.BindFunction<ApiMedia::BindFunction::EncodeFinished>(&TH264EncoderWrapper::EncodeFinished);
	Template.BindFunction<ApiMedia::BindFunction::WaitForNextPacket>(&TH264EncoderWrapper::WaitForNextPacket);
}

void TH264EncoderWrapper::Encode(Bind::TCallback& Params)
{
	auto& Frame = Params.GetArgumentPointer<TImageWrapper>(0);
	auto FrameTime = Params.GetArgumentInt(1);

	if (mEncoderThread)
	{
		std::shared_ptr<SoyPixels> PixelCopy( new SoyPixels() );
		{
			Soy::TScopeTimerPrint Timer("Copy pixels for thread", 2);
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

void TH264EncoderWrapper::EncodeFinished(Bind::TCallback& Params)
{
	if (mEncoderThread)
	{
		auto Encode = [=]()mutable
		{
			mEncoder->FlushFrames();
		};
		mEncoderThread->PushJob(Encode);
	}
	else
	{
		mEncoder->FlushFrames();
	}
}

void TH264EncoderWrapper::WaitForNextPacket(Bind::TCallback& Params)
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

	if ( !mEncoder->HasPackets())
		return;
	
	auto Resolve = [this](Bind::TLocalContext& Context)
	{
		//	gr: we're hitting race conditions here, pop packets ONLY AS we're resolving
		auto Flush = [this](Bind::TLocalContext& Context, Bind::TPromise& Promise)
		{
			auto NextPacket = mEncoder->PopPacket();
			auto Packet = Context.mGlobalContext.CreateObjectInstance(Context);
			Packet.SetInt("Time", NextPacket.mTime);
			auto Data = NextPacket.mData;
			if (Data)
			{
				Packet.SetArray("Data", GetArrayBridge(*Data));
				std::Debug << "Resolving packet (" << (Data->GetSize()) << ")" << std::endl;
			}
			else
			{
				std::Debug << "Resolving packet (null) hit race condition?" << std::endl;
			}
			Promise.Resolve(Context,Packet);
		};
		mNextPacketPromises.Flush(Flush);
	};
	auto& Context = mNextPacketPromises.GetContext();
	Context.Queue(Resolve);
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

int X264::GetColourSpace(SoyPixelsFormat::Type Format)
{
	switch (Format)
	{
	case SoyPixelsFormat::Yuv_8_8_8_Ntsc:	return X264_CSP_I420;
	//case SoyPixelsFormat::Greyscale:		return X264_CSP_I400;
	//case SoyPixelsFormat::Yuv_8_88_Ntsc:	return X264_CSP_NV12;
	//case SoyPixelsFormat::Yvu_8_88_Ntsc:	return X264_CSP_NV21;
	//case SoyPixelsFormat::Yvu_844_Ntsc:		return X264_CSP_NV16;
	//	these are not supported by x264
	//case SoyPixelsFormat::BGR:		return X264_CSP_BGR;
	//case SoyPixelsFormat::BGRA:		return X264_CSP_BGRA;
	//case SoyPixelsFormat::RGB:		return X264_CSP_BGR;
	}

	std::stringstream Error;
	Error << "X264::GetColourSpace unsupported format " << Format;
	throw Soy::AssertException(Error);
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
	mParam.i_csp = GetColourSpace(Meta.GetFormat());
	mParam.i_width = size_cast<int>(Meta.GetWidth());
	mParam.i_height = size_cast<int>(Meta.GetHeight());
	mParam.b_vfr_input = 0;
	mParam.b_repeat_headers = 1;
	mParam.b_annexb = 1;
	mParam.p_log_private = reinterpret_cast<void*>(&X264::Log);
	mParam.i_log_level = X264_LOG_DEBUG;

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
	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__, 2);
	AllocEncoder(Pixels.GetMeta());

	//	need planes
	auto& YuvPixels = Pixels;
	//YuvPixels.SetFormat(SoyPixelsFormat::Yuv_8_8_8_Ntsc);
	BufferArray<std::shared_ptr<SoyPixelsImpl>, 3> Planes;
	YuvPixels.SplitPlanes(GetArrayBridge(Planes));

	//auto& LumaPlane = *Planes[0];
	//auto& ChromaUPlane = *Planes[1];
	//auto& ChromaVPlane = *Planes[2];

	//	checks from example code https://github.com/jesselegg/x264/blob/master/example.c
	//	gr: look for proper validation funcs
	auto Width = Pixels.GetWidth();
	auto Height = Pixels.GetHeight();
	int LumaSize = Width * Height;
	int ChromaSize = LumaSize / 4;
	int ExpectedBufferSizes[] = { LumaSize, ChromaSize, ChromaSize };

	for (auto i = 0; i < Planes.GetSize(); i++)
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
		FlushFrames();
		
	}
}


void X264::TInstance::FlushFrames()
{
	//	when we're done with frames, we need to make the encoder flush out any more packets
	int Safety = 1000;
	while (--Safety > 0)
	{
		auto DelayedFrameCount = x264_encoder_delayed_frames(mHandle);
		if (DelayedFrameCount == 0)
			break;

		Encode(nullptr);
	}
}


void X264::TInstance::Encode(x264_picture_t* InputPicture)
{
	//	we're assuming here mPicture has been setup, or we're flushing

//	gr: currently, decoder NEEDS to have nal packets split
	auto OnNalPacket = [&](FixedRemoteArray<uint8_t>& Data)
	{
		auto FrameTime = mPicture.i_pts;

		TPacket OutputPacket;
		OutputPacket.mData.reset(new Array<uint8_t>());
		OutputPacket.mTime = FrameTime;
		OutputPacket.mData->PushBackArray(Data);
		OnOutputPacket(OutputPacket);
	};

	x264_picture_t OutputPicture;
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
		throw TNoFrameException();

	auto Packet = mPackets.PopAt(0);
	return Packet;
}

