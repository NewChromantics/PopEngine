#include "TApiMedia.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

#include "PopMovie/AvfVideoCapture.h"


using namespace v8;

const char EnumDevices_FunctionName[] = "EnumDevices";


void ApiMedia::Bind(TV8Container& Container)
{
	Container.BindObjectType("Media", TMediaWrapper::CreateTemplate );
	Container.BindObjectType("MediaSource", TMediaSourceWrapper::CreateTemplate );
}


void TMediaWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
	
	//	alloc window
	//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
	//		but it also needs to know of the V8container to run stuff
	//		cyclic hell!
	auto* NewWrapper = new TMediaWrapper();
	
	//	store persistent handle to the javascript object
	NewWrapper->mHandle.Reset( Isolate, Arguments.This() );
	NewWrapper->mContainer = &Container;
	
	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWrapper ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}


Local<FunctionTemplate> TMediaWrapper::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);
	
	//	add members
	Container.BindFunction<EnumDevices_FunctionName>( InstanceTemplate, EnumDevices );
	
	return ConstructorFunc;
}



template<typename TYPE>
v8::Persistent<TYPE,CopyablePersistentTraits<TYPE>> MakeLocal(v8::Isolate* Isolate,Local<TYPE> LocalHandle)
{
	Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
	PersistentHandle.Reset( Isolate, LocalHandle );
	return PersistentHandle;
}

v8::Local<v8::Value> TMediaWrapper::EnumDevices(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	//auto& This = v8::GetObject<TMediaWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;

	//auto* pThis = &This;
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( *Isolate, Resolver );

	auto* Container = &Params.mContainer;
	
	auto DoEnumDevices = [=]
	{
		try
		{
			Array<std::string> DeviceNames;
			auto EnumDevice = [&](const std::string& Name)
			{
				DeviceNames.PushBack(Name);
			};
			::Platform::EnumCaptureDevices(EnumDevice);
			
			auto OnCompleted = [=](Local<Context> Context)
			{
				//	return face points here
				//	gr: can't do this unless we're in the javascript thread...
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent );
				//ResolverPersistent.Reset();
				auto GetValue = [&](size_t Index)
				{
					return v8::GetString(*Context->GetIsolate(), DeviceNames[Index] );
				};
				auto DeviceNamesArray = v8::GetArray( *Context->GetIsolate(), DeviceNames.GetSize(), GetValue );
				ResolverLocal->Resolve( DeviceNamesArray );
			};
			
			//	queue the completion, doesn't need to be done instantly
			Container->QueueScoped( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Local<Context> Context)
			{
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent );
				//ResolverPersistent.Reset();
				auto Error = String::NewFromUtf8( Isolate, ExceptionString.c_str() );
				ResolverLocal->Reject( Error );
			};
			Container->QueueScoped( OnError );
		}
	};
	
	DoEnumDevices();
	//auto& Dlib = This.mDlibJobQueue;
	//Dlib.PushJob( RunFaceDetector );

	//	return the promise
	auto Promise = Resolver->GetPromise();
	return Promise;
}



void TMediaSourceWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
	
	//	alloc window
	//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
	//		but it also needs to know of the V8container to run stuff
	//		cyclic hell!
	auto* NewWrapper = new TMediaSourceWrapper();
	
	//	store persistent handle to the javascript object
	NewWrapper->mHandle.Reset( Isolate, Arguments.This() );
	NewWrapper->mContainer = &Container;

	auto OnFrameExtracted = [=](const SoyTime Time,size_t StreamIndex)
	{
		//std::Debug << "Got stream[" << StreamIndex << "] frame at " << Time << std::endl;
		NewWrapper->OnNewFrame(StreamIndex);
	};
	auto OnPrePushFrame = [](TPixelBuffer&,const TMediaExtractorParams&)
	{
		//std::Debug << "OnPrePushFrame" << std::endl;
	};

	//	create device
	try
	{
		auto DeviceName = v8::GetString( Arguments[0] );
		TMediaExtractorParams Params( DeviceName, DeviceName, OnFrameExtracted, OnPrePushFrame );
		Params.mForceNonPlanarOutput = false;
		Params.mDiscardOldFrames = true;
		NewWrapper->mExtractor = ::Platform::AllocCaptureExtractor( Params, nullptr );
		NewWrapper->mExtractor->AllocStreamBuffer(0);
		NewWrapper->mExtractor->Start(false);
	}
	catch(std::exception& e)
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, e.what() ));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWrapper ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}


Local<FunctionTemplate> TMediaSourceWrapper::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);
	
	//	add members
	//Container.BindFunction<EnumDevices_FunctionName>( InstanceTemplate, GetNewFramePromise );
	
	return ConstructorFunc;
}


void TMediaSourceWrapper::OnNewFrame(size_t StreamIndex)
{
	//	do a queued callback for OnNewFrame member
	//	we send the image handle here, because we can always make it have a TPixelBuffer internally and not grab the pixels
	auto PacketBuffer = this->mExtractor->GetStreamBuffer(StreamIndex);
	auto FramePacket = PacketBuffer->PopPacket();
	if ( !FramePacket )
	{
		//std::Debug << "Null packet in buffer?" << std::endl;
		return;
	}
	
	OnNewFrame( *FramePacket );
}


void TMediaSourceWrapper::OnNewFrame(const TMediaPacket& FramePacket)
{
	//	do a queued callback for OnNewFrame member
	auto* pImage = new TImageWrapper( *mContainer );
	
	//	get pixel buffer as pixels
	//	todo: put this in ImageWrapper so we can get opengl packets etc without conversion
	auto PixelBuffer = FramePacket.mPixelBuffer;
	if ( PixelBuffer == nullptr )
	{
		std::Debug << "Missing Pixel buffer in frame" << std::endl;
		return;
	}
	
	//	may get 2 planes
	BufferArray<SoyPixelsImpl*,2> Textures;
	float3x3 Transform;
	PixelBuffer->Lock( GetArrayBridge(Textures), Transform );
	try
	{
		//	convert pixels to RGB for face.
		//	todo: move to JS call which gives a promise, or more likely, opengl shader for when we want just a rect of the image
		SoyPixels RgbPixels( *Textures[0] );
		RgbPixels.SetFormat( SoyPixelsFormat::RGB );
		RgbPixels.ResizeFastSample( 640, 480 );
		pImage->SetPixels( RgbPixels );
		PixelBuffer->Unlock();
	}
	catch(std::exception& e)
	{
		PixelBuffer->Unlock();
		throw;
	}
	
	auto Runner = [this,pImage](Local<Context> context)
	{
		auto* isolate = context->GetIsolate();
		auto This = Local<Object>::New( isolate, this->mHandle );
		
		auto& Image = *pImage;
		auto ImageHandle = mContainer->CreateObjectInstance<TImageWrapper>( Image );
		
		BufferArray<Local<Value>,2> Args;
		Args.PushBack(ImageHandle);
	
		auto FuncHandle = v8::GetFunction( context, This, "OnNewFrame" );
		
		mContainer->ExecuteFunc( context, FuncHandle, This, GetArrayBridge(Args) );
	};
	mContainer->QueueScoped( Runner );
}

