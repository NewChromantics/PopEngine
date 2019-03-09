#include "TApiCommon.h"
#include "SoyDebug.h"
#include "SoyImage.h"
#include "SoyFilesystem.h"
#include "SoyStream.h"
#include "SoyOpengl.h"
#include "SoyOpenglContext.h"
#include "SoyMedia.h"
#include "JsCoreInstance.h"



const char CreateTestPromise_FunctionName[] = "CreateTestPromise";
const char Debug_FunctionName[] = "Debug";
const char CompileAndRun_FunctionName[] = "CompileAndRun";
const char LoadFileAsString_FunctionName[] = "LoadFileAsString";
const char LoadFileAsArrayBuffer_FunctionName[] = "LoadFileAsArrayBuffer";
const char WriteStringToFile_FunctionName[] = "WriteStringToFile";
const char GarbageCollect_FunctionName[] = "GarbageCollect";
const char SetTimeout_FunctionName[] = "SetTimeout";
const char Sleep_FunctionName[] = "Sleep";
const char ThreadTest_FunctionName[] = "ThreadTest";
const char GetTimeNowMs_FunctionName[] = "GetTimeNowMs";
const char GetComputerName_FunctionName[] = "GetComputerName";
const char ShowFileInFinder_FunctionName[] = "ShowFileInFinder";
const char GetImageHeapSize_FunctionName[] = "GetImageHeapSize";
const char GetImageHeapCount_FunctionName[] = "GetImageHeapCount";
const char GetHeapSize_FunctionName[] = "GetHeapSize";
const char GetHeapCount_FunctionName[] = "GetHeapCount";




const char Image_TypeName[] = "Image";

const char LoadFile_FunctionName[] = "Load";
const char Alloc_FunctionName[] = "Create";
const char Flip_FunctionName[] = "Flip";
const char GetWidth_FunctionName[] = "GetWidth";
const char GetHeight_FunctionName[] = "GetHeight";
const char GetRgba8_FunctionName[] = "GetRgba8";
const char SetLinearFilter_FunctionName[] = "SetLinearFilter";
const char Copy_FunctionName[] = "Copy";
const char WritePixels_FunctionName[] = "WritePixels";
const char Resize_FunctionName[] = "Resize";
const char Clip_FunctionName[] = "Clip";
const char Clear_FunctionName[] = "Clear";
const char SetFormat_FunctionName[] = "SetFormat";
const char GetFormat_FunctionName[] = "GetFormat";


namespace ApiPop
{
	const char Namespace[] = "Pop";
	
	static void 	Debug(Bind::TCallback& Params);
	static void 	CreateTestPromise(Bind::TCallback& Params);
	static void 	CompileAndRun(Bind::TCallback& Params);
	static void 	LoadFileAsString(Bind::TCallback& Params);
	static void 	LoadFileAsArrayBuffer(Bind::TCallback& Params);
	static void 	WriteStringToFile(Bind::TCallback& Params);
	static void 	GarbageCollect(Bind::TCallback& Params);
	static void 	SetTimeout(Bind::TCallback& Params);
	static void		Sleep(Bind::TCallback& Params);
	static void		ThreadTest(Bind::TCallback& Params);
	static void		GetTimeNowMs(Bind::TCallback& Params);
	static void		GetComputerName(Bind::TCallback& Params);
	static void		ShowFileInFinder(Bind::TCallback& Params);
	static void		GetImageHeapSize(Bind::TCallback& Params);
	static void		GetImageHeapCount(Bind::TCallback& Params);
	static void		GetHeapSize(Bind::TCallback& Params);
	static void		GetHeapCount(Bind::TCallback& Params);
}


void ApiPop::Debug(Bind::TCallback& Params)
{
	for ( auto a=0;	a<Params.GetArgumentCount();	a++ )
	{
		auto Arg = Params.GetArgumentString(a);
		std::Debug << Arg << std::endl;
	}
}


void ApiPop::CreateTestPromise(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise();
	
	Promise.Resolve("Resolved in c++");
	Params.Return( Promise );
}

void ApiPop::GarbageCollect(Bind::TCallback& Params)
{
	throw Soy::AssertException("v8 only");
	/*
	//	queue as job?
	std::Debug << "Invoking garbage collection..." << std::endl;
	auto& Paramsv8 = dynamic_cast<v8::TCallback&>( Params );
	Paramsv8.GetIsolate().RequestGarbageCollectionForTesting( v8::Isolate::kFullGarbageCollection );
	 */
}


static void ApiPop::SetTimeout(Bind::TCallback& Params)
{
	auto Callback = Params.GetArgumentFunction(0);
	auto TimeoutMs = Params.GetArgumentInt(1);
	auto CallbackPersistent = Params.mContext.CreatePersistent(Callback);
	
	auto OnRun = [=](Bind::TContext& Context)
	{
		try
		{
			auto Func = CallbackPersistent.GetFunction();
			Func.Call();
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in SetTimeout(" << TimeoutMs << ") callback: " << e.what() << std::endl;
		}
	};

	Params.mContext.QueueDelay( OnRun, TimeoutMs );
}




static void ApiPop::Sleep(Bind::TCallback& Params)
{
	throw Soy::AssertException("Not applicable in JavascriptCore");
	/*
	auto TimeoutMsHandle = v8::SafeCast<Number>(Params.mParams[0]);
	auto TimeoutMs = TimeoutMsHandle->Uint32Value();
	
	Params.mContainer.Yield( TimeoutMs );
	 */
}


std::shared_ptr<std::thread> gTestThread;
std::shared_ptr<JsCore::TContext> gTestContext;

void ApiPop::ThreadTest(Bind::TCallback& Params)
{
	/*
	auto ParamsJs = dynamic_cast<JsCore::TCallbackInfo&>( Params );
	auto TimeoutMs = Params.GetArgumentInt(0);
	
	if ( !gTestThread )
	{
		gTestContext = ParamsJs.mInstance.CreateContext();
		
		//	create another context
		auto ThreadFunc = [&]()
		{
			//auto Context = ParamsJs.mContext;
			auto Context = gTestContext->mContext;
			
			auto GlobalOther = JSContextGetGlobalObject( ParamsJs.mContext );
			{
				/*
				auto Global = JSContextGetGlobalObject( Context );
				JSStringRef GlobalNameString = JSStringCreateWithUTF8CString("this");
				
				JSValueRef Exception = nullptr;
				JSPropertyAttributes Attributes = kJSClassAttributeNone;
				JSObjectSetProperty( Context, Global, GlobalNameString, GlobalOther, Attributes, &Exception );
				 *  /
			}
			
			for ( auto i=0;	i<1000;	i++ )
			{
				std::this_thread::sleep_for( std::chrono::milliseconds(100) );
				
				//	create a promise object
				JSStringRef NewPromiseScript = JSStringCreateWithUTF8CString("Debug('Thread exec: ' + TestValue );");
				JSValueRef Exception = nullptr;
				JSEvaluateScript( Context, NewPromiseScript, GlobalOther, nullptr, 0, &Exception );
				if ( Exception!=nullptr )
					std::Debug << "An exception" << JsCore::GetString( Context, Exception ) << std::endl;
			}
			
		};
		gTestThread.reset( new std::thread(ThreadFunc) );
		
	}
	
	//	can we interrupt and call arbirtry funcs?
	//	gr: need to see if we cna do it on other threads
	std::this_thread::sleep_for( std::chrono::milliseconds(TimeoutMs) );
	*/
}



void ApiPop::GetTimeNowMs(Bind::TCallback& Params)
{
	SoyTime Now(true);
	
	auto NowMs = Now.GetMilliSeconds();
	auto NowMsInt = NowMs.count();
	
	Params.Return( NowMsInt );
}



void ApiPop::GetComputerName(Bind::TCallback& Params)
{
	auto Name = ::Platform::GetComputerName();
	Params.Return( Name );
}


void ApiPop::ShowFileInFinder(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	::Platform::ShowFileExplorer(Filename);
}


void ApiPop::GetImageHeapSize(Bind::TCallback& Params)
{
	auto& Heap = Params.mContext.GetImageHeap();
	auto Value = Heap.mAllocBytes;
	Params.Return( Value );
}

void ApiPop::GetImageHeapCount(Bind::TCallback& Params)
{
	auto& Heap = Params.mContext.GetImageHeap();
	auto Value = Heap.mAllocCount;
	Params.Return( Value );
}


void ApiPop::GetHeapSize(Bind::TCallback& Params)
{
	auto& Heap = Params.mContext.GetV8Heap();
	auto Value = Heap.mAllocBytes;
	Params.Return( Value );
}

void ApiPop::GetHeapCount(Bind::TCallback& Params)
{
	auto& Heap = Params.mContext.GetV8Heap();
	auto Value = Heap.mAllocCount;
	Params.Return( Value );
}





void ApiPop::CompileAndRun(Bind::TCallback& Params)
{
	auto Source = Params.GetArgumentString(0);
	auto Filename = Params.GetArgumentString(1);

	//	ignore the return for now
	Params.mContext.LoadScript( Source, Filename );
}



void ApiPop::LoadFileAsString(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	
	std::string Contents;
	Soy::FileToString( Filename, Contents);
	Params.Return( Contents );
}


void ApiPop::LoadFileAsArrayBuffer(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);

	Array<char> FileContents;
	Soy::FileToArray( GetArrayBridge(FileContents), Filename );
	auto FileContentsu8 = GetArrayBridge(FileContents).GetSubArray<uint8_t>(0,FileContents.GetDataSize());

	//	want this to be a typed array
	//auto ArrayBuffer = v8::GetTypedArray( Params.GetIsolate(), GetArrayBridge(FileContentsu8) );
	auto Array = Params.mContext.CreateArray( GetArrayBridge(FileContentsu8) );
	Params.Return( Array );
}



void ApiPop::WriteStringToFile(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	auto Contents = Params.GetArgumentString(1);
	auto Append = !Params.IsArgumentUndefined(2) ? Params.GetArgumentBool(2) : false;

	Soy::StringToFile( Filename, Contents, Append );
}


void ApiPop::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindObjectType<TImageWrapper>( Namespace );
	
	auto NamespaceObject = Context.GetGlobalObject(Namespace);
	
	Context.BindGlobalFunction<CreateTestPromise_FunctionName>( CreateTestPromise, Namespace );
	Context.BindGlobalFunction<Debug_FunctionName>( Debug, Namespace );
	Context.BindGlobalFunction<CompileAndRun_FunctionName>(CompileAndRun, Namespace );
	Context.BindGlobalFunction<LoadFileAsString_FunctionName>(LoadFileAsString, Namespace );
	Context.BindGlobalFunction<LoadFileAsArrayBuffer_FunctionName>(LoadFileAsArrayBuffer, Namespace );
	Context.BindGlobalFunction<WriteStringToFile_FunctionName>(WriteStringToFile, Namespace );
	Context.BindGlobalFunction<GarbageCollect_FunctionName>(GarbageCollect, Namespace );
	Context.BindGlobalFunction<SetTimeout_FunctionName>(SetTimeout, Namespace );
	Context.BindGlobalFunction<Sleep_FunctionName>(Sleep, Namespace );
	Context.BindGlobalFunction<ThreadTest_FunctionName>( ThreadTest, Namespace );
	Context.BindGlobalFunction<GetTimeNowMs_FunctionName>(GetTimeNowMs, Namespace );
	Context.BindGlobalFunction<GetComputerName_FunctionName>(GetComputerName, Namespace );
	Context.BindGlobalFunction<ShowFileInFinder_FunctionName>(ShowFileInFinder, Namespace );
	Context.BindGlobalFunction<GetImageHeapSize_FunctionName>(GetImageHeapSize, Namespace );
	Context.BindGlobalFunction<GetImageHeapCount_FunctionName>(GetImageHeapCount, Namespace );
	Context.BindGlobalFunction<GetHeapSize_FunctionName>(GetHeapSize, Namespace );
	Context.BindGlobalFunction<GetHeapCount_FunctionName>(GetHeapCount, Namespace );
}

TImageWrapper::~TImageWrapper()
{
	Free();
}

void TImageWrapper::Construct(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();

	
	if ( Params.IsArgumentString(0) )
		mName = Params.GetArgumentString(0);
	else
		mName = "undefined-name";
	
	/*
	//	try copying from other object
	const auto& Arg0 = Arguments.mParams[0];
	if ( Arg0->IsObject() )
	{
		try
		{
			auto& Arg0Image = v8::GetObject<TImageWrapper>( Arg0 );
			Copy(Arguments);
			return;
		}
		catch(std::exception& e)
		{
			std::Debug << "Trying to construct image from object: " << e.what() << std::endl;
		}
	}
	*/
	
	//	construct with filename
	if ( Params.IsArgumentString(0) )
	{
		LoadFile(Params);
		return;
	}
		
	//	construct with size
	if ( Params.IsArgumentArray(0) )
	{
		Alloc(Params);
		return;
	}

	
}

void TImageWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<Alloc_FunctionName>( Alloc );
	Template.BindFunction<LoadFile_FunctionName>( LoadFile );
	Template.BindFunction<Flip_FunctionName>( Flip );
	Template.BindFunction<GetWidth_FunctionName>( GetWidth );
	Template.BindFunction<GetHeight_FunctionName>( GetHeight );
	Template.BindFunction<GetRgba8_FunctionName>( GetRgba8 );
	Template.BindFunction<SetLinearFilter_FunctionName>( SetLinearFilter );
	Template.BindFunction<Copy_FunctionName>( Copy );
	Template.BindFunction<WritePixels_FunctionName>( WritePixels );
	Template.BindFunction<Resize_FunctionName>( Resize );
	Template.BindFunction<Clip_FunctionName>( Clip );
	Template.BindFunction<Clear_FunctionName>( Clear );
	Template.BindFunction<SetFormat_FunctionName>( SetFormat );
	Template.BindFunction<GetFormat_FunctionName>( GetFormat );
}


void TImageWrapper::Flip(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	
	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	auto& Pixels = This.GetPixels();
	Pixels.Flip();
	This.OnPixelsChanged();
}


void TImageWrapper::Alloc(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	SoyPixelsMeta Meta( 1, 1, SoyPixelsFormat::RGBA );
	
	if ( Params.IsArgumentArray(0) )
	{
		BufferArray<uint32_t,2> Size;
		Params.GetArgumentArray( 0, GetArrayBridge(Size) );
		Meta.DumbSetWidth( Size[0] );
		Meta.DumbSetHeight( Size[1] );

		if ( !Params.IsArgumentUndefined(1) )
		{
			auto Format = SoyPixelsFormat::ToType( Params.GetArgumentString(1) );
			Meta.DumbSetFormat( Format );
		}
	}
	else if ( !Params.IsArgumentUndefined(0) )
	{
		auto Width = Params.GetArgumentInt(0);
		auto Height = Params.GetArgumentInt(1);
		Meta.DumbSetWidth( Width );
		Meta.DumbSetHeight( Height );
		
		if ( !Params.IsArgumentUndefined(2) )
		{
			auto Format = SoyPixelsFormat::ToType( Params.GetArgumentString(2) );
			Meta.DumbSetFormat( Format );
		}
	}

	SoyPixels Temp( Meta );
	This.SetPixels( Temp );
}

void TImageWrapper::LoadFile(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	
	auto Filename = Params.GetArgumentFilename(0);
	
	This.DoLoadFile( Filename );
}

void TImageWrapper::DoLoadFile(const std::string& Filename)
{
	//	load file
	Array<char> Bytes;
	Soy::FileToArray( GetArrayBridge(Bytes), Filename );
	TStreamBuffer BytesBuffer;
	BytesBuffer.Push( GetArrayBridge(Bytes) );

	//	alloc pixels
	auto& Heap = mContext.GetImageHeap();
	std::shared_ptr<SoyPixels> NewPixels( new SoyPixels(Heap) );
	
	if ( Soy::StringEndsWith( Filename, Png::FileExtensions, false ) )
	{
		Png::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Jpeg::FileExtensions, false ) )
	{
		Jpeg::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Gif::FileExtensions, false ) )
	{
		Gif::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Tga::FileExtensions, false ) )
	{
		Tga::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Bmp::FileExtensions, false ) )
	{
		Bmp::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Psd::FileExtensions, false ) )
	{
		Psd::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		OnPixelsChanged();
		return;
	}

	throw Soy::AssertException( std::string("Unhandled image file extension of ") + Filename );
}


void TImageWrapper::DoSetLinearFilter(bool LinearFilter)
{
	//	for now, only allow this pre-creation
	//	what we could do, is queue an opengl job. but if we're IN a job now, it'll set it too late
	//	OR, queue it to be called before next GetTexture()
	if ( mOpenglTexture != nullptr )
		throw Soy::AssertException("Cannot change linear filter setting if texture is already created");

	mLinearFilter = LinearFilter;
}


void TImageWrapper::Copy(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	auto& That = Params.GetArgumentPointer<TImageWrapper>(0);

	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	std::lock_guard<std::recursive_mutex> ThatLock(That.mPixelsLock);

	auto& ThisPixels = This.GetPixels();
	auto& ThatPixels = That.GetPixels();

	ThisPixels.Copy(ThatPixels);
	This.OnPixelsChanged();
}

void TImageWrapper::WritePixels(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();

	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);

	auto Width = Params.GetArgumentInt(0);
	auto Height = Params.GetArgumentInt(1);
	
	Array<uint8_t> Rgba;
	Params.GetArgumentArray(2,GetArrayBridge(Rgba) );
	
	auto* Rgba8 = static_cast<uint8_t*>(Rgba.GetArray());
	auto DataSize = Rgba.GetDataSize();
	SoyPixelsRemote NewPixels( Rgba8, Width, Height, DataSize, SoyPixelsFormat::RGBA );
	This.SetPixels(NewPixels);
}



void TImageWrapper::Resize(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	
	auto NewWidth = Params.GetArgumentInt(0);
	auto NewHeight = Params.GetArgumentInt(1);

	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	
	auto& ThisPixels = This.GetPixels();
	
	ThisPixels.ResizeFastSample( NewWidth, NewHeight );
	This.OnPixelsChanged();
}


void TImageWrapper::Clear(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	This.Free();
}



void TImageWrapper::Clip(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	Soy::TScopeTimerPrint Timer(__func__,5);

	BufferArray<int,4> RectPx;

	Params.GetArgumentArray( 0, GetArrayBridge(RectPx) );
	
	if ( RectPx.GetSize() != 4 )
	{
		std::stringstream Error;
		Error << "Expected 4 values for cliping rect (got " << RectPx.GetSize() << ")";
		throw Soy::AssertException(Error.str());
	}
	
	if ( RectPx[0] < 0 || RectPx[1] < 0 || RectPx[2] <= 0 || RectPx[3] <= 0 )
	{
		std::stringstream Error;
		Error << "Clip( " << RectPx[0] << ", " << RectPx[1] << ", " << RectPx[2] << ", " << RectPx[3] << ") out of bounds";
		throw Soy::AssertException(Error.str());
	}

	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	
	auto& ThisPixels = This.GetPixels();
	
	ThisPixels.Clip( RectPx[0], RectPx[1], RectPx[2], RectPx[3] );
	This.OnPixelsChanged();
}


void TImageWrapper::SetFormat(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();

	auto FormatName = Params.GetArgumentString(0);
	auto NewFormat = SoyPixelsFormat::ToType(FormatName);
	
	//	gr: currently only handling pixels
	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	if ( This.mPixelsVersion != This.GetLatestVersion() )
		throw Soy::AssertException("Image.SetFormat only works on pixels at the moment, and that's not the latest version");

	auto& Pixels = This.GetPixels();
	Pixels.SetFormat(NewFormat);
	This.OnPixelsChanged();
}

void TImageWrapper::GetFormat(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	
	auto Meta = This.GetMeta();

	auto Format = Meta.GetFormat();
	auto FormatString = SoyPixelsFormat::ToString(Format);
	Params.Return( FormatString );
}



void TImageWrapper::Free()
{
	std::lock_guard<std::recursive_mutex> ThisLock(mPixelsLock);

	//	clear pixels
	mPixels.reset();
	mPixelsVersion = 0;
	

	
	//if ( mOpenglTexture )
	//	mOpenglTexture->mAutoRelease = false;
	

	//	clear gl
	if ( mOpenglTextureDealloc )
	{
		mOpenglTextureDealloc();
		mOpenglTextureDealloc = nullptr;
	}
	mOpenglTexture.reset();
	mOpenglTextureVersion = 0;
	
	mOpenglLastPixelReadBuffer.reset();
	mOpenglLastPixelReadBufferVersion = 0;
	
	//	clear pixel buffer
	mPixelBuffer.reset();
	mPixelBufferMeta = SoyPixelsMeta();
	mPixelBufferVersion = 0;
}

void TImageWrapper::GetWidth(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();

	auto Meta = This.GetMeta();
	Params.Return( Meta.GetWidth() );
}


void TImageWrapper::GetHeight(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	
	auto Meta = This.GetMeta();
	Params.Return( Meta.GetHeight() );
}


void TImageWrapper::GetRgba8(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	
	auto AllowBgraAsRgba = !Params.IsArgumentUndefined(0) ? Params.GetArgumentBool(0) : false;
	auto IsTargetArray = Params.IsArgumentArray(1);
	auto& Heap = Params.mContext.GetImageHeap();
	
	Soy::TScopeTimerPrint Timer(__func__,5);
	
	//	gr: this func will probably need to return a promise if reading from opengl etc (we want it to be async anyway!)
	auto& CurrentPixels = This.GetPixels();
	
	//	convert pixels if they're in the wrong format
	std::shared_ptr<SoyPixels> ConvertedPixels;
	SoyPixelsImpl* pPixels = nullptr;
	if ( CurrentPixels.GetFormat() == SoyPixelsFormat::RGBA )
	{
		pPixels = &CurrentPixels;
	}
	else if ( AllowBgraAsRgba && CurrentPixels.GetFormat() == SoyPixelsFormat::BGRA )
	{
		pPixels = &CurrentPixels;
	}
	else
	{
		Soy::TScopeTimerPrint Timer("GetRgba8 conversion to RGBA", 5 );
		ConvertedPixels.reset( new SoyPixels(CurrentPixels,Heap) );
		ConvertedPixels->SetFormat( SoyPixelsFormat::RGBA );
		pPixels = ConvertedPixels.get();
	}	
	auto& Pixels = *pPixels;
	
	auto& PixelsArray = Pixels.GetPixelsArray();
	
	if ( IsTargetArray )
	{
		auto TargetArray = Params.GetArgumentArray(1);
		TargetArray.CopyTo( GetArrayBridge(PixelsArray) );
		Params.Return( TargetArray );
	}
	else
	{
		auto TargetArray = Params.mContext.CreateArray( GetArrayBridge(PixelsArray) );
		Params.Return( TargetArray );
	}
}


void TImageWrapper::SetLinearFilter(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	auto LinearFilter = Params.GetArgumentBool(0);
	This.DoSetLinearFilter( LinearFilter );
}


void TImageWrapper::GetPixelBufferPixels(std::function<void(const ArrayBridge<SoyPixelsImpl*>&,float3x3&)> Callback)
{
	if ( !mPixelBuffer )
		throw Soy::AssertException("Can't get pixel buffer pixels with no pixelbuffer");
	
	if ( mPixelBufferVersion != GetLatestVersion() )
		throw Soy::AssertException("Trying to get pixel buffer pixels that are out of date");

	//	lock pixels
	BufferArray<SoyPixelsImpl*,2> Textures;
	float3x3 Transform;
	mPixelBuffer->Lock( GetArrayBridge(Textures), Transform );
	try
	{
		Callback( GetArrayBridge(Textures), Transform );
		mPixelBuffer->Unlock();
	}
	catch(std::exception& e)
	{
		mPixelBuffer->Unlock();
		throw;
	}
}

void TImageWrapper::GetTexture(Opengl::TContext& Context,std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError)
{
	//	already created & current version
	if ( mOpenglTexture != nullptr )
	{
		if ( mOpenglTextureVersion == GetLatestVersion() )
		{
			OnTextureLoaded();
			return;
		}
	}
	
	if ( !mPixels && !mPixelBuffer )
		throw Soy::AssertException("Trying to get opengl texture when we have no pixels/pixelbuffer");
	
	auto* pContext = &Context;
	auto AllocAndOrUpload = [=]
	{
		//	gr: this will need to be on the context's thread
		//		need to fail here if we're not
		try
		{
			std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
			Soy::TScopeTimerPrint Timer("TImageWrapper::GetTexture::Alloc/Upload", 10 );

			auto AllocTexture = [&](const SoyPixelsMeta& Meta)
			{
				auto TextureSlot = pContext->mCurrentTextureSlot++;
				if ( mOpenglTexture == nullptr )
				{
					//std::Debug << "Creating new opengl texture " << Meta << " in slot " << TextureSlot << std::endl;
					mOpenglTexture.reset( new Opengl::TTexture( Meta, GL_TEXTURE_2D, TextureSlot ) );
					//std::Debug << "<<< " << mOpenglTexture->mTexture.mName << std::endl;
					//this->mOpenglClientStorage.reset( new SoyPixels );
					//mOpenglTexture->mClientBuffer = this->mOpenglClientStorage;
				
					//mOpenglTexture->mAutoRelease = false;
					
					//	alloc the deffered delete func
					mOpenglTextureDealloc = [this,pContext]
					{
						//	gr: this context can be deleted here...
						pContext->QueueDelete(mOpenglTexture);
					};
				}
				mOpenglTexture->Bind(TextureSlot);
				mOpenglTexture->SetFilter( mLinearFilter );
				mOpenglTexture->SetRepeat( mRepeating );
			};

			SoyGraphics::TTextureUploadParams UploadParams;
			UploadParams.mAllowClientStorage = true;
			
			if ( GetLatestVersion() == mPixelsVersion )
			{
				AllocTexture( mPixels->GetMeta() );
				mOpenglTexture->Write( *mPixels, UploadParams );
				mOpenglTextureVersion = mPixelsVersion;
			}
			else if ( GetLatestVersion() == mPixelBufferVersion )
			{
				auto CopyPixels = [&](const ArrayBridge<SoyPixelsImpl*>& Textures,float3x3& Transform)
				{
					auto& Pixels = *Textures[0];
					mPixelBufferMeta = Pixels.GetMeta();
					AllocTexture( mPixelBufferMeta );
					mOpenglTexture->Write( Pixels, UploadParams );
					mOpenglTextureVersion = mPixelBufferVersion;
				};
				GetPixelBufferPixels(CopyPixels);
			}
			else
			{
				throw Soy::AssertException("Don't know where to get meta for new opengl texture");
			}
			OnTextureLoaded();
		}
		catch(std::exception& e)
		{
			OnError( e.what() );
		}
	};
	
	if ( Context.IsLockedToThisThread() )
	{
		AllocAndOrUpload();
	}
	else
	{
		Context.PushJob( AllocAndOrUpload );
	}
}

Opengl::TTexture& TImageWrapper::GetTexture()
{
	if ( !mOpenglTexture )
		throw Soy::AssertException("Image missing opengl texture. Accessing before generating.");
	
	if ( mOpenglTextureVersion != GetLatestVersion() )
		throw Soy::AssertException("Opengl texture is out of date");
	
	return *mOpenglTexture;
}



void TImageWrapper::GetPixels(SoyPixelsImpl& CopyTarget)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Pixels = GetPixels();
	CopyTarget.Copy(Pixels);
}

SoyPixelsMeta TImageWrapper::GetMeta()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	auto LatestVersion = GetLatestVersion();

	//	opengl may have been released!
	if ( mOpenglTextureVersion == LatestVersion && mOpenglTexture )
	{
		return mOpenglTexture->GetMeta();
	}
	
	if ( mPixelsVersion == LatestVersion && mPixels )
		return mPixels->GetMeta();

	if ( mPixelBufferVersion == LatestVersion )
	{
		if ( mPixelBufferMeta.IsValid() )
			return mPixelBufferMeta;
	}
	
	if ( mPixelBufferVersion == LatestVersion && mPixelBuffer )
	{	
		//	need to fetch the pixels to get the meta :/
		//	so we need to read the pixels, then get the meta from there
		BufferArray<SoyPixelsImpl*,2> Textures;
		float3x3 Transform;
		mPixelBuffer->Lock( GetArrayBridge(Textures), Transform );
		try
		{
			//	gr: there's a chance here, some TPixelBuffers unlock and then release the data.
			//		need to not let that happen
			mPixelBufferMeta = Textures[0]->GetMeta();
			mPixelBuffer->Unlock();
			return mPixelBufferMeta;
		}
		catch(std::exception& e)
		{
			mPixelBuffer->Unlock();
			throw;
		}
	}
	
	if ( mPixelsVersion == LatestVersion && mPixels )
		return mPixels->GetMeta();
	
	throw Soy::AssertException("Don't know where to get meta from");
}

SoyPixelsImpl& TImageWrapper::GetPixels()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Heap = mContext.GetImageHeap();

	if ( mPixelsVersion < GetLatestVersion() && mPixelBufferVersion == GetLatestVersion() )
	{
		//	grab pixels from image buffer
		auto CopyPixels = [&](const ArrayBridge<SoyPixelsImpl*>& Pixels,float3x3& Transform)
		{
			mPixels.reset( new SoyPixels(Heap) );
			mPixels->Copy( *Pixels[0] );
			mPixelsVersion = mPixelBufferVersion;
		};
		this->GetPixelBufferPixels( CopyPixels );
		return *mPixels;
	}
	

	if ( mPixelsVersion < GetLatestVersion() )
	{
		std::stringstream Error;
		Error << "Image pixels(v" << mPixelsVersion <<") are out of date (v" << GetLatestVersion() << ")";
		throw Soy::AssertException(Error.str());
	}
	
	//	is latest and not allocated, this is okay, lets just alloc
	if ( mPixelsVersion == 0 && mPixels == nullptr )
	{
		mPixels.reset( new SoyPixels(Heap) );
		mPixelsVersion = 1;
	}
	
	if ( mPixels == nullptr )
	{
		std::stringstream Error;
		Error << "Image pixels(v" << mPixelsVersion <<") latest, but null?";
		throw Soy::AssertException(Error.str());
	}
	
	return *mPixels;
}

size_t TImageWrapper::GetLatestVersion() const
{
	size_t MaxVersion = mPixelsVersion;
	if ( mOpenglTextureVersion > MaxVersion )
		MaxVersion = mOpenglTextureVersion;
	
	if ( mPixelBufferVersion > MaxVersion )
		MaxVersion = mPixelBufferVersion;
	
	return MaxVersion;
}


void TImageWrapper::OnOpenglTextureChanged()
{
	//	is now latest version
	auto LatestVersion = GetLatestVersion();
	mOpenglTextureVersion = LatestVersion+1;
	size_t TextureSlot = 0;
	mOpenglTexture->Bind(TextureSlot);
	mOpenglTexture->RefreshMeta();
}



void TImageWrapper::OnPixelsChanged()
{
	auto LatestVersion = GetLatestVersion();
	mPixelsVersion = LatestVersion+1;
}

void TImageWrapper::SetPixels(const SoyPixelsImpl& NewPixels)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Heap = mContext.GetImageHeap();
	mPixels.reset( new SoyPixels(NewPixels,Heap) );
	OnPixelsChanged();
}

void TImageWrapper::SetPixels(std::shared_ptr<SoyPixelsImpl> NewPixels)
{
	//if ( NewPixels->GetFormat() != SoyPixelsFormat::RGB )
	//	std::Debug << "Setting image to pixels: " << NewPixels->GetMeta() << std::endl;
	
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixels = NewPixels;
	OnPixelsChanged();
}

void TImageWrapper::SetPixelBuffer(std::shared_ptr<TPixelBuffer> NewPixels)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixelBuffer = NewPixels;
	mPixelBufferVersion = GetLatestVersion()+1;
}

void TImageWrapper::ReadOpenglPixels(SoyPixelsFormat::Type Format)
{
	//	gr: this needs to be in the opengl thread!
	//Context.IsInThread
	
	if ( !mOpenglTexture )
		throw Soy::AssertException("Trying to ReadOpenglPixels with no texture");

	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	auto& Heap = mContext.GetImageHeap();

	//	warning in case we haven't actually updated
	if ( mPixelsVersion >= mOpenglTextureVersion )
		std::Debug << "Warning, overwriting newer/same pixels(v" << mPixelsVersion << ") with gl texture (v" << mOpenglTextureVersion << ")";
	//	if we have no pixels, alloc
	if ( mPixels == nullptr )
		mPixels.reset( new SoyPixels(Heap) );

	auto Flip = false;
	
	mPixels->GetMeta().DumbSetFormat(Format);
	mPixels->GetPixelsArray().SetSize( mPixels->GetMeta().GetDataSize() );
	
	mOpenglTexture->Read( *mPixels, Format, Flip );
	mPixelsVersion = mOpenglTextureVersion;
}

void TImageWrapper::SetOpenglLastPixelReadBuffer(std::shared_ptr<Array<uint8_t>> PixelBuffer)
{
	Soy::TScopeTimerPrint Timer(__func__,5);
	if ( GetLatestVersion() != mOpenglTextureVersion )
	{
		std::stringstream Error;
		Error << __func__ << " expected opengl (" << mOpenglTextureVersion << ") to be latest version (" << GetLatestVersion() << ")";
		throw Soy::AssertException(Error.str());
	}
	
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	mOpenglLastPixelReadBuffer = PixelBuffer;
	mOpenglLastPixelReadBufferVersion = mOpenglTextureVersion;
}

