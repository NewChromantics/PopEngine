#include "TApiMedia.h"
#include "TApiCommon.h"
#include <SoyFilesystem.h>
#include <SoyMedia.h>
#include <magic_enum.hpp>
#include "Json11/json11.hpp"
#include <SoyRuntimeLibrary.h>



//	video decoding and encoding
#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include <PopH264_Osx/PopH264.h>	//	via swift package. Found via auto complete!
#elif defined(TARGET_WINDOWS)
#pragma comment(lib,"PopH264.lib")
#include <com.newchromantics.poph264/windows/Release_x64/PopH264.h>
#elif defined(TARGET_LINUX) || defined(TARGET_ANDROID)
#include <PopH264.h>
#endif

#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include <PopCameraDevice_Osx/PopCameraDevice.h>//	via swift package. Found via auto complete!
#elif defined(TARGET_WINDOWS)
#pragma comment(lib,"PopCameraDevice.lib")
#include <popcameradevice/windows/Release_NoKinect_x64/PopCameraDevice.h>
#elif defined(TARGET_LINUX) || defined(TARGET_ANDROID)
#include <PopCameraDevice.h>
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

namespace PopH264Encoder
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



class PopH264Decoder::TInstance
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


class PopH264Encoder::TInstance
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
	DEFINE_BIND_TYPENAME(H264Decoder);
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

	Bind::TPersistent	FrameToObject(Bind::TLocalContext& Context, PopCameraDevice::TFrame& Frame,size_t PendingFrames);
	Bind::TObject		PacketToObject(Bind::TLocalContext& Context,const ArrayBridge<uint8_t>&& Data,const std::string& MetaJson);
}



void ApiMedia::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<BindFunction::EnumDevices>( ApiMedia::EnumDevices, Namespace );

	Context.BindObjectType<TCameraDeviceWrapper>( Namespace );

	Context.BindObjectType<TH264DecoderWrapper>(Namespace);
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
			//	gr: this buffer had to get bigger again, could maybe check the last char is } (or parse) and try again :)
			Array<char> JsonBuffer(1024*100);
			PopCameraDevice_EnumCameraDevicesJson(JsonBuffer.GetArray(), size_cast<int>(JsonBuffer.GetDataSize()));

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

	std::Debug << "PopH264 version " << PopH264_GetVersion() << std::endl;
}


void ApiMedia::TH264DecoderWrapper::Construct(Bind::TCallback& Params)
{
	if (!Params.IsArgumentUndefined(0))
		mSplitPlanes = Params.GetArgumentBool(0);

	mDecoder.reset( new PopH264Decoder::TInstance );
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

void ApiMedia::TH264DecoderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::Decode>(&TH264DecoderWrapper::Decode );
	Template.BindFunction<ApiMedia::BindFunction::WaitForNextFrame>(&TH264DecoderWrapper::WaitForNextFrame);
}



void ApiMedia::TH264DecoderWrapper::FlushQueuedData()
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

std::shared_ptr<Array<uint8_t>> ApiMedia::TH264DecoderWrapper::PopQueuedData()
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


void ApiMedia::TH264DecoderWrapper::PushQueuedData(const ArrayBridge<uint8_t>&& Data)
{
	std::lock_guard<std::mutex> Lock(mPushDataLock);
	if ( !mPushData )
		mPushData.reset( new Array<uint8_t>() );
	mPushData->PushBackArray(Data);
	//std::Debug << "Decoder queue size " << mPushData->GetDataSize() << " bytes" << std::endl;
}
	
void ApiMedia::TH264DecoderWrapper::Decode(Bind::TCallback& Params)
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

void ApiMedia::TH264DecoderWrapper::FlushPendingFrames()
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
			auto FrameObject = ApiMedia::FrameToObject(Context, PoppedFrame,mFrames.GetSize());
			auto Object = FrameObject.GetObject(Context);
			mFrameRequests.Resolve( Context, Object );
		}
		else
		{
			mFrameRequests.Reject(Context,PoppedError);
		}
	};
	auto& Context = mFrameRequests.GetContext();
	Context.Queue(Flush);
}

void ApiMedia::TH264DecoderWrapper::OnNewFrame()
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

void ApiMedia::TH264DecoderWrapper::OnError(const std::string& Error)
{
	{
		std::lock_guard<std::mutex> Lock(mFramesLock);
		mErrors.PushBack(Error);
	}
	FlushPendingFrames();
}

void ApiMedia::TH264DecoderWrapper::WaitForNextFrame(Bind::TCallback& Params)
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

	std::Debug << "PopCameraDevice version " << PopCameraDevice_GetVersion() << std::endl;
}


PopCameraDevice::TInstance::TInstance(const std::string& Name,const std::string& OptionsJson, std::function<void()> OnNewFrame) :
	mOnNewFrame	( OnNewFrame )
{
	char ErrorBuffer[1000] = { 0 };
	mHandle = PopCameraDevice_CreateCameraDevice(Name.c_str(), OptionsJson.c_str(), ErrorBuffer, std::size(ErrorBuffer));

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


void GetPixelMetasFromJson(ArrayBridge<SoyPixelsMeta>&& PlaneMetas, const std::string& JsonString,std::string& FrameMeta)
{
	//std::Debug << __PRETTY_FUNCTION__ << "(" << JsonString << std::endl;
	std::string Error;
	auto JsonObject = json11::Json::parse(JsonString, Error);
	if (JsonObject == json11::Json())
		throw Soy::AssertException(std::string("JSON parse error: ") + Error);

	//	gr: the whole thing is now the meta... do we want the distinction?
	//FrameMeta = JsonObject["Meta"].dump();
	FrameMeta = JsonString;

	auto JsonPlanesNode = JsonObject["Planes"];
	if (!JsonPlanesNode.is_array())
		throw Soy::AssertException(std::string("Expecting Frame Meta JSON .Planes to be an array;") + JsonString);

	auto& JsonPlanes = JsonPlanesNode.array_items();
	auto ParsePlane = [&](const json11::Json& PlaneObject)
	{
		auto Width = PlaneObject["Width"].number_value();
		auto Height = PlaneObject["Height"].number_value();
		//auto Channels = PlaneObject["Channels"].number_value();
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
		PlaneMetas.PushBack(Meta);
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

	//	gr: this can get massive now with arkit... might run into some issues
	Array<char> JsonBuffer(1024*25);
	//	gr: this is json now, so we need a good way to extract what we need...
	auto NextFrameNumberSigned = PopCameraDevice_PeekNextFrame(mHandle, JsonBuffer.GetArray(), JsonBuffer.GetDataSize());
	if (NextFrameNumberSigned == -1 )
		throw TNoFrameException();
	Frame.mFrameNumber = static_cast<uint32_t>(NextFrameNumberSigned);

	std::string Json(JsonBuffer.GetArray());

	//	get plane count
	BufferArray<SoyPixelsMeta, 4> PlaneMetas;
	GetPixelMetasFromJson(GetArrayBridge(PlaneMetas), Json, Frame.mMeta );
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
	
	auto NewFrameTime = PopCameraDevice_PopNextFrame(
		mHandle,
		nullptr,	//	json buffer, we've already got above
		0,
		PlanePixelsBytes[0], size_cast<int>(PlanePixelsByteSize[0]),
		PlanePixelsBytes[1], size_cast<int>(PlanePixelsByteSize[1]),
		PlanePixelsBytes[2], size_cast<int>(PlanePixelsByteSize[2])
	);

	//	no new frame
	if (NewFrameTime == -1)
		throw Soy::AssertException("New frame post-peek invalid");

	return Frame;
}




void ApiMedia::TCameraDeviceWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentString(0);
		
	//	param 1 is now an object of json params
	std::string OptionsJson;
	if (!Params.IsArgumentUndefined(1))
	{
		auto OptionsObject = Params.GetArgumentObject(1);
		OptionsJson = Bind::StringifyObject( Params.mLocalContext, OptionsObject );
	}

	if (!Params.IsArgumentUndefined(2))
	{
		//throw Soy::AssertException("Argument 3(OnlyLatestFrame) for Api.Media.Source should now be in json options");
		mOnlyLatestFrame = Params.GetArgumentBool(2);
	}

	//	todo: frame buffer [plane]pool
	auto OnNewFrame = std::bind(&TCameraDeviceWrapper::OnNewFrame, this);
	mInstance.reset(new PopCameraDevice::TInstance( DeviceName, OptionsJson, OnNewFrame ));
}


void ApiMedia::TCameraDeviceWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::WaitForNextFrame>(&TCameraDeviceWrapper::WaitForNextFrame);
	Template.BindFunction<ApiMedia::BindFunction::Free>(&TCameraDeviceWrapper::Free);
}

void ApiMedia::TCameraDeviceWrapper::Free(Bind::TCallback& Params)
{
	mInstance.reset();
}

void ApiMedia::TCameraDeviceWrapper::WaitForNextFrame(Bind::TCallback& Params)
{
	auto Promise = mFrameRequests.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	FlushPendingFrames();
}

JsCore::TPersistent ApiMedia::FrameToObject(Bind::TLocalContext& Context,PopCameraDevice::TFrame& Frame,size_t PendingFrames)
{
	//	alloc key names once
	static std::string _TimeMs = "TimeMs";
	static std::string _Meta = "Meta";
	static std::string _Planes = "Planes";
	static std::string _PendingFrames = "PendingFrames";

	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__,3);
	auto FrameObject = Context.mGlobalContext.CreateObjectInstance(Context);
	
	//	gr: it feels like the object we're allocating is disapearing whilst we're doing stuff
	//		we should be locked though, so why would the garbage collector do this...
	JsCore::TPersistent FrameObjectPersistent( Context, FrameObject, "ApiMedia::FrameToObject Frame" );
	
	auto Max32 = std::numeric_limits<uint32_t>::max();
	auto Time64 = Frame.mTime.GetTime();
	auto Time32 = Time64 % Max32;
	
	FrameObject.SetInt(_TimeMs,Time32);

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
		Image.SetName("Media output frame");
		Image.SetPixels(Pixels);
		PlaneImages.PushBack(ImageObject);
	};
	AddImage(Frame.mPlane0);
	AddImage(Frame.mPlane1);
	AddImage(Frame.mPlane2);
	AddImage(Frame.mPlane3);
	

	FrameObject.SetArray(_Planes, GetArrayBridge(PlaneImages));
	
	//	extra meta
	FrameObject.SetInt(_PendingFrames,PendingFrames);

	return FrameObjectPersistent;
}

Bind::TObject ApiMedia::PacketToObject(Bind::TLocalContext& Context,const ArrayBridge<uint8_t>&& Data,const std::string& MetaJson)
{
	//	alloc key names once
	static std::string _Data = "Data";
	static std::string _Meta = "Meta";
	static std::string _PendingFrames = "PendingFrames";

	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__,3);
	auto FrameObject = Context.mGlobalContext.CreateObjectInstance(Context);
	
	//	convert meta json to an object
	auto MetaObject = Bind::ParseObjectString( Context.mLocalContext, MetaJson );
	
	FrameObject.SetArray(_Data,Data);
	FrameObject.SetObject(_Meta, MetaObject);
	
	return FrameObject;
}

void ApiMedia::TCameraDeviceWrapper::FlushPendingFrames()
{
	if (mFrames.IsEmpty())
		return;
	if (!mFrameRequests.HasPromises())
		return;

	auto Flush = [this](Bind::TLocalContext& Context)
	{
		//	gr: this flush is expensive because of the work done AFTERwards (encoding!)
		Soy::TScopeTimerPrint Timer("TPopCameraDeviceWrapper::FlushPendingFrames::Flush", 60);
		
		PopCameraDevice::TFrame PoppedFrame;
		{
			Soy::TScopeTimerPrint Timer("TPopCameraDeviceWrapper::FlushPendingFrames::Flush Lock&Pop",5);
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
		auto PendingFrames = mFrames.GetSize();
		auto FrameObject = ApiMedia::FrameToObject(Context,PoppedFrame,PendingFrames);
		auto Object = FrameObject.GetObject(Context);
		mFrameRequests.Resolve(Context,Object);
	};
	auto& Context = mFrameRequests.GetContext();
	Context.Queue(Flush);
}

void ApiMedia::TCameraDeviceWrapper::OnNewFrame()
{
	PopCameraDevice::TFrame Frame = mInstance->PopLastFrame();

	{
		std::lock_guard<std::mutex> Lock(mFramesLock);
		mFrames.PushBack(Frame);
	}
	FlushPendingFrames();
}


PopH264Decoder::TInstance::TInstance()
{
	PopH264::LoadDll();
	
	const char* OptionsJson = "{}";
	char ErrorBuffer[2000] = {0};
	mHandle = PopH264_CreateDecoder(OptionsJson,ErrorBuffer,std::size(ErrorBuffer));
	if ( mHandle <= 0 )
	{
		std::stringstream Error;
		Error << "Failed to create PopH264 decoder instance. Error=" << ErrorBuffer << " handle=" << mHandle;
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


PopH264Decoder::TInstance::~TInstance()
{
	PopH264_DestroyInstance(mHandle);
}



void PopH264Decoder::TInstance::PushData(ArrayBridge<uint8_t>&& Data, int32_t FrameNumber)
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

size_t PopH264Decoder::TInstance::GetPendingFrameCount()
{
	return mPendingFrameCount;
}

PopCameraDevice::TFrame PopH264Decoder::TInstance::PopLastFrame(bool SplitPlanes,bool ONlyLatest)
{
	PopCameraDevice::TFrame Frame;
	Frame.mTime = SoyTime(true);
		

	Array<char> JsonBuffer(1024*25);
	//	gr: this is json now, so we need a good way to extract what we need...
	PopH264_PeekFrame(mHandle, JsonBuffer.GetArray(), JsonBuffer.GetSize() );
	//if (NextFrameNumber < 0)
	//	throw TNoFrameException();
	std::string Json(JsonBuffer.GetArray());

	//	get plane count
	BufferArray<SoyPixelsMeta, 4> PlaneMetas;
	GetPixelMetasFromJson(GetArrayBridge(PlaneMetas), Json, Frame.mMeta );
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



void ApiMedia::TH264EncoderWrapper::Construct(Bind::TCallback& Params)
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
	
	mEncoder.reset(new PopH264Encoder::TInstance(OptionsJson));

	mEncoder->mOnPacketReady = [this]()
	{
		this->OnPacketOutput();
	};

	mEncoderThread.reset(new SoyWorkerJobThread("H264 encoder"));
	mEncoderThread->Start();
}

void ApiMedia::TH264EncoderWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiMedia::BindFunction::Encode>(&TH264EncoderWrapper::Encode);
	Template.BindFunction<ApiMedia::BindFunction::EncodeFinished>(&TH264EncoderWrapper::EncodeFinished);
	Template.BindFunction<ApiMedia::BindFunction::WaitForNextPacket>(&TH264EncoderWrapper::WaitForNextPacket);
}

void ApiMedia::TH264EncoderWrapper::Encode(Bind::TCallback& Params)
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
		//	gr: we're doing redundant copies here, see if we can just make Image.CopyPixels(NewPixels)
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

void ApiMedia::TH264EncoderWrapper::EncodeFinished(Bind::TCallback& Params)
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

void ApiMedia::TH264EncoderWrapper::WaitForNextPacket(Bind::TCallback& Params)
{
	auto Promise = mNextPacketPromises.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	//	flush in case there's data availible
	OnPacketOutput();
}

void ApiMedia::TH264EncoderWrapper::OnPacketOutput()
{
	//	no promises
	if (!mNextPacketPromises.HasPromises())
		return;

	if ( !mEncoder->HasPackets())
		return;
	
	auto Resolve = [this](Bind::TLocalContext& Context)
	{
		//	gr: also, we can lose packets, if in the mean time promises have been flushed
		//		so check again... as we're in the JS thread.. nothing else should flush in the mean time
		//		but still consider popping & unpopping promises to resolve here
		if (!mNextPacketPromises.HasPromises())
			return;

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


PopH264Encoder::TInstance::TInstance(const std::string& EncoderOptionsJson)
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

PopH264Encoder::TInstance::~TInstance()
{
	PopH264_DestroyEncoder(mHandle);	
}
	
void PopH264Encoder::TInstance::PushFrame(const SoyPixelsImpl& Pixels,const std::string& EncodeMetaJson)
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

void PopH264Encoder::TInstance::FlushFrames()
{
	//	todo
}


bool PopH264Encoder::TInstance::HasPackets()
{
	auto NextSize = PopH264_EncoderPopData( mHandle, nullptr, 0 );
	if ( NextSize <= 0 )
		return false;
	return true;
}

bool PopH264Encoder::TInstance::PopPacket(ArrayBridge<uint8_t>&& Data,std::string& MetaJson)
{
	//	get size of packet
	auto NextSize = PopH264_EncoderPopData( mHandle, nullptr, 0 );
	//	no packet (or error!)
	if ( NextSize <= 0 )
		return false;

	//	get the meta
	Array<char> JsonBuffer(1024*25);
	PopH264_EncoderPeekData( mHandle, JsonBuffer.GetArray(), JsonBuffer.GetSize() );
	MetaJson = std::string( JsonBuffer.GetArray() );

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

