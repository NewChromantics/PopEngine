#include "TApiCommon.h"
#include "SoyDebug.h"
#include "SoyImageProxy.h"
#include "SoyFilesystem.h"
#include "SoyStream.h"
#if defined(ENABLE_OPENGL)
#include "SoyOpengl.h"
#include "SoyOpenglContext.h"
#endif
#include "TBind.h"
#include <SoyWindow.h>
#include <SoyPng.h>
#include <SoyShellExecute.h>
#include <SoyMedia.h>

//	stdout/err stuff
#include <iostream>
#if defined(TARGET_WINDOWS)
#include <io.h>
#include <fcntl.h>
#endif


namespace ApiPop
{
	const char Namespace[] = "Pop";
	DEFINE_BIND_TYPENAME(AsyncLoop);
	DEFINE_BIND_TYPENAME(Image);
	DEFINE_BIND_TYPENAME(FileMonitor);
	DEFINE_BIND_TYPENAME(ShellExecute);

	static void 	Debug(Bind::TCallback& Params);
	static void 	Warning(Bind::TCallback& Params);
	static void 	StdOut(Bind::TCallback& Params);
	static void 	StdErr(Bind::TCallback& Params);
	static void 	CreateTestPromise(Bind::TCallback& Params);
	static void		ImportAsync(Bind::TCallback& Params);	//	html import()
	static void		ImportSync(Bind::TCallback& Params);	//	node require()
	static void 	CompileAndRun(Bind::TCallback& Params);
	static void		FileExists(Bind::TCallback& Params);
	static void 	LoadFileAsString(Bind::TCallback& Params);
	static void 	LoadFileAsStringAsync(Bind::TCallback& Params);
	static void 	LoadFileAsImage(Bind::TCallback& Params);
	static void 	LoadFileAsImageAsync(Bind::TCallback& Params);
	static void 	LoadFileAsArrayBuffer(Bind::TCallback& Params);
	static void 	LoadFileAsArrayBufferAsync(Bind::TCallback& Params);
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
	static void		GetExeFilename(Bind::TCallback& Params);
	static void		GetExeDirectory(Bind::TCallback& Params);
	static void		GetExeArguments(Bind::TCallback& Params);
	static void		GetPlatform(Bind::TCallback& Params);
	static void		ShellOpen(Bind::TCallback& Params);
	static void		ShowWebPage(Bind::TCallback& Params);

	//	system stuff
	DEFINE_BIND_FUNCTIONNAME(FileExists);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsString);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsStringAsync);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsArrayBuffer);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsArrayBufferAsync);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsImage);
	DEFINE_BIND_FUNCTIONNAME(LoadFileAsImageAsync);
	DEFINE_BIND_FUNCTIONNAME(WriteStringToFile);
	DEFINE_BIND_FUNCTIONNAME(WriteToFile);
	DEFINE_BIND_FUNCTIONNAME(GetFilenames);
	DEFINE_BIND_FUNCTIONNAME(SetTimeout);		//	web-compatible call, should really use await Pop.Yield()
	DEFINE_BIND_FUNCTIONNAME(GetTimeNowMs);		//	returns a relative time, as javascript can't handle 64bit int. Need to rename this to something like GetTimeRelativeMs()
	DEFINE_BIND_FUNCTIONNAME(GetComputerName);
	DEFINE_BIND_FUNCTIONNAME(ShowFileInFinder);
	DEFINE_BIND_FUNCTIONNAME(EnumScreens);
	DEFINE_BIND_FUNCTIONNAME(GetExeFilename);
	DEFINE_BIND_FUNCTIONNAME(GetExeDirectory);
	DEFINE_BIND_FUNCTIONNAME(GetExeArguments);
	DEFINE_BIND_FUNCTIONNAME(GetPlatform);
	DEFINE_BIND_FUNCTIONNAME(ShellOpen);
	DEFINE_BIND_FUNCTIONNAME(ShowWebPage);

	//	engine stuff
	DEFINE_BIND_FUNCTIONNAME(CompileAndRun);
	DEFINE_BIND_FUNCTIONNAME(import);
	DEFINE_BIND_FUNCTIONNAME(require);
	DEFINE_BIND_FUNCTIONNAME(CreateTestPromise);
	DEFINE_BIND_FUNCTIONNAME(Debug);
	DEFINE_BIND_FUNCTIONNAME(Warning);
	DEFINE_BIND_FUNCTIONNAME(StdErr);
	DEFINE_BIND_FUNCTIONNAME(StdOut);
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
	
	//	TFileMonitor
	DEFINE_BIND_FUNCTIONNAME(Add);
	DEFINE_BIND_FUNCTIONNAME(WaitForChange);	
}

template<typename STREAMTYPE>	//	ostream
void PrintToStream(STREAMTYPE& Stream,Bind::TCallback& Params)
{
	for (auto a = 0; a < Params.GetArgumentCount(); a++)
	{
		auto IsUndefined = Params.IsArgumentUndefined(a);
		auto Arg = IsUndefined ? "Undefined" : Params.GetArgumentString(a);
		Stream << (a == 0 ? "" : ",") << Arg;
	}
	Stream << std::endl;
}

void ApiPop::Debug(Bind::TCallback& Params)
{
	PrintToStream(std::Debug, Params);
}

void ApiPop::StdErr(Bind::TCallback& Params)
{
	PrintToStream(std::cerr, Params);
}


void ApiPop::Warning(Bind::TCallback& Params)
{
	std::Debug << "Warning: ";
	PrintToStream(std::Debug, Params);
}


void ApiPop::StdOut(Bind::TCallback& Params)
{
	//	if there is one binary argument for std out, we write directly as binary
	//	for binary output
	//	gr: any typed array?
	if (!Params.IsArgumentArrayU8(0))
	{
		PrintToStream(std::cout, Params);
		return;
	}

	Array<uint8_t> Binary;
	Params.GetArgumentArray(0, GetArrayBridge(Binary));

	//	gr: is this the safest way to definitely get binary out?
	//	on windows, stdout IS a file*
	FILE* FileHandle = stdout;

	//	make sure file is binary on windows (unix is always binary)
#if defined(TARGET_WINDOWS)
	//	from https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setmode?view=vs-2017
	//	flush before changing mode
	{
		auto Error = fflush(FileHandle);
		Platform::IsOkay(Error, "fflush stdout before set mode");
		auto FileDescriptor = fileno(FileHandle);
		auto PreviousMode = setmode(FileDescriptor, O_BINARY);
		if ( PreviousMode == -1 )
			Platform::ThrowLastError("setmode stdout O_BINARY");
	}
#endif
	auto WrittenBytes = fwrite(Binary.GetArray(), 1, Binary.GetDataSize(), FileHandle);
	//	if not all bytes were written, there's an error
	if (WrittenBytes != Binary.GetDataSize())
	{
		auto FileError = ferror(FileHandle);
		std::stringstream Error;
		Error << "fwrite(" << Binary.GetDataSize() << ") wrote " << WrittenBytes << " (FileError=" << FileError << ")";
		Platform::IsOkay(FileError, Error.str());
		//	if the above doesnt throw, still throw
		throw Soy::AssertException(Error);
	}

	//	gr: flush? close?
	{
		auto Error = fflush(FileHandle);
		Platform::IsOkay(Error, "fflush stdout after write");
	}
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
				/ *
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
	//	this time will always >32bit, so we could either switch to uptime
	//	or just & 32 bit...
	//SoyTime Now(true);
	SoyTime Now = SoyTime::UpTime();

	auto NowMs = Now.GetMilliSeconds();
	size_t NowMsInt = NowMs.count();

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

#if defined(ENABLE_OPENGL)
		auto OpenglTextureCount = Opengl::TContext::GetTextureAllocationCount();
		Object.SetInt( "OpenglTextureCount", OpenglTextureCount );
#endif
	}
	catch (std::exception& e)
	{
	}
	
	Params.Return( Object );
}

#if defined(TARGET_UWP)
//	gr: need to move win32 stuff out of SoyOpenglWindow
void Platform::EnumScreens(std::function<void(Platform::TScreenMeta&)> EnumScreen)
{
}
#endif

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



void ApiPop::GetExeFilename(Bind::TCallback& Params)
{
	auto Path = Platform::GetExeFilename();
	Params.Return( Path	);
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
	Platform::GetExeArguments(GetArrayBridge(Arguments));
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
#elif defined(TARGET_LINUX)
	Params.Return("Linux");
#elif defined(TARGET_ANDROID)
	Params.Return("Android");
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

	auto& Context = Params.mContext;
	//	create a global this
	//	add a .exports object and return that as the module
	//auto Global = Context.CreateObjectInstance(Params.mLocalContext);
	auto Global = Context.GetGlobalObject(Params.mLocalContext);
	auto Exports = Context.CreateObjectInstance(Params.mLocalContext);
	Global.SetObject("exports", Exports);
	
	Context.LoadScript( Source, Filename, Global );
	
	Params.Return(Exports);
}

void ApiPop::ImportAsync(Bind::TCallback& Params)
{
	auto Promise = Params.mContext.CreatePromise( Params.mLocalContext, std::string(__FUNCTION__) );
	Params.Return( Promise );

	auto OnModuleLoaded = [=](Bind::TLocalContext& Context,Bind::TObject& Module)
	{
		Promise.Resolve(Context,Module);
	};
	
	auto OnError = [=](Bind::TLocalContext& Context,const std::string& Error)
	{
		Promise.Reject(Context,Error);
	};

	//	should we throw instantly or error go to reject
	try
	{
		auto Filename = Params.GetArgumentFilename(0);
		Params.mContext.LoadModule( Filename, OnModuleLoaded, OnError );
	}
	catch(std::exception& e)
	{
		OnError( Params.mLocalContext, e.what() );
	}	
}


void ApiPop::ImportSync(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	
	auto OnModuleLoaded = [&](Bind::TLocalContext& Context,Bind::TObject& Exports)
	{
		Params.Return(Exports);
	};
	
	auto OnError = [&](Bind::TLocalContext& Context,const std::string& Error)
	{
		std::Debug << "Module error " << Error << std::endl;
	};
	
	Params.mContext.LoadModule( Filename, OnModuleLoaded, OnError );
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


void ApiPop::LoadFileAsStringAsync(Bind::TCallback& Params)
{
	LoadFileAsString(Params);
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


void ApiPop::LoadFileAsImageAsync(Bind::TCallback& Params)
{
	LoadFileAsImage(Params);
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


void ApiPop::LoadFileAsArrayBufferAsync(Bind::TCallback& Params)
{
	LoadFileAsArrayBuffer(Params);
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
	auto Append = !Params.IsArgumentUndefined(2) ? Params.GetArgumentBool(2) : false;

	std::string Filename = Params.GetArgumentFilename(0);

	//	make sure directory is created if it's a new filename
	auto Directory = Platform::GetDirectoryFromFilename(Filename);
	Platform::CreateDirectory(Directory);

	Array<uint8_t> Contents;
	auto ContentsArgumentIndex = 1;

	//	gr: let's allow string as binary
	if (Params.IsArgumentString(ContentsArgumentIndex))
	{
		auto ContentsString = Params.GetArgumentString(ContentsArgumentIndex);
		auto* ContentsStringData = ContentsString.c_str();
		auto ContentsStringLength = ContentsString.length();
		auto ContentsStringArray = GetRemoteArray(reinterpret_cast<const uint8_t*>(ContentsStringData), ContentsStringLength);
		Contents.PushBackArray(ContentsStringArray);
	}
	else if ( !Params.IsArgumentArray(ContentsArgumentIndex) )
	{
		//	write as a string if not a specific binary array
		if ( Append )
			throw Soy::AssertException("Currently cannot append file with string");
		WriteStringToFile(Params);
		return;
	}
	else
	{
		Params.GetArgumentArray(ContentsArgumentIndex, GetArrayBridge(Contents));
	}

	Soy::ArrayToFile( GetArrayBridge(Contents), Filename, Append );
}


void ApiPop::GetFilenames(Bind::TCallback& Params)
{
	//	if no directory, list all files at root
	std::string Directory = Params.mContext.GetResolvedFilename("");
	if ( !Params.IsArgumentUndefined(0) )
	{
		Directory = Params.GetArgumentFilename(0);
	}

	//	correct directory name (ie, add slash if it's a dir, and remove filename if it's a file)
	Directory = Platform::GetDirectoryFromFilename(Directory);
	
	//	recurse
	auto SearchDirectory = Directory + "**";
	
	//	os list all files
	Array<std::string> Filenames;
	auto EnumFile = [&](const std::string& FilePath)
	{
		//	gr: should this be relative to original dir, or context dir...
		//		for asset server, we want it relative to input dir.
		//		if this changes, start adding param options
		auto& Filename = Filenames.PushBack(FilePath);
		
		//	cut directory off if it starts with it
		if ( !Soy::StringTrimLeft(Filename,Directory,true) )
			std::Debug << "Warning, FilePath " << FilePath << " was not prefixed with directory(" << Directory << ") as expected, full path returned" << std::endl;
	};
	
	Platform::EnumFiles( SearchDirectory, EnumFile );

	Params.Return( GetArrayBridge(Filenames) );
}
	
void ApiPop::Bind(Bind::TContext& Context)
{
	//	import is a replacement for import()
	//	gr: this doesn't work in jsc, symbol doesn't get overwritten
	//Context.BindGlobalFunction<BindFunction::import>(Import);

	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindObjectType<TImageWrapper>( Namespace );
	Context.BindObjectType<TAsyncLoopWrapper>( Namespace );
	Context.BindObjectType<TFileMonitorWrapper>(Namespace);
	Context.BindObjectType<TShellExecuteWrapper>(Namespace);

	Context.BindGlobalFunction<BindFunction::CreateTestPromise>( CreateTestPromise, Namespace );
	Context.BindGlobalFunction<BindFunction::Debug>( Debug, Namespace );
	Context.BindGlobalFunction<BindFunction::Warning>(Warning, Namespace);
	Context.BindGlobalFunction<BindFunction::StdOut>(StdOut, Namespace);
	Context.BindGlobalFunction<BindFunction::StdErr>(StdErr, Namespace );
	Context.BindGlobalFunction<BindFunction::CompileAndRun>(CompileAndRun, Namespace );
	Context.BindGlobalFunction<BindFunction::import>(ImportAsync, Namespace );
	Context.BindGlobalFunction<BindFunction::require>(ImportSync, Namespace );
	Context.BindGlobalFunction<BindFunction::FileExists>(FileExists, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsString>(LoadFileAsString, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsStringAsync>(LoadFileAsStringAsync, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsImage>(LoadFileAsImage, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsImageAsync>(LoadFileAsImageAsync, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsArrayBuffer>(LoadFileAsArrayBuffer, Namespace );
	Context.BindGlobalFunction<BindFunction::LoadFileAsArrayBufferAsync>(LoadFileAsArrayBufferAsync, Namespace );
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
	Context.BindGlobalFunction<BindFunction::GetExeFilename>(GetExeFilename, Namespace );
	Context.BindGlobalFunction<BindFunction::GetExeDirectory>(GetExeDirectory, Namespace );
	Context.BindGlobalFunction<BindFunction::GetExeArguments>(GetExeArguments, Namespace );
	Context.BindGlobalFunction<BindFunction::GetPlatform>(GetPlatform, Namespace);
	Context.BindGlobalFunction<BindFunction::ShellOpen>(ShellOpen, Namespace);
	Context.BindGlobalFunction<BindFunction::ShowWebPage>(ShowWebPage, Namespace);
}

TImageWrapper::~TImageWrapper()
{
}

void TImageWrapper::Construct(Bind::TCallback& Params)
{
	mImage.reset(new SoyImageProxy());

	//	matching with web api
	if (Params.IsArgumentString(0))
	{
		mImage->mName = Params.GetArgumentString(0);
	}
	else
	{
		mImage->mName = "Pop.Image";
	}

	bool IsFilename = false;
	auto& Filename = mImage->mName;
	if (Params.IsArgumentString(0))
		if (Soy::StringContains(Filename, ".",true))
			IsFilename = true;

	if (IsFilename)
	{
		//	webapi does 
		//		HtmlImage = Pop.GetCachedAsset(Filename);
		//		WritePixels(HtmlImage)
		//	so we will attempt to load filename
		LoadFile(Params);
	}
	else if (Params.IsArgumentArray(0))
	{
		/*
		Pop.Debug("Init image with size", Filename);
		const Size = arguments[0];
		const PixelFormat = arguments[1] || 'RGBA';
		const Width = Size[0];
		const Height = Size[1];
		let PixelData = new Array(Width * Height * 4);
		PixelData.fill(0);
		const Pixels = IsFloatFormat(PixelFormat) ? new Float32Array(PixelData) : new Uint8Array(PixelData);
		this.WritePixels(Width, Height, Pixels, PixelFormat);
		*/
		Alloc(Params);
	}
	else if (Params.IsArgumentString(0))
	{
		//	just a name
	}
	else if (!Params.IsArgumentUndefined(0))
	{
		throw Soy::AssertException("Unhandled Pop.Image constructor");
	}	
}

void TImageWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	using namespace ApiPop;

	Template.BindFunction<BindFunction::Alloc>( &TImageWrapper::Alloc );
	Template.BindFunction<BindFunction::LoadFile>( &TImageWrapper::LoadFile );
	Template.BindFunction<BindFunction::Flip>( &TImageWrapper::Flip );
	Template.BindFunction<BindFunction::GetWidth>( &TImageWrapper::GetWidth );
	Template.BindFunction<BindFunction::GetHeight>( &TImageWrapper::GetHeight );
	Template.BindFunction<BindFunction::GetRgba8>( &TImageWrapper::GetRgba8 );
	Template.BindFunction<BindFunction::GetPixelBuffer>( &TImageWrapper::GetPixelBuffer );
	Template.BindFunction<BindFunction::SetLinearFilter>( &TImageWrapper::SetLinearFilter );
	Template.BindFunction<BindFunction::Copy>( &TImageWrapper::Copy );
	Template.BindFunction<BindFunction::WritePixels>( &TImageWrapper::WritePixels );
	Template.BindFunction<BindFunction::Resize>( &TImageWrapper::Resize );
	Template.BindFunction<BindFunction::Clip>( &TImageWrapper::Clip );
	Template.BindFunction<BindFunction::Clear>( &TImageWrapper::Clear );
	Template.BindFunction<BindFunction::SetFormat>( &TImageWrapper::SetFormat );
	Template.BindFunction<BindFunction::GetFormat>( &TImageWrapper::GetFormat );
	Template.BindFunction<BindFunction::GetPngData>( &TImageWrapper::GetPngData );
}

SoyImageProxy& TImageWrapper::GetImage()
{
	if ( !mImage )
		throw Soy::AssertException("Image has no SoyImageProxy (use after free)");
	return *mImage;
}

void TImageWrapper::Flip(Bind::TCallback& Params)
{
	auto& Image = GetImage();
	Image.Flip();
}


void TImageWrapper::Alloc(Bind::TCallback& Params)
{
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
	auto& Image = GetImage();
	Image.SetPixels( Temp );
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
	
	GetImage().LoadFile( Filename, OnMetaFound );
}



void TImageWrapper::Copy(Bind::TCallback& Params)
{
	auto& This = Params.This<TImageWrapper>();
	auto& That = Params.GetArgumentPointer<TImageWrapper>(0);

	auto& ThisImage = This.GetImage();
	auto& ThatImage = That.GetImage();
	ThisImage.Copy(ThatImage);
}

void TImageWrapper::WritePixels(Bind::TCallback& Params)
{
	auto& Image = GetImage();
	auto Width = Params.GetArgumentInt(0);
	auto Height = Params.GetArgumentInt(1);
	
	auto Format = SoyPixelsFormat::RGBA;
	if ( !Params.IsArgumentUndefined(3) )
	{
		auto FormatStr = Params.GetArgumentString(3);
		Format = SoyPixelsFormat::ToType( FormatStr );
	}
	
	if ( SoyPixelsFormat::IsFloatChannel(Format) )
	{
		Array<float> Floats;
		Params.GetArgumentArray(2, GetArrayBridge(Floats) );
		auto Floats8 = GetArrayBridge(Floats).GetSubArray<uint8_t>( 0, Floats.GetDataSize() );
		auto DataSize = Floats8.GetDataSize();
		SoyPixelsRemote NewPixels(Floats8.GetArray(), Width, Height, DataSize, Format );
		Image.SetPixels(NewPixels);
	}
	else if ( SoyPixelsFormat::GetBytesPerChannel(Format) == sizeof(uint8_t) )
	{
		Array<uint8_t> PixelBuffer8;
		Params.GetArgumentArray(2,GetArrayBridge(PixelBuffer8) );
		auto DataSize = PixelBuffer8.GetDataSize();
		SoyPixelsRemote NewPixels( PixelBuffer8.GetArray(), Width, Height, DataSize, Format );
		Image.SetPixels(NewPixels);
	}
	else
	{
		std::stringstream Error;
		Error << "Format for pixels which is not float or 8bit, not handled";
		throw Soy_AssertException(Error);
	}
}



void TImageWrapper::Resize(Bind::TCallback& Params)
{
	auto NewWidth = Params.GetArgumentInt(0);
	auto NewHeight = Params.GetArgumentInt(1);

	GetImage().Resize(NewWidth,NewHeight);
}


void TImageWrapper::Clear(Bind::TCallback& Params)
{
	GetImage().Free();
	//mImage.reset();
}



void TImageWrapper::Clip(Bind::TCallback& Params)
{
	Soy::TScopeTimerPrint Timer(__func__,5);

	BufferArray<int,4> RectPx;

	Params.GetArgumentArray( 0, GetArrayBridge(RectPx) );
	
	if ( RectPx.GetSize() != 4 )
	{
		std::stringstream Error;
		Error << "Expected 4 values for cliping rect (got " << RectPx.GetSize() << ")";
		throw Soy::AssertException(Error.str());
	}
	
	auto& Image = GetImage();
	Image.Clip(RectPx[0],RectPx[1],RectPx[2],RectPx[3]);
}


void TImageWrapper::SetFormat(Bind::TCallback& Params)
{
	auto FormatName = Params.GetArgumentString(0);
	auto NewFormat = SoyPixelsFormat::ToType(FormatName);
	
	GetImage().SetFormat(NewFormat);
}

void TImageWrapper::GetFormat(Bind::TCallback& Params)
{
	auto Meta = GetImage().GetMeta();
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
	Array<uint8_t> PngData;
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
		TPng::GetPng( Pixels, GetArrayBridge(PngData), CompressionLevel );
	else
		TPng::GetPng( Pixels, GetArrayBridge(PngData), CompressionLevel, GetArrayBridge(ExifData) );
	
	Params.Return( GetArrayBridge(PngData) );
}

void TImageWrapper::GetWidth(Bind::TCallback& Params)
{
	auto Meta = GetImage().GetMeta();
	Params.Return( Meta.GetWidth() );
}


void TImageWrapper::GetHeight(Bind::TCallback& Params)
{
	auto Meta = GetImage().GetMeta();
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
	//auto& Heap = Params.mContext.GetImageHeap();
	
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
		if ( ComponentSize == 4 )
		{
			auto Pixelsf = GetArrayBridge(PixelsArray).GetSubArray<float>(0, PixelsArray.GetSize() / ComponentSize);
			Params.Return(GetArrayBridge(Pixelsf));
		}
		else if (ComponentSize == 2)
		{
			auto Pixels16 = GetArrayBridge(PixelsArray).GetSubArray<uint16_t>(0, PixelsArray.GetSize() / ComponentSize);
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
	GetImage().SetLinearFilter( LinearFilter );
}

SoyPixelsImpl& TImageWrapper::GetPixels()
{
	return GetImage().GetPixels();
}

void TImageWrapper::GetPixels(SoyPixelsImpl& CopyTarget)
{
	return GetImage().GetPixels(CopyTarget);
}

void TImageWrapper::SetPixels(std::shared_ptr<SoyPixelsImpl> Pixels)
{
	return GetImage().SetPixels(Pixels);
}

Opengl::TTexture& TImageWrapper::GetTexture()
{
	return GetImage().GetTexture();
}

std::shared_ptr<Opengl::TTexture> TImageWrapper::GetTexturePtr()
{
	return GetImage().GetTexturePtr();
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
	Template.BindFunction<BindFunction::Add>(&TFileMonitorWrapper::Add);
	Template.BindFunction<BindFunction::WaitForChange>(&TFileMonitorWrapper::WaitForChange);
}

void ApiPop::TFileMonitorWrapper::Construct(Bind::TCallback& Params)
{
	this->mChangedFileQueue.mResolveObject = [this](Bind::TLocalContext& Context,Bind::TPromise& Promise,std::string& Filename)
	{
		Promise.Resolve( Context, Filename );
	};
	
	Add(Params);
}

void ApiPop::TFileMonitorWrapper::Add(Bind::TCallback& Params)
{
	for (auto i = 0; i < Params.GetArgumentCount(); i++)
	{
		auto Filename = Params.GetArgumentString(i);
		Add(Filename);
	}
}


void ApiPop::TFileMonitorWrapper::Add(const std::string& Filename)
{
	//	make monitor
	auto FileMonitor = std::make_shared<Platform::TFileMonitor>(Filename);
	FileMonitor->mOnChanged = [=](const std::string& Filename) {	this->OnChanged(Filename); };
	mMonitors.PushBack(FileMonitor);
}


void ApiPop::TFileMonitorWrapper::OnChanged(const std::string& Filename)
{
	mChangedFileQueue.Push(Filename);
}

void ApiPop::TFileMonitorWrapper::WaitForChange(Bind::TCallback& Params)
{
	auto Promise = mChangedFileQueue.AddPromise(Params.mLocalContext);
	Params.Return(Promise);
}


void ApiPop::TShellExecuteWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::WaitForExit>(&TShellExecuteWrapper::WaitForExit);
	Template.BindFunction<BindFunction::WaitForOutput>(&TShellExecuteWrapper::WaitForOutput);
}

void ApiPop::TShellExecuteWrapper::Construct(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentString(0);
	Array<std::string> Arguments;
	if ( !Params.IsArgumentUndefined(1) )
		Params.GetArgumentArray(1, GetArrayBridge(Arguments));

	auto OnExit = std::bind(&TShellExecuteWrapper::OnExit, this, std::placeholders::_1);
	auto OnStdOut = std::bind(&TShellExecuteWrapper::OnStdOut, this, std::placeholders::_1);
	auto OnStdErr = std::bind(&TShellExecuteWrapper::OnStdErr, this, std::placeholders::_1);
	mShellExecute.reset(new Soy::TShellExecute(Filename, GetArrayBridge(Arguments), OnExit, OnStdOut, OnStdErr));
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
