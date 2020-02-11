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


namespace ApiPop
{
	const char Namespace[] = "Pop";
	DEFINE_BIND_TYPENAME(AsyncLoop);
	DEFINE_BIND_TYPENAME(Image);
	DEFINE_BIND_TYPENAME(FileMonitor);
	DEFINE_BIND_TYPENAME(ShellExecute);

	static void 	Debug(Bind::TCallback& Params);
	static void 	CreateTestPromise(Bind::TCallback& Params);
	static void 	CompileAndRun(Bind::TCallback& Params);
	static void		FileExists(Bind::TCallback& Params);
	static void 	LoadFileAsString(Bind::TCallback& Params);
	static void 	LoadFileAsImage(Bind::TCallback& Params);
	static void 	LoadFileAsArrayBuffer(Bind::TCallback& Params);
	static void 	WriteStringToFile(Bind::TCallback& Params);
	static void 	WriteToFile(Bind::TCallback& Params);
	static void 	GetFilenames(Bind::TCallback& Params);
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
	static void		GetPlatform(Bind::TCallback& Params);
	static void		ShellOpen(Bind::TCallback& Params);
	static void		ShowWebPage(Bind::TCallback& Params);

	//	system stuff
	DEFINE_BIND_FUNCTIONNAME(FileExists);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsString);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsArrayBuffer);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsImage);
	DEFINE_BIND_FUNCTIONNAME(WriteStringToFile);
	DEFINE_BIND_FUNCTIONNAME(WriteToFile);
	DEFINE_BIND_FUNCTIONNAME(GetFilenames);
	DEFINE_BIND_FUNCTIONNAME(SetTimeout);		//	web-compatible call, should really use await Pop.Yield()
	DEFINE_BIND_FUNCTIONNAME(GetTimeNowMs);		//	returns a relative time, as javascript can't handle 64bit int. Need to rename this to something like GetTimeRelativeMs()
	DEFINE_BIND_FUNCTIONNAME(GetComputerName);
	DEFINE_BIND_FUNCTIONNAME(ShowFileInFinder);
	DEFINE_BIND_FUNCTIONNAME(EnumScreens);
	DEFINE_BIND_FUNCTIONNAME(GetExeDirectory);
	DEFINE_BIND_FUNCTIONNAME(GetExeArguments);
	DEFINE_BIND_FUNCTIONNAME(GetPlatform);
	DEFINE_BIND_FUNCTIONNAME(ShellOpen);
	DEFINE_BIND_FUNCTIONNAME(ShowWebPage);

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

	//	TImageWrapper
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

	//	TAsyncLoop
	DEFINE_BIND_FUNCTIONNAME(Iteration);

	//	TShellExecute
	DEFINE_BIND_FUNCTIONNAME(WaitForExit);
	DEFINE_BIND_FUNCTIONNAME(WaitForOutput);

}


void ApiPop::Debug(Bind::TCallback& Params)
{
	for ( auto a=0;	a<Params.GetArgumentCount();	a++ )
	{
		auto IsUndefined = Params.IsArgumentUndefined(a);
		auto Arg = IsUndefined ? "Undefined" : Params.GetArgumentString(a);
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
#if !defined(JSAPI_CHAKRA)
#error Test this without the deduction, chakra seems okay with SoyTime(true)
#endif
		//FirstTimestamp = NowMsInt - OneDayMs;
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

void ApiPop::GetPlatform(Bind::TCallback& Params)
{
#if defined(TARGET_WINDOWS)
	Params.Return("Windows");
#elif defined(TARGET_OSX)
	Params.Return("Osx");
#elif defined(TARGET_IOS)
	Params.Return("Ios");
#else
#error Undefined platform
#endif
}

void ApiPop::ShellOpen(Bind::TCallback& Params)
{
	//	todo: support current working dir, params, verbs etc (depending on what OSX supports)
	auto Command = Params.GetArgumentString(0);
	Platform::ShellExecute(Command);
}

void ApiPop::ShowWebPage(Bind::TCallback& Params)
{
	auto Url = Params.GetArgumentString(0);
	Platform::ShellOpenUrl(Url);
}



void ApiPop::CompileAndRun(Bind::TCallback& Params)
{
	auto Source = Params.GetArgumentString(0);
	auto Filename = Params.GetArgumentString(1);

	//	ignore the return for now
	Params.mContext.LoadScript( Source, Filename );
}



void ApiPop::FileExists(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	
	auto Exists = Platform::FileExists( Filename );
	Params.Return( Exists );
}



void ApiPop::LoadFileAsString(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	
	std::string Contents;
	{
		Soy::TScopeTimerPrint Timer( (std::string("Loading file[string] ") + Filename).c_str(),5);
		Soy::FileToString( Filename, Contents);
	}
	Params.Return( Contents );
}


void ApiPop::LoadFileAsImage(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);

	//	alloc an image and load
	auto& Context = Params.mContext;
	auto ImageObject = Context.CreateObjectInstance( Params.mLocalContext, TImageWrapper::GetTypeName() );
	auto& Image = ImageObject.This<TImageWrapper>();
	Image.LoadFile( Params );

	Params.Return( ImageObject );
}



void ApiPop::LoadFileAsArrayBuffer(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);

	Array<char> FileContents;
	{
		Soy::TScopeTimerPrint Timer( (std::string("Loading file[binary] ") + Filename).c_str(),5);
		Soy::FileToArray( GetArrayBridge(FileContents), Filename );
	}
	
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


void ApiPop::GetFilenames(Bind::TCallback& Params)
{
	//	if no directory, list all files
	std::string Directory = Params.mContext.GetResolvedFilename("");
	if ( !Params.IsArgumentUndefined(0) )
		Directory = Params.GetArgumentFilename(0);
	
	//	recurse
	Directory += "/**";
	
	//	os list all files
	Array<std::string> Filenames;
	auto EnumFile = [&](const std::string& Filename)
	{
		//	todo: make filename an api-relative filename
		Filenames.PushBack(Filename);
	};
	
	Platform::EnumFiles( Directory, EnumFile );

	Params.Return( GetArrayBridge(Filenames) );
}
	
void ApiPop::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindObjectType<TImageWrapper>( Namespace );
	Context.BindObjectType<TAsyncLoopWrapper>( Namespace );
	Context.BindObjectType<TFileMonitorWrapper>(Namespace);
	Context.BindObjectType<TShellExecuteWrapper>(Namespace);

	Context.BindGlobalFunction<BindFunction::CreateTestPromise>( CreateTestPromise, Namespace );
	Context.BindGlobalFunction<BindFunction::Debug>( Debug, Namespace );
	Context.BindGlobalFunction<BindFunction::CompileAndRun>(CompileAndRun, Namespace );
	Context.BindGlobalFunction<BindFunction::FileExists>(FileExists, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsString>(LoadFileAsString, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsImage>(LoadFileAsImage, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsArrayBuffer>(LoadFileAsArrayBuffer, Namespace );
	Context.BindGlobalFunction<BindFunction::WriteStringToFile>(WriteStringToFile, Namespace );
	Context.BindGlobalFunction<BindFunction::WriteToFile>(WriteToFile, Namespace );
	Context.BindGlobalFunction<BindFunction::GetFilenames>(GetFilenames, Namespace );
	Context.BindGlobalFunction<BindFunction::GarbageCollect>(GarbageCollect, Namespace );
	Context.BindGlobalFunction<BindFunction::SetTimeout>(SetTimeout, Namespace );
	Context.BindGlobalFunction<BindFunction::Sleep>(Sleep, Namespace );
	Context.BindGlobalFunction<BindFunction::Yield>( Yield, Namespace );
	Context.BindGlobalFunction<BindFunction::IsDebuggerAttached>( IsDebuggerAttached, Namespace );
	Context.BindGlobalFunction<BindFunction::ThreadTest>( ThreadTest, Namespace );
	Context.BindGlobalFunction<BindFunction::ExitApplication>( ExitApplication, Namespace );
	Context.BindGlobalFunction<BindFunction::GetTimeNowMs>(GetTimeNowMs, Namespace );
	Context.BindGlobalFunction<BindFunction::GetComputerName>(GetComputerName, Namespace );
	Context.BindGlobalFunction<BindFunction::ShowFileInFinder>(ShowFileInFinder, Namespace );
	Context.BindGlobalFunction<BindFunction::GetImageHeapSize>(GetImageHeapSize, Namespace );
	Context.BindGlobalFunction<BindFunction::GetImageHeapCount>(GetImageHeapCount, Namespace );
	Context.BindGlobalFunction<BindFunction::GetHeapSize>(GetHeapSize, Namespace );
	Context.BindGlobalFunction<BindFunction::GetHeapCount>(GetHeapCount, Namespace );
	Context.BindGlobalFunction<BindFunction::GetHeapObjects>(GetHeapObjects, Namespace );
	Context.BindGlobalFunction<BindFunction::GetCrtHeapSize>(GetCrtHeapSize, Namespace );
	Context.BindGlobalFunction<BindFunction::GetCrtHeapCount>(GetCrtHeapCount, Namespace );
	Context.BindGlobalFunction<BindFunction::EnumScreens>(EnumScreens, Namespace );
	Context.BindGlobalFunction<BindFunction::GetExeDirectory>(GetExeDirectory, Namespace );
	Context.BindGlobalFunction<BindFunction::GetExeArguments>(GetExeArguments, Namespace );
	Context.BindGlobalFunction<BindFunction::GetPlatform>(GetPlatform, Namespace);
	Context.BindGlobalFunction<BindFunction::ShellOpen>(ShellOpen, Namespace);
	Context.BindGlobalFunction<BindFunction::ShowWebPage>(ShowWebPage, Namespace);
}

TImageWrapper::~TImageWrapper()
{
	Free();
}

void TImageWrapper::Construct(Bind::TCallback& Params)
{
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
	using namespace ApiPop;

	Template.BindFunction<BindFunction::Alloc>( Alloc );
	Template.BindFunction<BindFunction::LoadFile>( &TImageWrapper::LoadFile );
	Template.BindFunction<BindFunction::Flip>( Flip );
	Template.BindFunction<BindFunction::GetWidth>( GetWidth );
	Template.BindFunction<BindFunction::GetHeight>( GetHeight );
	Template.BindFunction<BindFunction::GetRgba8>( GetRgba8 );
	Template.BindFunction<BindFunction::GetPixelBuffer>( GetPixelBuffer );
	Template.BindFunction<BindFunction::SetLinearFilter>( SetLinearFilter );
	Template.BindFunction<BindFunction::Copy>( Copy );
	Template.BindFunction<BindFunction::WritePixels>( WritePixels );
	Template.BindFunction<BindFunction::Resize>( Resize );
	Template.BindFunction<BindFunction::Clip>( Clip );
	Template.BindFunction<BindFunction::Clear>( Clear );
	Template.BindFunction<BindFunction::SetFormat>( SetFormat );
	Template.BindFunction<BindFunction::GetFormat>( GetFormat );
	Template.BindFunction<BindFunction::GetPngData>( &TImageWrapper::GetPngData );
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
	auto Filename = Params.GetArgumentFilename(0);

	auto OnMetaFound = [&](const std::string& Section,const ArrayBridge<uint8_t>& Data)
	{
		auto This = Params.ThisObject();
		
		//	add meta if it's not there
		if ( !This.HasMember("Meta") )
			This.SetObjectFromString("Meta","{}");
		
		//	set this section as a meta
		auto ThisMeta = This.GetObject("Meta");
		if ( This.HasMember(Section) )
			std::Debug << "Overwriting image meta section " << Section << std::endl;
	
		ThisMeta.SetArray( Section, Data );
	};
	
	DoLoadFile( Filename, OnMetaFound );
}

void TImageWrapper::DoLoadFile(const std::string& Filename,std::function<void(const std::string&,const ArrayBridge<uint8_t>&)> OnMetaFound)
{
	//	gr: feels like this function should be a generic soy thing
	
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
		Png::Read( *NewPixels, BytesBuffer, OnMetaFound );
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
	
	auto Format = SoyPixelsFormat::RGBA;
	if ( !Params.IsArgumentUndefined(3) )
	{
		auto FormatStr = Params.GetArgumentString(3);
		Format = SoyPixelsFormat::ToType( FormatStr );
	}
	
	Array<uint8_t> PixelBuffer8;
	if ( SoyPixelsFormat::IsFloatChannel(Format) )
	{
		Array<float> Floats;
		Params.GetArgumentArray(2, GetArrayBridge(Floats) );
		auto Floats8 = GetArrayBridge(Floats).GetSubArray<uint8_t>( 0, Floats.GetDataSize() );
		PixelBuffer8.Copy( Floats8 );
	}
	else if ( SoyPixelsFormat::GetBytesPerChannel(Format) == sizeof(uint8_t) )
	{
		Params.GetArgumentArray(2,GetArrayBridge(PixelBuffer8) );
	}
	else
	{
		std::stringstream Error;
		Error << "Format for pixels which is not float or 8bit, not handled";
		throw Soy_AssertException(Error);
	}
	
	auto DataSize = PixelBuffer8.GetDataSize();
	auto* Pixels = PixelBuffer8.GetArray();
	SoyPixelsRemote NewPixels( Pixels, Width, Height, DataSize, Format );
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
	
	//	require compression level
	float CompressionLevel = Params.GetArgumentFloat(0);
	
	//	encode exif data!
	Array<char> PngDataChar;
	auto PngDataCharBridge = GetArrayBridge(PngDataChar);
	Array<uint8_t> ExifData;
	if ( Params.IsArgumentArray(1) )
	{
		Params.GetArgumentArray( 1, GetArrayBridge(ExifData) );
	}
	else if ( !Params.IsArgumentUndefined(1) )
	{
		auto ExifString = Params.GetArgumentString(1);
		Soy::StringToArray( ExifString, GetArrayBridge(ExifData) );
	}
	
	if ( ExifData.IsEmpty() )
		TPng::GetPng( Pixels, PngDataCharBridge, CompressionLevel );
	else
		TPng::GetPng( Pixels, PngDataCharBridge, CompressionLevel, GetArrayBridge(ExifData) );

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

void TImageWrapper::SetOpenglTexture(const Opengl::TAsset& Texture)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	//	todo: delete old texture
	mOpenglTexture.reset(new Opengl::TTexture(Texture.mName, SoyPixelsMeta(), GL_TEXTURE_2D));
	auto LatestVersion = GetLatestVersion();
	mOpenglTextureVersion = LatestVersion + 1;
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
	Template.BindFunction<ApiPop::BindFunction::Iteration>( Iteration );
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




void ApiPop::TFileMonitorWrapper::CreateTemplate(Bind::TTemplate& Template)
{
}

void ApiPop::TFileMonitorWrapper::Construct(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);

	mFileMonitor.reset( new Platform::TFileMonitor( Filename ) );
	mFileMonitor->mOnChanged = std::bind( &TFileMonitorWrapper::OnChanged, this );
}

void ApiPop::TFileMonitorWrapper::OnChanged()
{
	auto Callback = [this](Bind::TLocalContext& Context)
	{
		auto This = this->GetHandle(Context);
		auto ThisOnChanged = This.GetFunction("OnChanged");
		JsCore::TCallback Callback(Context);
		ThisOnChanged.Call( Callback );
	};
	this->mContext.Queue( Callback );
}


#if defined(TARGET_WINDOWS)
class TReadWritePipe
{
public:
	TReadWritePipe();
	~TReadWritePipe();

	void	StartReadThread(std::function<void(const std::string&)>& OnRead);

protected:
	void	CloseHandles();

public:
	HANDLE	mReadPipe = nullptr;
	HANDLE	mWritePipe = nullptr;
	std::shared_ptr<SoyThreadLambda>	mReadThread;
};
#endif

//	gr: this might be nice as an RAII wrapper, but for JS, I want the initial creation to throw, 
//		and if it was RAII, i'd need a thread on the wrapper, which wouldn't immediately throw
//		unless we have a semaphore or something to indicate it has started
class Platform::TShellExecute : public SoyThread
{
public:
	TShellExecute(const std::string& RunCommand, std::function<void(int)> OnExit, std::function<void(const std::string&)> OnStdOut, std::function<void(const std::string&)> OnStdErr);
	~TShellExecute();

protected:
	virtual void				Thread() override;
	void						CreateProcessHandle(const std::string& Command);
	void						WaitForProcessHandle();

	std::function<void(int32_t)>	mOnExit;
	std::function<void(const std::string&)>	mOnStdOut;
	std::function<void(const std::string&)>	mOnStdErr;
	int32_t							mExitCode = 0;
#if defined(TARGET_WINDOWS)
	PROCESS_INFORMATION				mProcessInfo;
	std::shared_ptr<TReadWritePipe>	mStdOutPipe;
	std::shared_ptr<TReadWritePipe>	mStdErrPipe;
#endif
};


Platform::TShellExecute::TShellExecute(const std::string& RunCommand, std::function<void(int)> OnExit, std::function<void(const std::string&)> OnStdOut, std::function<void(const std::string&)> OnStdErr) :
	SoyThread	(std::string("ShellExecute ") + RunCommand ),
	mOnExit		(OnExit),
	mOnStdOut	(OnStdOut),
	mOnStdErr	(OnStdErr)
{
	//	throw here if we can't create the process
	CreateProcessHandle( RunCommand );
	Start();
}

Platform::TShellExecute::~TShellExecute()
{
	//	need to add Kill() here to kill any process as we're currently waiting with Infinite timeout
	Stop(true);
}

void Platform::TShellExecute::Thread()
{
	WaitForProcessHandle();
	if (mOnExit)
	{
		mOnExit(mExitCode);
	}
	else
	{
		std::Debug << "Process exiteed with " << mExitCode << std::endl;
	}
	Stop(false);
}

#if defined(TARGET_WINDOWS)
TReadWritePipe::TReadWritePipe()
{
	// Create a pipe for the redirection of the STDOUT 
	// of a child process. 
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = true;
	saAttr.lpSecurityDescriptor = nullptr;

	auto bSuccess = CreatePipe(&mReadPipe, &mWritePipe, &saAttr, 0);
	if (!bSuccess)
		Platform::ThrowLastError("CreatePipe(StdOut");

	bSuccess = SetHandleInformation(mReadPipe, HANDLE_FLAG_INHERIT, 0);
	if (!bSuccess)
		Platform::ThrowLastError("SetHandleInformation(StdOut");

}
#endif

#if defined(TARGET_WINDOWS)
TReadWritePipe::~TReadWritePipe()
{
	if (mReadThread)
	{
		mReadThread->Stop(false);

		//	can't call CloseHandle from a different thread as ReadFile is blocked, 
		//	all I/O block (expects single threaded)
		//	so CloseHandle will hang whilst ReadFile hangs
		//	instead kill anything on that thread (the read) and we should be able to interrupt
		if (!CancelSynchronousIo(mReadThread->GetThreadNativeHandle()))
		{
			//	gr: this will fail if nothing is currently blocking
			auto Error = Platform::GetLastErrorString();
			std::Debug << "CancelSynchronousIo TReadWritePipe ReadThread error " << Error << std::endl;
		}
		//	gr: we can get a race condition here, where the loop has looped around and ReadFile is blocking again
		CloseHandles();

		//	wait for thread to end (OnOutput callback might still be executing)
		mReadThread->WaitToFinish();
		mReadThread.reset();
	}
	
	CloseHandles();
}
#endif

#if defined(TARGET_WINDOWS)
void TReadWritePipe::CloseHandles()
{
	//	CloseHandle throws an exception if closed twice, so we have to be safe
	if (mReadPipe)
	{
		CloseHandle(mReadPipe);
		mReadPipe = nullptr;
	}
	
	if (mWritePipe)
	{
		CloseHandle(mWritePipe);
		mWritePipe = nullptr;
	}
}
#endif

#if defined(TARGET_WINDOWS)
void TReadWritePipe::StartReadThread(std::function<void(const std::string&)>& OnRead)
{
	auto ReadThread = [this, OnRead]()
	{
		DWORD BytesAvailible = 0;
		DWORD BytesLeft = 0;
		if (!PeekNamedPipe(mReadPipe, nullptr, 0, nullptr, &BytesAvailible, &BytesLeft))
			Platform::ThrowLastError("ReadWritePipe PeekNamedPipe");

		CHAR Buffer[1024];
		DWORD BytesRead = 0;
		auto Success = ::ReadFile(this->mReadPipe, Buffer, std::size(Buffer), &BytesRead, NULL);
		//	https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
		//	Note  The GetLastError code ERROR_IO_PENDING is not a failure; it designates the read operation is pending completion asynchronously. For more information, see Remarks.
		if (!Success)
			Platform::ThrowLastError("ReadWritePipe ReadFile");

		if (BytesRead == 0)
		{
			std::Debug << "ReadWritePipe ReadFile returned 0 bytes read" << std::endl;
			return true;
		}
		//std::Debug << "Pipe read " << BytesRead << "/" << BytesAvailible << "/" << BytesLeft << " bytes" << std::endl;
			
		//	split by line?
		std::string Line(Buffer, BytesRead);
		OnRead(Line);
		return true;
	};
	this->mReadThread.reset(new SoyThreadLambda("ReadPipe",ReadThread));
}
#endif

void Platform::TShellExecute::CreateProcessHandle(const std::string& RunCommand)
{
#if defined(TARGET_WINDOWS)
	//	https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
	mStdOutPipe.reset(new TReadWritePipe);
	mStdErrPipe.reset(new TReadWritePipe);

	// Setup the child process to use the STDOUT redirection
	PROCESS_INFORMATION& piProcInfo = mProcessInfo;
	STARTUPINFO siStartInfo;
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	
	siStartInfo.hStdError = mStdErrPipe->mWritePipe;
	siStartInfo.hStdOutput = mStdOutPipe->mWritePipe;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	

	auto* RunCommandStr = const_cast<char*>(RunCommand.c_str());

	// Execute a synchronous child process & get exit code
	auto Success = CreateProcessA(nullptr,
		RunCommandStr,  // command line 
		nullptr,                 // process security attributes 
		nullptr,                 // primary thread security attributes 
		true,                 // handles are inherited 
		0,                    // creation flags 
		nullptr,                 // use parent's environment 
		nullptr,                 // use parent's current directory 
		&siStartInfo,         // STARTUPINFO pointer 
		&piProcInfo);        // receives PROCESS_INFORMATION
	
	std::Debug << "Started process " << RunCommand << "..." << std::endl;
	if (Success)
		return;

	//	get last error
	Platform::IsOkay("CreateProcess");

	//	if that didn't throw like it should, just throw anyway
	throw Soy::AssertException("CreateProcess failed (No LastError?)");
#else
	Soy_AssertTodo();
#endif
}



void Platform::TShellExecute::WaitForProcessHandle()
{
#if defined(TARGET_WINDOWS)
	//	start async read-pipe/file threads
	mStdOutPipe->StartReadThread(this->mOnStdOut);
	mStdErrPipe->StartReadThread(this->mOnStdErr);

	auto Timeout = INFINITE;	//	 (DWORD)(-1L)
	DWORD ExitCode = 0;
	WaitForSingleObject(mProcessInfo.hProcess, Timeout);
	GetExitCodeProcess(mProcessInfo.hProcess, &ExitCode);
	CloseHandle(mProcessInfo.hProcess);
	CloseHandle(mProcessInfo.hThread);

	//	these should have auto ended by now
	mStdOutPipe.reset();
	mStdErrPipe.reset();

	/*
	// Read the data written to the pipe
		DWORD bytesInPipe = 0;
		while (bytesInPipe == 0) {
			bSuccess = PeekNamedPipe(g_hChildStd_OUT_Rd, NULL, 0, NULL,
				&bytesInPipe, NULL);
			if (!bSuccess) return bSuccess;
		}
		if (bytesInPipe == 0) return TRUE;
		DWORD dwRead;
		CHAR *pipeContents = new CHAR[bytesInPipe];
		bSuccess = ReadFile(g_hChildStd_OUT_Rd, pipeContents,
			bytesInPipe, &dwRead, NULL);
		if (!bSuccess || dwRead == 0) return FALSE;

		// Split the data into lines and add them to the return vector
		std::stringstream stream(pipeContents);
		std::string str;
		while (getline(stream, str))
			stdOutLines->push_back(CStringW(str.c_str()));
	*/
#else
	Soy_AssertTodo();
#endif
}




void ApiPop::TShellExecuteWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::WaitForExit>(&TShellExecuteWrapper::WaitForExit);
	Template.BindFunction<BindFunction::WaitForOutput>(&TShellExecuteWrapper::WaitForOutput);
}

void ApiPop::TShellExecuteWrapper::Construct(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentString(0);

	auto OnExit = std::bind(&TShellExecuteWrapper::OnExit, this, std::placeholders::_1);
	auto OnStdOut = std::bind(&TShellExecuteWrapper::OnStdOut, this, std::placeholders::_1);
	auto OnStdErr = std::bind(&TShellExecuteWrapper::OnStdErr, this, std::placeholders::_1);
	mShellExecute.reset(new Platform::TShellExecute(Filename, OnExit, OnStdOut, OnStdErr));
}

void ApiPop::TShellExecuteWrapper::WaitForExit(Bind::TCallback& Params)
{
	auto Promise = mWaitForExitPromises.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	FlushPending();
}


void ApiPop::TShellExecuteWrapper::WaitForOutput(Bind::TCallback& Params)
{
	auto Promise = mWaitForOutputPromises.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	FlushPending();
}

void ApiPop::TShellExecuteWrapper::FlushPendingOutput()
{
	if (!mWaitForOutputPromises.HasPromises())
		return;

	//	gr: maybe this should output an array of strings so we can pump out a lot at once
	std::string NextOutput;
	{
		std::lock_guard<std::mutex> Lock(mMetaLock);
		if (mPendingOutput.IsEmpty())
			return;

		//	concat?
		NextOutput = mPendingOutput.PopAt(0);
	}
	
	//	flush with string
	auto Flush = [this, NextOutput](Bind::TLocalContext& Context)
	{
		auto HandlePromise = [this, NextOutput](Bind::TLocalContext& LocalContext, Bind::TPromise& Promise)
		{
			Promise.Resolve(LocalContext, NextOutput);
		};
		mWaitForOutputPromises.Flush(HandlePromise);
	};
	auto& Context = mWaitForOutputPromises.GetContext();
	Context.Queue(Flush);
}

void ApiPop::TShellExecuteWrapper::FlushPending()
{
	//	if there is pending output, flush that first so we get outputs done
	FlushPendingOutput();

	//	ideally we lock, but JS is in single process, so this might be okay...
	//	the output promises are flushed if there was output,
	//	so now, if there are any left, they will flush with exit code

	//	process still running
	if (!mHasExited)
		return;


	auto FlushWithExit = [&](Bind::TPromiseQueue& Queue)
	{
		//	no pending promises
		if (!Queue.HasPromises())
			return;

		auto* pQueue = &Queue;
		auto Flush = [this, pQueue](Bind::TLocalContext& Context)
		{
			auto HandlePromise = [this](Bind::TLocalContext& LocalContext, Bind::TPromise& Promise)
			{
				Promise.Resolve(LocalContext, mExitedCode);
			};
			pQueue->Flush(HandlePromise);
		};
		auto& Context = Queue.GetContext();
		Context.Queue(Flush);
	};

	FlushWithExit(mWaitForOutputPromises);
	FlushWithExit(mWaitForExitPromises);
}

void ApiPop::TShellExecuteWrapper::OnExit(int32_t ExitCode)
{
	{
		std::lock_guard<std::mutex> Lock(mMetaLock);
		mExitedCode = ExitCode;

		//	kill process (should already be gone, but this will set our "state" as, not running
		//	gr: this is currently being called FROM the thread, so gets locked
		//		we don't really wanna detatch the thread and could be left with a dangling thread
		//mShellExecute.reset();
		mHasExited = true;
	}
	FlushPending();
}

void ApiPop::TShellExecuteWrapper::OnStdErr(const std::string& Output)
{
	{
		std::lock_guard<std::mutex> Lock(mMetaLock);
		mPendingOutput.PushBack(Output);
	}
	FlushPendingOutput();
}

void ApiPop::TShellExecuteWrapper::OnStdOut(const std::string& Output)
{
	{
		std::lock_guard<std::mutex> Lock(mMetaLock);
		mPendingOutput.PushBack(Output);
	}
	FlushPendingOutput();
}
