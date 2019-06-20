#include "TApiCommon.h"
#include "SoyDebug.h"
#include "SoyImage.h"
#include "SoyFilesystem.h"
#include "SoyStream.h"
#include "SoyOpengl.h"
#include "SoyOpenglContext.h"
#include "SoyMedia.h"
#include "TBind.h"
#include "SoyWindow.h"
#include "SoyPng.h"


DEFINE_BIND_FUNCTIONNAME(LoadFileAsString);

//	system stuff
//extern const char LoadFileAsString_FunctionName[] = "LoadFileAsString";
extern const char LoadFileAsArrayBuffer_FunctionName[] = "LoadFileAsArrayBuffer";
extern const char WriteStringToFile_FunctionName[] = "WriteStringToFile";
extern const char WriteToFile_FunctionName[] = "WriteToFile";
extern const char SetTimeout_FunctionName[] = "SetTimeout";
extern const char GetTimeNowMs_FunctionName[] = "GetTimeNowMs";

//	engine stuff
DEFINE_BIND_FUNCTIONNAME(CompileAndRun);
DEFINE_BIND_FUNCTIONNAME(CreateTestPromise);
DEFINE_BIND_FUNCTIONNAME(Debug);
DEFINE_BIND_FUNCTIONNAME(ThreadTest);
DEFINE_BIND_FUNCTIONNAME(GetImageHeapSize);
DEFINE_BIND_FUNCTIONNAME(GetImageHeapCount);
DEFINE_BIND_FUNCTIONNAME(GetHeapSize);
DEFINE_BIND_FUNCTIONNAME(GetHeapCount);
DEFINE_BIND_FUNCTIONNAME(GetHeapObjects);
DEFINE_BIND_FUNCTIONNAME(GetCrtHeapSize);
DEFINE_BIND_FUNCTIONNAME(GetCrtHeapCount);
DEFINE_BIND_FUNCTIONNAME(GarbageCollect);
DEFINE_BIND_FUNCTIONNAME(Sleep);
DEFINE_BIND_FUNCTIONNAME(Yield);
DEFINE_BIND_FUNCTIONNAME(IsDebuggerAttached);
DEFINE_BIND_FUNCTIONNAME(Thread);
DEFINE_BIND_FUNCTIONNAME(ExitApplication);

//	platform stuff
DEFINE_BIND_FUNCTIONNAME(GetComputerName);
DEFINE_BIND_FUNCTIONNAME(ShowFileInFinder);
DEFINE_BIND_FUNCTIONNAME(EnumScreens);
DEFINE_BIND_FUNCTIONNAME(GetExeDirectory);
DEFINE_BIND_FUNCTIONNAME(GetExeArguments);





const char Image_TypeName[] = "Image";

DEFINE_BIND_FUNCTIONNAME(Alloc);
DEFINE_BIND_FUNCTIONNAME(LoadFile);
DEFINE_BIND_FUNCTIONNAME(Create);
DEFINE_BIND_FUNCTIONNAME(Flip);
DEFINE_BIND_FUNCTIONNAME(GetWidth);
DEFINE_BIND_FUNCTIONNAME(GetHeight);
DEFINE_BIND_FUNCTIONNAME(GetRgba8);
DEFINE_BIND_FUNCTIONNAME(GetPixelBuffer);
DEFINE_BIND_FUNCTIONNAME(SetLinearFilter);
DEFINE_BIND_FUNCTIONNAME(Copy);
DEFINE_BIND_FUNCTIONNAME(WritePixels);
DEFINE_BIND_FUNCTIONNAME(Resize);
DEFINE_BIND_FUNCTIONNAME(Clip);
DEFINE_BIND_FUNCTIONNAME(Clear);
DEFINE_BIND_FUNCTIONNAME(SetFormat);
DEFINE_BIND_FUNCTIONNAME(GetFormat);
DEFINE_BIND_FUNCTIONNAME(GetPngData);

DEFINE_BIND_FUNCTIONNAME(Iteration);

namespace ApiPop
{
	const char Namespace[] = "Pop";
	DEFINE_BIND_TYPENAME(AsyncLoop);

	static void 	Debug(Bind::TCallback& Params);
	static void 	CreateTestPromise(Bind::TCallback& Params);
	static void 	CompileAndRun(Bind::TCallback& Params);
	static void 	LoadFileAsString(Bind::TCallback& Params);
	static void 	LoadFileAsArrayBuffer(Bind::TCallback& Params);
	static void 	WriteStringToFile(Bind::TCallback& Params);
	static void 	WriteToFile(Bind::TCallback& Params);
	static void 	GarbageCollect(Bind::TCallback& Params);
	static void 	SetTimeout(Bind::TCallback& Params);
	static void		Sleep(Bind::TCallback& Params);
	static void		Yield(Bind::TCallback& Params);
	static void		IsDebuggerAttached(Bind::TCallback& Params);
	static void		ExitApplication(Bind::TCallback& Params);
	static void		ThreadTest(Bind::TCallback& Params);
	static void		GetTimeNowMs(Bind::TCallback& Params);
	static void		GetComputerName(Bind::TCallback& Params);
	static void		ShowFileInFinder(Bind::TCallback& Params);
	static void		GetImageHeapSize(Bind::TCallback& Params);
	static void		GetImageHeapCount(Bind::TCallback& Params);
	static void		GetHeapSize(Bind::TCallback& Params);
	static void		GetHeapCount(Bind::TCallback& Params);
	static void		GetHeapObjects(Bind::TCallback& Params);
	static void		GetCrtHeapSize(Bind::TCallback& Params);
	static void		GetCrtHeapCount(Bind::TCallback& Params);
	static void		EnumScreens(Bind::TCallback& Params);
	static void		GetExeDirectory(Bind::TCallback& Params);
	static void		GetExeArguments(Bind::TCallback& Params);
}


void ApiPop::Debug(Bind::TCallback& Params)
{
	for ( auto a=0;	a<Params.GetArgumentCount();	a++ )
	{
		auto Arg = Params.GetArgumentString(a);
		std::Debug << (a==0?"":",") << Arg;
	}
	std::Debug << std::endl;
}


void ApiPop::CreateTestPromise(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise( Params.mLocalContext, __FUNCTION__);
	
	Promise.Resolve( Params.mLocalContext, "Resolved in c++");
	Params.Return( Promise );
}

void ApiPop::GarbageCollect(Bind::TCallback& Params)
{
	Params.mContext.GarbageCollect( Params.GetContextRef() );

}


static void ApiPop::SetTimeout(Bind::TCallback& Params)
{
	auto Callback = Params.GetArgumentFunction(0);
	auto TimeoutMs = Params.GetArgumentInt(1);
	auto CallbackPersistent = Bind::TPersistent( Params.mLocalContext, Callback, "SetTimeout callback");
	
	auto OnRun = [=](Bind::TLocalContext& Context)
	{
		try
		{
			auto Func = CallbackPersistent.GetFunction(Context);
			Bind::TCallback Call( Context );
			Func.Call(Call);
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in SetTimeout(" << TimeoutMs << ") callback: " << e.what() << std::endl;
		}
	};

	Params.mContext.Queue( OnRun, TimeoutMs );
}



static void ApiPop::Yield(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise( Params.mLocalContext, __FUNCTION__);
//#error this promise = in the lambda is crashing as it immediately causes a retain.... but on a very old context???
	auto DelayMs = 0;
	if ( !Params.IsArgumentUndefined(0) )
		DelayMs = Params.GetArgumentInt(0);
	
	auto OnYield = [=](Bind::TLocalContext& Context)
	{
		//	don't need to do anything, we have just let the system breath
		Promise.Resolve( Context, "Yield complete");
	};

	Params.mContext.Queue( OnYield, DelayMs );
	
	Params.Return( Promise );
}


static void ApiPop::IsDebuggerAttached(Bind::TCallback& Params)
{
	auto DebuggerAttached = Platform::IsDebuggerAttached();
	Params.Return( DebuggerAttached );
}

static void ApiPop::ExitApplication(Bind::TCallback& Params)
{
	auto ReturnCode = 0;
	if ( !Params.IsArgumentUndefined(0) )
		ReturnCode = Params.GetArgumentInt(0);

	Params.mContext.Shutdown(ReturnCode);
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
	size_t NowMsInt = NowMs.count();

	static size_t FirstTimestamp = 0;

	//	make time start from 1 day ago to get around 64bit issue
	//	this will lap at 54 days...
	if ( FirstTimestamp == 0 )
	{
		auto OneDayMs = 86400000;
		FirstTimestamp = NowMsInt - OneDayMs;
	}
	NowMsInt -= FirstTimestamp;

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
	auto& Heap = Params.mContext.GetGeneralHeap();
	auto Value = Heap.mAllocBytes;
	Params.Return( Value );
}

void ApiPop::GetHeapCount(Bind::TCallback& Params)
{
	auto& Heap = Params.mContext.GetGeneralHeap();
	auto Value = Heap.mAllocCount;
	Params.Return( Value );
}


void ApiPop::GetCrtHeapSize(Bind::TCallback& Params)
{
	auto& Heap = prmem::GetCRTHeap();
	Heap.Update();
	auto Value = Heap.mAllocBytes;
	Params.Return( Value );
}

void ApiPop::GetCrtHeapCount(Bind::TCallback& Params)
{
	auto& Heap = prmem::GetCRTHeap();
	Heap.Update();
	auto Value = Heap.mAllocCount;
	Params.Return( Value );
}


void ApiPop::GetHeapObjects(Bind::TCallback& Params)
{
	auto& Heap = Params.mContext.GetGeneralHeap();
	auto* pHeapDebug = Heap.GetDebug();
	if ( !pHeapDebug )
		throw Soy::AssertException("Heap doesn't have debug enabled");

	auto& HeapDebug = *pHeapDebug;
	//	get object counts
	std::map<const std::string*,int> TypeCounts;

	auto EnumAlloc = [&](const prmem::HeapDebugItem& Allocation)
	{
		auto* Typename = Allocation.mTypename;
		TypeCounts[Typename] += Allocation.mElements;
		/*
		if ( *Typename == "TImageWrapper" )
		{
			auto* pImage = (TImageWrapper*)(Allocation.mObject);
			std::Debug << pImage->mName << std::endl;
		}
		*/
	};
	HeapDebug.EnumAllocations(EnumAlloc);
	
	auto Object = Params.mContext.CreateObjectInstance( Params.mLocalContext );

	for ( auto it=TypeCounts.begin();	it!=TypeCounts.end();	it++ )
	{
		auto& Name = *it->first;
		auto Count = it->second;
		Object.SetInt( Name, Count );
	}

	//	set persistent info
	{
		auto& ContextDebug = Params.mContext.mDebug;
		for ( auto it=ContextDebug.mPersistentObjectCount.begin();	it!=ContextDebug.mPersistentObjectCount.end();	it++ )
		{
			auto& Name = it->first;
			auto Count = it->second;
			Object.SetInt( std::string("Persistent_") + Name, Count );
		}
	}
	
	try
	{
		auto& DebugHeap = Soy::GetDebugStreamHeap();
		Object.SetInt( "DebugStreamHeapSizeBytes", DebugHeap.GetAllocatedBytes() );

		auto OpenglTextureCount = Opengl::TContext::GetTextureAllocationCount();
		Object.SetInt( "OpenglTextureCount", OpenglTextureCount );

	}
	catch (std::exception& e)
	{
	}
	
	Params.Return( Object );
}



void ApiPop::EnumScreens(Bind::TCallback& Params)
{
	BufferArray<Bind::TObject,20> ScreenMetas;
	auto EnumScreen = [&](const Platform::TScreenMeta& Meta)
	{
		auto Screen = Params.mContext.CreateObjectInstance( Params.mLocalContext );
		Screen.SetString("Name", Meta.mName );
		Screen.SetInt("Left", Meta.mWorkRect.Left() );
		Screen.SetInt("Top", Meta.mWorkRect.Top() );
		Screen.SetInt("Width", Meta.mWorkRect.GetWidth() );
		Screen.SetInt("Height", Meta.mWorkRect.GetHeight() );

		//	may need to supply x&y
		Screen.SetInt("ResolutionWidth", Meta.mFullRect.GetWidth() );
		Screen.SetInt("ResolutionHieght", Meta.mFullRect.GetHeight() );
		
		ScreenMetas.PushBack( Screen );
	};
	Platform::EnumScreens( EnumScreen );
	
	Params.Return( GetArrayBridge(ScreenMetas) );
}




void ApiPop::GetExeDirectory(Bind::TCallback& Params)
{
	//	gr: for the API, on OSX we want the dir the .app is in
	auto Path = Platform::GetExePath();
	if ( Soy::StringTrimRight(Path,".app/Contents/MacOS/",false) )
	{
		Path = Platform::GetDirectoryFromFilename( Path, true );
	}
	
	Params.Return( Path	);
}


void ApiPop::GetExeArguments(Bind::TCallback& Params)
{
	Array<std::string> Arguments;
	Params.mContext.GetExeArguments( GetArrayBridge(Arguments) );
	Params.Return( GetArrayBridge(Arguments) );
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

	//	can't do typed arrays of signed ints, so convert
	auto FileContentsu8 = GetArrayBridge(FileContents).GetSubArray<uint8_t>(0,FileContents.GetDataSize());

	Params.Return( GetArrayBridge(FileContentsu8) );
}



void ApiPop::WriteStringToFile(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	auto Contents = Params.GetArgumentString(1);
	auto Append = !Params.IsArgumentUndefined(2) ? Params.GetArgumentBool(2) : false;
		
	Soy::StringToFile( Filename, Contents, Append );
}

void ApiPop::WriteToFile(Bind::TCallback& Params)
{
	//	write as a string if not a specific binary array
	if ( !Params.IsArgumentArray(1) )
	{
		WriteStringToFile(Params);
		return;
	}
	
	auto Filename = Params.GetArgumentFilename(0);

	//	need to have some generic interface here I think
	//	we dont have the type exposed in Bind yet
	Array<uint8_t> Contents;
	Params.GetArgumentArray( 1, GetArrayBridge(Contents) );

	auto Append = !Params.IsArgumentUndefined(2) ? Params.GetArgumentBool(2) : false;
	if ( Append )
		throw Soy::AssertException("Currently not supporting binary append in WriteToFile()");
	
	auto ContentsChar = GetArrayBridge(Contents).GetSubArray<char>(0,Contents.GetSize());
	Soy::ArrayToFile( GetArrayBridge(ContentsChar), Filename );
}

void ApiPop::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindObjectType<TImageWrapper>( Namespace );
	Context.BindObjectType<TAsyncLoopWrapper>( Namespace );
	
	Context.BindGlobalFunction<CreateTestPromise_FunctionName>( CreateTestPromise, Namespace );
	Context.BindGlobalFunction<Debug_FunctionName>( Debug, Namespace );
	Context.BindGlobalFunction<CompileAndRun_FunctionName>(CompileAndRun, Namespace );
	Context.BindGlobalFunction<LoadFileAsString_FunctionName>(LoadFileAsString, Namespace );
	Context.BindGlobalFunction<LoadFileAsArrayBuffer_FunctionName>(LoadFileAsArrayBuffer, Namespace );
	Context.BindGlobalFunction<WriteStringToFile_FunctionName>(WriteStringToFile, Namespace );
	Context.BindGlobalFunction<WriteToFile_FunctionName>(WriteToFile, Namespace );
	Context.BindGlobalFunction<GarbageCollect_FunctionName>(GarbageCollect, Namespace );
	Context.BindGlobalFunction<SetTimeout_FunctionName>(SetTimeout, Namespace );
	Context.BindGlobalFunction<Sleep_FunctionName>(Sleep, Namespace );
	Context.BindGlobalFunction<Yield_FunctionName>( Yield, Namespace );
	Context.BindGlobalFunction<IsDebuggerAttached_FunctionName>( IsDebuggerAttached, Namespace );
	Context.BindGlobalFunction<ThreadTest_FunctionName>( ThreadTest, Namespace );
	Context.BindGlobalFunction<ExitApplication_FunctionName>( ExitApplication, Namespace );
	Context.BindGlobalFunction<GetTimeNowMs_FunctionName>(GetTimeNowMs, Namespace );
	Context.BindGlobalFunction<GetComputerName_FunctionName>(GetComputerName, Namespace );
	Context.BindGlobalFunction<ShowFileInFinder_FunctionName>(ShowFileInFinder, Namespace );
	Context.BindGlobalFunction<GetImageHeapSize_FunctionName>(GetImageHeapSize, Namespace );
	Context.BindGlobalFunction<GetImageHeapCount_FunctionName>(GetImageHeapCount, Namespace );
	Context.BindGlobalFunction<GetHeapSize_FunctionName>(GetHeapSize, Namespace );
	Context.BindGlobalFunction<GetHeapCount_FunctionName>(GetHeapCount, Namespace );
	Context.BindGlobalFunction<GetHeapObjects_FunctionName>(GetHeapObjects, Namespace );
	Context.BindGlobalFunction<GetCrtHeapSize_FunctionName>(GetCrtHeapSize, Namespace );
	Context.BindGlobalFunction<GetCrtHeapCount_FunctionName>(GetCrtHeapCount, Namespace );
	Context.BindGlobalFunction<EnumScreens_FunctionName>(EnumScreens, Namespace );
	Context.BindGlobalFunction<GetExeDirectory_FunctionName>(GetExeDirectory, Namespace );
	Context.BindGlobalFunction<GetExeArguments_FunctionName>(GetExeArguments, Namespace );
}

TImageWrapper::~TImageWrapper()
{
	Free();
}

void TImageWrapper::Construct(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();

	
	if ( Params.IsArgumentString(1) )
		mName = Params.GetArgumentString(1);
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
	Template.BindFunction<GetPixelBuffer_FunctionName>( GetPixelBuffer );
	Template.BindFunction<SetLinearFilter_FunctionName>( SetLinearFilter );
	Template.BindFunction<Copy_FunctionName>( Copy );
	Template.BindFunction<WritePixels_FunctionName>( WritePixels );
	Template.BindFunction<Resize_FunctionName>( Resize );
	Template.BindFunction<Clip_FunctionName>( Clip );
	Template.BindFunction<Clear_FunctionName>( Clear );
	Template.BindFunction<SetFormat_FunctionName>( SetFormat );
	Template.BindFunction<GetFormat_FunctionName>( GetFormat );
	Template.BindFunction<GetPngData_FunctionName>( &TImageWrapper::GetPngData );
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
	auto& Heap = GetContext().GetImageHeap();
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
	
	auto Format = SoyPixelsFormat::RGBA;
	if ( !Params.IsArgumentUndefined(3) )
	{
		auto FormatStr = Params.GetArgumentString(3);
		Format = SoyPixelsFormat::ToType( FormatStr );
	}
	
	auto* Rgba8 = static_cast<uint8_t*>(Rgba.GetArray());
	auto DataSize = Rgba.GetDataSize();
	SoyPixelsRemote NewPixels( Rgba8, Width, Height, DataSize, Format );
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

void TImageWrapper::GetPngData(Bind::TCallback& Params)
{
	//	todo: do this async, but we don't want a generic thread, or one just for this object
	//		(I don't think so anyway)
	//		lets create a worker/job thread/pool in JS and make the user pass it in, then use
	//		has control over which jobs are on which threads, monitoring, load balancing and
	//		how many. Maybe whenever a thread is passed in, you get a promise back as a general API rule
	auto& CurrentPixels = this->GetPixels();
	std::shared_ptr<SoyPixelsImpl> pPixels;
	if ( CurrentPixels.GetFormat() == SoyPixelsFormat::RGB || CurrentPixels.GetFormat() == SoyPixelsFormat::RGBA )
	{
		pPixels.reset( new SoyPixelsRemote(CurrentPixels) );
	}
	else
	{
		//	copy & change format
		pPixels.reset( new SoyPixels(CurrentPixels) );
		pPixels->SetFormat( SoyPixelsFormat::RGBA );
	}
	auto& Pixels = *pPixels;
	
	Array<char> PngDataChar;
	auto PngDataCharBridge = GetArrayBridge(PngDataChar);
	Array<uint8_t> ExifData;
	if ( Params.IsArgumentArray(0) )
	{
		Params.GetArgumentArray( 0, GetArrayBridge(ExifData) );
	}
	else if ( !Params.IsArgumentUndefined(0) )
	{
		auto ExifString = Params.GetArgumentString(0);
		Soy::StringToArray( ExifString, GetArrayBridge(ExifData) );
	}
	
	if ( ExifData.IsEmpty() )
		TPng::GetPng( Pixels, PngDataCharBridge );
	else
		TPng::GetPng( Pixels, PngDataCharBridge, GetArrayBridge(ExifData) );

	auto PngData8 = PngDataCharBridge.GetSubArray<uint8_t>(0,PngDataChar.GetSize());
	
	Params.Return( GetArrayBridge(PngData8) );
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
		Params.Return( GetArrayBridge(PixelsArray) );
	}
}


void TImageWrapper::GetPixelBuffer(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	
	auto IsTargetArray = Params.IsArgumentArray(1);
	auto& Heap = Params.mContext.GetImageHeap();
	
	Soy::TScopeTimerPrint Timer(__func__,5);
	
	auto& Pixels = This.GetPixels();
	auto& PixelsArray = Pixels.GetPixelsArray();
	
	if ( IsTargetArray )
	{
		auto TargetArray = Params.GetArgumentArray(1);
		TargetArray.CopyTo( GetArrayBridge(PixelsArray) );
		Params.Return( TargetArray );
	}
	else
	{
		//	for float & 16 bit formats, convert to their proper javascript type
		auto ComponentSize = SoyPixelsFormat::GetBytesPerChannel(Pixels.GetFormat());
		if (ComponentSize == 2)
		{
			auto Pixels16 = GetArrayBridge(PixelsArray).GetSubArray<uint16_t>(0, PixelsArray.GetSize() / 2);
			Params.Return(GetArrayBridge(Pixels16));
		}
		else
		{
			Params.Return(GetArrayBridge(PixelsArray));
		}
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


std::shared_ptr<Opengl::TTexture> TImageWrapper::GetTexturePtr()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Texture = GetTexture();
	return mOpenglTexture;
}

void TImageWrapper::GetTexture(Opengl::TContext& Context,std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError)
{
	//std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

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
			UploadParams.mAllowClientStorage = false;
			
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
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

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
	auto& Heap = GetContext().GetImageHeap();

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


void TImageWrapper::OnOpenglTextureChanged(Opengl::TContext& Context)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	if ( !mOpenglTexture )
		throw Soy::AssertException("OnOpenglChanged with null texture");

	//	is now latest version
	auto LatestVersion = GetLatestVersion();
	mOpenglTextureVersion = LatestVersion+1;
	auto TextureSlot = Context.mCurrentTextureSlot++;
	mOpenglTexture->Bind(TextureSlot);
	mOpenglTexture->RefreshMeta();
}



void TImageWrapper::OnPixelsChanged()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto LatestVersion = GetLatestVersion();
	mPixelsVersion = LatestVersion+1;
}

void TImageWrapper::SetPixels(const SoyPixelsImpl& NewPixels)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	auto& Heap = GetContext().GetImageHeap();
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

	auto& Heap = GetContext().GetImageHeap();

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





void TAsyncLoopWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<Iteration_FunctionName>( Iteration );
}
	
void TAsyncLoopWrapper::Construct(Bind::TCallback& Params)
{
	auto Function = Params.GetArgumentFunction(0);
	mFunction = Bind::TPersistent( Params.mLocalContext, Function, "TAsyncLoopWrapper function" );

	
	static Bind::TPersistent MakeIterationBindThisFunction;
	if ( !MakeIterationBindThisFunction )
	{
		auto* FunctionSource =  R"V0G0N(
			let MakeThisFunction = function(This)
			{
				return This.Iteration.bind(This);
			}
			MakeThisFunction;
		)V0G0N";

		auto mContext = Params.GetContextRef();
		JSStringRef FunctionSourceString = JsCore::GetString( mContext, FunctionSource );
		JSValueRef Exception = nullptr;
		auto FunctionValue = JSEvaluateScript( mContext, FunctionSourceString, nullptr, nullptr, 0, &Exception );
		JsCore::ThrowException( mContext, Exception );
		
		Bind::TFunction MakePromiseFunction( mContext, FunctionValue );
		//mIterationBindThisFunction = Bind::TPersistent( MakePromiseFunction, "MakePromiseFunction" );
	
		MakeIterationBindThisFunction = Bind::TPersistent(Params.mLocalContext, MakePromiseFunction,"MakeIterationBindThisFunction");
	}
	
	{
		auto This = GetHandle(Params.mLocalContext);
		Bind::TCallback Call( Params.mLocalContext );
		Call.SetArgumentObject(0,This);
		auto MakeFunc = MakeIterationBindThisFunction.GetFunction(Params.mLocalContext);
		MakeFunc.Call(Call);
		
		auto IterationBindThisFunction = Call.GetReturnFunction();
		this->mIterationBindThisFunction = Bind::TPersistent(Params.mLocalContext, IterationBindThisFunction,"Iteration Func");
	}
	
	Iteration( Params );
}

void TAsyncLoopWrapper::Iteration(Bind::TCallback& Params)
{
	//std::Debug << "Iteration()" << std::endl;
	
	auto* pThis = &Params.This<TAsyncLoopWrapper>();
	auto Execute = [=](Bind::TLocalContext& Context)
	{
		auto ThisHandle = pThis->GetHandle(Context);
		auto ThisIterationFunction = pThis->mIterationBindThisFunction.GetFunction(Context);
		
		//	run the func, get the promise returned
		//	append to then() something to run another iteration
		JsCore::TCallback IterationCall(Context);
		auto Func = pThis->mFunction.GetFunction(Context);
		//std::Debug << "AsyncFunc()" << std::endl;
		Func.Call(IterationCall);
		auto Promise = IterationCall.GetReturnObject();
		
		//	get the then function
		auto ThenFunc = Promise.GetFunction("then");
		JsCore::TCallback ThenCall(Context);
		ThenCall.SetThis( Promise );
		ThenCall.SetArgumentFunction(0,ThisIterationFunction);
		//std::Debug << "then()" << std::endl;
		ThenFunc.Call( ThenCall );
	};
	Params.mContext.Queue( Execute );
	//std::Debug << "Context Queue Size: " << Params.mContext.mJobQueue.GetJobCount() << std::endl;
}

