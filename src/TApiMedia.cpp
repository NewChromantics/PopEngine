#include "TApiMedia.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoyFilesystem.h"
#include "SoyLib/src/SoyMedia.h"
#include "MagicEnum/include/magic_enum.hpp"
#include "Json11/json11.hpp"
#include "SoyLib/src/SoyRuntimeLibrary.h"


//	video decoding and encoding
#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "Libs/PopH264_Osx.framework/Headers/PopH264.h"
#elif defined(TARGET_WINDOWS)
#pragma comment(lib,"PopH264.lib")
#include "Libs/PopH264/PopH264.h"
#endif


#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "Libs/PopCameraDevice_Osx.framework/Headers/PopCameraDevice.h"
#elif defined(TARGET_WINDOWS)
#pragma comment(lib,"PopCameraDevice.lib")
#include "Libs/PopCameraDevice/PopCameraDevice.h"
#endif


//	video capture
namespace PopCameraDevice
{
	void	LoadDll();
}

namespace PopH264
{
	void	LoadDll();
}

namespace X264
{
	class TInstance;
	class TPacket;
}


class TNoFrameException : public std::exception
{
public:
	virtual const char* what() const __noexcept { return "No new frame"; }
};


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

	void					PushData(ArrayBridge<uint8_t>&& Data, int32_t FrameNumber);
	PopCameraDevice::TFrame	PopLastFrame(bool SplitPlanes,bool OnlyLatest);	//	find a way to re-use the camera version
	size_t					GetPendingFrameCount();

protected:
	int32_t		mHandle = 0;
	int32_t		mPendingFrameCount = 0;
	
public:
	std::function<void()>	mOnFrameReady;
};


//	thread which calls a lambda when woken
class TLambdaWakeThread : public SoyWorkerThread
{
public:
	TLambdaWakeThread(const std::string& ThreadName,std::function<void()> Functor) :
		SoyWorkerThread	( ThreadName, SoyWorkerWaitMode::Wake ),
		mLambda			( Functor )
	{
		
	}
	//~TLambdaWakeThread();

	virtual bool	Iteration() override
	{
		mLambda();
		return true;
	}
	
protected:
	std::function<void()>		mLambda;
};


//	now PopH264 encoder instance
class X264::TInstance
{
public:
	TInstance(const std::string& EncoderOptionsJson);
	~TInstance();

	void			PushFrame(const SoyPixelsImpl& Pixels,const std::string& EncodeMetaJson);
	void			FlushFrames();
	bool			PopPacket(ArrayBridge<uint8_t>&& Data,std::string& MetaJson);
	bool			HasPackets();

protected:
	int32_t			mHandle = 0;

public:
	std::function<void()>	mOnPacketReady;
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
	Bind::TObject	PacketToObject(Bind::TLocalContext& Context,const ArrayBridge<uint8_t>&& Data,const std::string& MetaJson);
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
			JsonBuffer.SetSize(6000);
			PopCameraDevice_EnumCameraDevicesJson(JsonBuffer.GetArray(), JsonBuffer.GetDataSize());

			std::string Json(JsonBuffer.GetArray());
			auto Object = Bind::ParseObjectString(LocalContext.mLocalContext, Json);
			Promise.Resolve( LocalContext, Object);
		}
		catch(std::exception& e)
		{
			std::Debug << "PopCameraDevice_EnumCameraDevicesJson() " << e.what() << std::endl;
			
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
	static std::shared_ptr<Soy::TRuntimeLibrary> Dll;
	if (Dll)
		return;

	//	on OSX, if the framework is in resources, it should auto resolve symbols
#if defined(TARGET_WINDOWS)
	//	current bodge
	const char* Filename = "PopH264.dll";
	Dll.reset(new Soy::TRuntimeLibrary(Filename));
#endif
	
#if defined(TARGET_OSX)
	//	current bodge
	const char* Filename = "/Volumes/Code/Panopoly3/PopH264_Osx.framework/Versions/A/PopH264_Osx";
	Dll.reset(new Soy::TRuntimeLibrary(Filename));
#endif
}


void TAvcDecoderWrapper::Construct(Bind::TCallback& Params)
{
	if (!Params.IsArgumentUndefined(0))
		mSplitPlanes = Params.GetArgumentBool(0);

	mDecoder.reset( new PopH264::TInstance );
	mDecoder->mOnFrameReady = [this]()
	{
		this->OnNewFrame();
	};
	
	auto FlushQueuedData = [this]()
	{
		this->FlushQueuedData();
	};
	
	std::string ThreadName("TAvcDecoderWrapper::DecoderThread");
	mDecoderThread.reset( new TLambdaWakeThread(ThreadName,FlushQueuedData) );
	mDecoderThread->Start();
}

void TAvcDecoderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::Decode>(&TAvcDecoderWrapper::Decode );
	Template.BindFunction<ApiMedia::BindFunction::WaitForNextFrame>(&TAvcDecoderWrapper::WaitForNextFrame);
}



void TAvcDecoderWrapper::FlushQueuedData()
{
	try
	{
		auto pPacketBytes = PopQueuedData();
		if ( !pPacketBytes )
			return;
		
		//std::Debug << "Pushing " << pPacketBytes->GetDataSize() << " bytes to decoder" << std::endl;
		
		auto& Decoder = *mDecoder;
		auto& PacketBytes = *pPacketBytes;
		Decoder.PushData(GetArrayBridge(PacketBytes), 0);
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
}

std::shared_ptr<Array<uint8_t>> TAvcDecoderWrapper::PopQueuedData()
{
	std::lock_guard<std::mutex> Lock(mPushDataLock);
	auto Data = mPushData;
	if ( !Data )
		return nullptr;
	if ( Data->IsEmpty() )
		return nullptr;
	mPushData.reset();
	return Data;
}


void TAvcDecoderWrapper::PushQueuedData(const ArrayBridge<uint8_t>&& Data)
{
	std::lock_guard<std::mutex> Lock(mPushDataLock);
	if ( !mPushData )
		mPushData.reset( new Array<uint8_t>() );
	mPushData->PushBackArray(Data);
	//std::Debug << "Decoder queue size " << mPushData->GetDataSize() << " bytes" << std::endl;
}
	
void TAvcDecoderWrapper::Decode(Bind::TCallback& Params)
{
	Array<uint8_t> PacketBytes;
	Params.GetArgumentArray(0, GetArrayBridge(PacketBytes));

/*
	auto DecoderJobCount = mDecoderThread->GetJobCount();
	if ( DecoderJobCount > 0 )
		std::Debug << "Decoder job queue size " << DecoderJobCount << std::endl;
*/
	//	queue up data
	PushQueuedData(GetArrayBridge(PacketBytes));

	//	queue up a pop&decode
	mDecoderThread->Wake();
	//mDecoderThread->PushJob(Decode);
}

void TAvcDecoderWrapper::FlushPendingFrames()
{
	if (!mFrameRequests.HasPromises())
		return;

	//auto HasFrames = !mFrames.IsEmpty();
	auto HasFrames = mDecoder->GetPendingFrameCount() > 0;
	if (!HasFrames && mErrors.IsEmpty())
		return;


	
	auto Flush = [this](Bind::TLocalContext& Context) mutable
	{
		Soy::TScopeTimerPrint Timer("TAvcDecoderWrapper::FlushPendingFrames::Flush", 5);
		
		//	gr: ideally this is on its own thread so as not to block JS or the decoder
		//	defer this until flush so we can grab latest and not stall decoder's decode thread
		//	gr: todo: pop all
		try
		{
			Soy::TScopeTimerPrint Timer2("TAvcDecoderWrapper::FlushPendingFrames::pop", 5);
			auto Frame = mDecoder->PopLastFrame(mSplitPlanes,mOnlyLatest);
			{
				std::lock_guard<std::mutex> Lock(mFramesLock);
				mFrames.PushBack(Frame);
				mErrors.Clear();
			}
		}
		catch(TNoFrameException& e)
		{
			//	supress this error
		}
		
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
			mFrameRequests.Resolve(Context,FrameObject);
		}
		else
		{
			mFrameRequests.Reject(Context,PoppedError);
		}
	};
	auto& Context = mFrameRequests.GetContext();
	Context.Queue(Flush);
}

void TAvcDecoderWrapper::OnNewFrame()
{
	//	defer this until flush so we can grab latest and not stall decoder's decode thread
	/*
	//	pop frame out
	auto Frame = mDecoder->PopLastFrame(mSplitPlanes);
	{
		std::lock_guard<std::mutex> Lock(mFramesLock);
		mFrames.PushBack(Frame);
		mErrors.Clear();
	}*/
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
	//	on OSX, if the framework is in resources, it should auto resolve symbols
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
	auto NextFrameNumberSigned = PopCameraDevice_PeekNextFrame(mHandle, JsonBuffer.GetArray(), JsonBuffer.GetDataSize());
	if (NextFrameNumberSigned == -1 )
		throw TNoFrameException();
	auto NextFrameNumber = static_cast<uint32_t>(NextFrameNumberSigned);

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
	if (NewFrameTime == -1)
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
	//	alloc key names once
	static std::string _TimeMs = "TimeMs";
	static std::string _Meta = "Meta";
	static std::string _Planes = "Planes";

	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__,3);
	auto FrameObject = Context.mGlobalContext.CreateObjectInstance(Context);
	FrameObject.SetInt(_TimeMs,Frame.mTime.GetTime());

	if ( Frame.mMeta.length() )
	{
		if (Frame.mMeta[0] == '{')
		{
			//	turn string into json object
			FrameObject.SetObjectFromString(_Meta, Frame.mMeta);
		}
		else
		{
			FrameObject.SetString(_Meta, Frame.mMeta);
		}
	}

	
	BufferArray<Bind::TObject,4> PlaneImages;
	
	auto AddImage = [&](std::shared_ptr<SoyPixelsImpl>& Pixels)
	{
		if (!Pixels)
			return;
		auto ImageObject = Context.mGlobalContext.CreateObjectInstance(Context, TImageWrapper::GetTypeName());
		auto& Image = ImageObject.This<TImageWrapper>();
		//Image.mName = "Media output frame";
		Image.SetPixels(Pixels);
		PlaneImages.PushBack(ImageObject);
	};
	AddImage(Frame.mPlane0);
	AddImage(Frame.mPlane1);
	AddImage(Frame.mPlane2);
	AddImage(Frame.mPlane3);
	

	FrameObject.SetArray(_Planes, GetArrayBridge(PlaneImages));

	return FrameObject;
}

Bind::TObject ApiMedia::PacketToObject(Bind::TLocalContext& Context,const ArrayBridge<uint8_t>&& Data,const std::string& MetaJson)
{
	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__,3);
	auto FrameObject = Context.mGlobalContext.CreateObjectInstance(Context);
	
	//	convert meta json to an object
	auto MetaObject = Bind::ParseObjectString( Context.mLocalContext, MetaJson );
	
	FrameObject.SetArray("Data",Data);
	FrameObject.SetObject("Meta", MetaObject);
	
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
		//	gr: this flush is expensive because of the work done AFTERwards (encoding!)
		Soy::TScopeTimerPrint Timer("TPopCameraDeviceWrapper::FlushPendingFrames::Flush", 60);
		
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
		mFrameRequests.Resolve(Context,FrameObject);
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
	
	auto Decoder = POPH264_DECODERMODE_SOFTWARE;
	mHandle = PopH264_CreateInstance(Decoder);
	if ( mHandle <= 0 )
	{
		std::stringstream Error;
		Error << "Failed to create PopH264 instance. Error=" << mHandle;
		throw Soy::AssertException(Error.str());
	}

	mOnFrameReady = []()
	{
		std::Debug << "Decoded frame ready (no callback assigned)" << std::endl;
	};
	
	auto OnFrameReady = [](void* pThis)
	{
		auto* This = reinterpret_cast<TInstance*>(pThis);
		This->mPendingFrameCount++;
		This->mOnFrameReady();
	};
	PopH264_DecoderAddOnNewFrameCallback( mHandle, OnFrameReady, this );
}


PopH264::TInstance::~TInstance()
{
	PopH264_DestroyInstance(mHandle);
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

size_t PopH264::TInstance::GetPendingFrameCount()
{
	return mPendingFrameCount;
}

PopCameraDevice::TFrame PopH264::TInstance::PopLastFrame(bool SplitPlanes,bool ONlyLatest)
{
	PopCameraDevice::TFrame Frame;
	Frame.mTime = SoyTime(true);
		

	char JsonBuffer[1000] = {0};
	//	gr: this is json now, so we need a good way to extract what we need...
	PopH264_PeekFrame(mHandle, JsonBuffer, std::size(JsonBuffer) );
	//if (NextFrameNumber < 0)
	//	throw TNoFrameException();
	std::string Json(JsonBuffer);

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

	mPendingFrameCount--;
	
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
	std::string OptionsJson;
	
	//	backwards compatibility
	if ( Params.IsArgumentNumber(0) )
	{
		auto Preset = Params.GetArgumentInt(0);
		std::stringstream Json;
		Json << "{\"Quality\":" << Preset << "}";
		OptionsJson = Json.str();
		std::Debug << "H264 encoder deprecated constructor. Param0 should now be " << OptionsJson << std::endl;
	}
	else if ( !Params.IsArgumentUndefined(0) )
	{
		auto OptionsObject = Params.GetArgumentObject(0);
		OptionsJson = Bind::StringifyObject( Params.mLocalContext, OptionsObject );
	}
	
	mEncoder.reset(new X264::TInstance(OptionsJson));

	mEncoder->mOnPacketReady = [this]()
	{
		this->OnPacketOutput();
	};

	mEncoderThread.reset(new SoyWorkerJobThread("H264 encoder"));
	mEncoderThread->Start();
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
	
	std::string EncodeMeta;

	//	 get user-supplied meta
	if ( !Params.IsArgumentUndefined(1) )
	{
		//	backwards compatibility, first param used to be a frame time
		if ( Params.IsArgumentNumber(1) )
		{
			auto FrameTime = Params.GetArgumentInt(1);
			Bind::TObject Meta = Params.mContext.CreateObjectInstance(Params.mLocalContext);
			Meta.SetInt("Time",FrameTime);
			EncodeMeta = Bind::StringifyObject( Params.mLocalContext, Meta );
		}
		else
		{
			auto Meta = Params.GetArgumentObject(1);
			EncodeMeta = Bind::StringifyObject( Params.mLocalContext, Meta );
		}
	}
	

	if (mEncoderThread)
	{
		std::shared_ptr<SoyPixels> PixelCopy( new SoyPixels() );
		{
			Soy::TScopeTimerPrint Timer("Copy pixels for thread", 2);
			Frame.GetPixels(*PixelCopy);
		}
		auto Encode = [=]()mutable
		{
			mEncoder->PushFrame(*PixelCopy, EncodeMeta);
		};
		auto EncoderJobCount = mEncoderThread->GetJobCount();
		if ( EncoderJobCount > 0 )
			std::Debug << "Encoder job queue size " << EncoderJobCount << std::endl;
		mEncoderThread->PushJob(Encode);
	}
	else
	{
		auto& Pixels = Frame.GetPixels();
		mEncoder->PushFrame(Pixels, EncodeMeta);
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
		//	in case of race condition (probbaly because there's no mutex on HasPackets)
		//	we get a failed pop (this lambda has probably executed since being queued)
		//	abort the resolve
		Array<uint8_t> Data;
		std::string Meta;
		if ( !mEncoder->PopPacket( GetArrayBridge(Data), Meta ) )
			return;

		auto PacketObject = ApiMedia::PacketToObject( Context, GetArrayBridge(Data), Meta );
		mNextPacketPromises.Resolve(PacketObject);
	};
	auto& Context = mNextPacketPromises.GetContext();
	Context.Queue(Resolve);
}


X264::TInstance::TInstance(const std::string& EncoderOptionsJson)
{
	mOnPacketReady = []()
	{
		std::Debug << "Encoded packet ready (no callback assigned)" << std::endl;
	};
	
	
	char ErrorBuffer[200] = {0};
	mHandle = PopH264_CreateEncoder( EncoderOptionsJson.c_str(), ErrorBuffer, std::size(ErrorBuffer) );
	if ( mHandle <= 0 )
	{
		std::stringstream Error;
		Error << "Failed to create PopH264 encoder instance. Error(" << mHandle << ") " << ErrorBuffer;
		throw Soy::AssertException(Error.str());
	}
	
	auto OnPacketReady = [](void* pThis)
	{
		auto* This = reinterpret_cast<TInstance*>(pThis);
		This->mOnPacketReady();
	};
	PopH264_EncoderAddOnNewPacketCallback( mHandle, OnPacketReady, this );
}

X264::TInstance::~TInstance()
{
	PopH264_DestroyEncoder(mHandle);	
}
	
void X264::TInstance::PushFrame(const SoyPixelsImpl& Pixels,const std::string& EncodeMetaJson)
{
	//	for meta, include the user-meta
	using namespace json11;
	Json::object Meta;
	if ( !EncodeMetaJson.empty() )
	{
		std::string Error;
		auto Obj = Json::parse(EncodeMetaJson, Error);
		if ( !Error.empty() )
			throw Soy::AssertException( std::string(__PRETTY_FUNCTION__) + Error );
		Meta = Obj.object_items();
	}
	
	//	include required meta
	Meta["Width"] = static_cast<int32_t>(Pixels.GetWidth());
	Meta["Height"] = static_cast<int32_t>(Pixels.GetHeight());
	
	//	try and be as flexible as possible
	BufferArray<std::shared_ptr<SoyPixelsImpl>,3> Planes;
	Pixels.SplitPlanes(GetArrayBridge(Planes));

	BufferArray<const uint8_t*,3> PixelDatas;
	BufferArray<int32_t,3> PixelSizes;

	//	gr: for now, use one plane instead of 2 for some formats
	if (Planes.GetSize() == 2)
	{
		auto Format = Pixels.GetFormat();
		//auto FormatName = magic_enum::enum_name(Format);
		auto FormatName = SoyPixelsFormat::ToString(Format);
		Meta["Format"] = std::string(FormatName);//	json11 converts string_view to array
		auto& Array = Pixels.GetPixelsArray();
		PixelSizes.PushBack(Array.GetDataSize());
		PixelDatas.PushBack(Array.GetArray());
	}
	else
	{
		for (auto i = 0; i < Planes.GetSize(); i++)
		{
			auto& Pixels = *Planes[i];
			PixelSizes.PushBack(Pixels.GetMeta().GetDataSize());
			PixelDatas.PushBack(Pixels.GetPixelsArray().GetArray());
		}
		
		if ( Planes.GetSize() == 1 )
		{
			auto& Pixels = *Planes[0];
			auto Format = Pixels.GetFormat();
			//auto FormatName = magic_enum::enum_name(Format);
			auto FormatName = SoyPixelsFormat::ToString(Format);
			Meta["Format"] = std::string(FormatName);//	json11 converts string_view to array
		}
	}

	while ( PixelDatas.GetSize() < 3 )
	{
		PixelDatas.PushBack(nullptr);
		PixelSizes.PushBack(0);
	}

	Meta["LumaSize"] = PixelSizes[0];
	Meta["ChromaUSize"] = PixelSizes[1];
	Meta["ChromaVSize"] = PixelSizes[2];

	Json FinalJson( Meta );
	auto MetaString = FinalJson.dump();
	
	char ErrorBuffer[500] = {0};
	PopH264_EncoderPushFrame( mHandle, MetaString.c_str(), PixelDatas[0], PixelDatas[1], PixelDatas[2], ErrorBuffer, std::size(ErrorBuffer) );
	if ( ErrorBuffer[0] != 0 )
	{
		std::string Error(ErrorBuffer);
		throw Soy::AssertException(Error);
	}
}

void X264::TInstance::FlushFrames()
{
	//	todo
}


bool X264::TInstance::HasPackets()
{
	auto NextSize = PopH264_EncoderPopData( mHandle, nullptr, 0 );
	if ( NextSize <= 0 )
		return false;
	return true;
}

bool X264::TInstance::PopPacket(ArrayBridge<uint8_t>&& Data,std::string& MetaJson)
{
	//	get size of packet
	auto NextSize = PopH264_EncoderPopData( mHandle, nullptr, 0 );
	//	no packet (or error!)
	if ( NextSize <= 0 )
		return false;

	//	get the meta
	char JsonBuffer[1000] = {0};
	PopH264_EncoderPeekData( mHandle, JsonBuffer, std::size(JsonBuffer) );
	MetaJson = std::string( JsonBuffer );

	Data.SetSize(NextSize);
	auto ReadSize = PopH264_EncoderPopData( mHandle, Data.GetArray(), Data.GetDataSize() );
	if ( ReadSize != NextSize )
	{
		//	error. The meta should contain an error
		std::stringstream Error;
		Error << "Error getting next packet (size " << ReadSize << "!=" << NextSize <<") Meta: " << MetaJson;
		throw Soy::AssertException(Error);
	}

	return true;
}
