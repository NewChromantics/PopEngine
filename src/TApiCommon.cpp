#include "TApiCommon.h"
#include <SoyDebug.h>
#include <SoyImage.h>
#include <SoyFilesystem.h>
#include <SoyStream.h>
#include <SoyOpengl.h>
#include <SoyOpenglContext.h>
#include <SoyMedia.h>

using namespace v8;

const char Debug_FunctionName[] = "Debug";
const char CompileAndRun_FunctionName[] = "CompileAndRun";
const char LoadFileAsString_FunctionName[] = "LoadFileAsString";
const char LoadFileAsArrayBuffer_FunctionName[] = "LoadFileAsArrayBuffer";
const char WriteStringToFile_FunctionName[] = "WriteStringToFile";
const char GarbageCollect_FunctionName[] = "GarbageCollect";
const char SetTimeout_FunctionName[] = "setTimeout";
const char Sleep_FunctionName[] = "Sleep";
const char GetComputerName_FunctionName[] = "GetComputerName";
const char ShowFileInFinder_FunctionName[] = "ShowFileInFinder";
const char GetImageHeapSize_FunctionName[] = "GetImageHeapSize";
const char GetImageHeapCount_FunctionName[] = "GetImageHeapCount";
const char GetV8HeapSize_FunctionName[] = "GetV8HeapSize";
const char GetV8HeapCount_FunctionName[] = "GetV8HeapCount";




const char Image_TypeName[] = "Image";

const char LoadFile_FunctionName[] = "Load";
const char Alloc_FunctionName[] = "Create";
const char Flip_FunctionName[] = "Flip";
const char GetWidth_FunctionName[] = "GetWidth";
const char GetHeight_FunctionName[] = "GetHeight";
const char GetRgba8_FunctionName[] = "GetRgba8";
const char SetLinearFilter_FunctionName[] = "SetLinearFilter";
const char Copy_FunctionName[] = "Copy";
const char Resize_FunctionName[] = "Resize";
const char Clip_FunctionName[] = "Clip";
const char Clear_FunctionName[] = "Clear";
const char SetFormat_FunctionName[] = "SetFormat";
const char GetFormat_FunctionName[] = "GetFormat";



static v8::Local<v8::Value> Debug(v8::CallbackInfo& Params);
static v8::Local<v8::Value> CompileAndRun(v8::CallbackInfo& Params);
static v8::Local<v8::Value> LoadFileAsString(v8::CallbackInfo& Params);
static v8::Local<v8::Value> LoadFileAsArrayBuffer(v8::CallbackInfo& Params);
static v8::Local<v8::Value> WriteStringToFile(v8::CallbackInfo& Params);
static v8::Local<v8::Value> GarbageCollect(v8::CallbackInfo& Params);
static v8::Local<v8::Value> SetTimeout(v8::CallbackInfo& Params);
static v8::Local<v8::Value> Sleep(v8::CallbackInfo& Params);
static v8::Local<v8::Value> GetComputerName(v8::CallbackInfo& Params);
static v8::Local<v8::Value> ShowFileInFinder(v8::CallbackInfo& Params);
static v8::Local<v8::Value> GetImageHeapSize(v8::CallbackInfo& Params);
static v8::Local<v8::Value> GetImageHeapCount(v8::CallbackInfo& Params);
static v8::Local<v8::Value> GetV8HeapSize(v8::CallbackInfo& Params);
static v8::Local<v8::Value> GetV8HeapCount(v8::CallbackInfo& Params);


void ApiCommon::Bind(TV8Container& Container)
{
	//  load api's before script & executions
	Container.BindGlobalFunction<Debug_FunctionName>(Debug);
	Container.BindGlobalFunction<CompileAndRun_FunctionName>(CompileAndRun);
	Container.BindGlobalFunction<LoadFileAsString_FunctionName>(LoadFileAsString);
	Container.BindGlobalFunction<LoadFileAsArrayBuffer_FunctionName>(LoadFileAsArrayBuffer);
	Container.BindGlobalFunction<WriteStringToFile_FunctionName>(WriteStringToFile);
	Container.BindGlobalFunction<GarbageCollect_FunctionName>(GarbageCollect);
	Container.BindGlobalFunction<SetTimeout_FunctionName>(SetTimeout);
	Container.BindGlobalFunction<Sleep_FunctionName>(Sleep);
	Container.BindGlobalFunction<GetComputerName_FunctionName>(GetComputerName);
	Container.BindGlobalFunction<ShowFileInFinder_FunctionName>(ShowFileInFinder);
	Container.BindGlobalFunction<GetImageHeapSize_FunctionName>(GetImageHeapSize);
	Container.BindGlobalFunction<GetImageHeapCount_FunctionName>(GetImageHeapCount);
	Container.BindGlobalFunction<GetV8HeapSize_FunctionName>(GetV8HeapSize);
	Container.BindGlobalFunction<GetV8HeapCount_FunctionName>(GetV8HeapCount);

	Container.BindObjectType( TImageWrapper::GetObjectTypeName(), TImageWrapper::CreateTemplate, TV8ObjectWrapperBase::Allocate<TImageWrapper> );
}

static Local<Value> Debug(CallbackInfo& Params)
{
	auto& args = Params.mParams;
	
	if (args.Length() < 1)
	{
		throw Soy::AssertException("log() with no args");
	}
	
	HandleScope scope(Params.mIsolate);
	for ( auto i=0;	i<args.Length();	i++ )
	{
		auto arg = args[i];
		String::Utf8Value value(arg);
		std::Debug << *value << std::endl;
	}
	
	return Undefined(Params.mIsolate);
}


static Local<Value> GarbageCollect(CallbackInfo& Params)
{
	//auto& args = Params.mParams;
	
	//	queue as job?
	{
		HandleScope scope(Params.mIsolate);
		std::Debug << "Invoking garbage collection..." << std::endl;
		Params.GetIsolate().RequestGarbageCollectionForTesting( v8::Isolate::kFullGarbageCollection );
	}
	
	return Undefined(Params.mIsolate);
}


static Local<Value> SetTimeout(CallbackInfo& Params)
{
	auto Callback = v8::SafeCast<Function>(Params.mParams[0]);
	auto TimeoutMsHandle = v8::SafeCast<Number>(Params.mParams[1]);
	auto TimeoutMs = TimeoutMsHandle->Uint32Value();
	auto CallbackPersistent = std::make_shared<V8Storage<Function>>( Params.GetIsolate(), Callback );

	auto* Container = &Params.mContainer;
	
	auto OnRun = [=](Local<v8::Context> Context)
	{
		auto& Isolate = *Context->GetIsolate();
		//auto CallbackLocal = v8::GetLocal( Isolate, CallbackPersistent->mPersistent );
		BufferArray<v8::Local<v8::Value>,1> Args;
		Local<Object> This;
		try
		{
			Container->ExecuteFunc( Context, CallbackPersistent->GetLocal(Isolate), This, GetArrayBridge(Args) );
		}
		catch(std::exception& e)
		{
			std::Debug << "Exception in SetTimeout(" << TimeoutMs << ") callback: " << e.what() << std::endl;
		}
	};
	//	need a persistent handle to the callback?
	Params.mContainer.QueueDelayScoped( OnRun, TimeoutMs );

	//	web normally returns a handle that can be cancelled
	return Undefined(Params.mIsolate);
}



static Local<Value> Sleep(CallbackInfo& Params)
{
	auto TimeoutMsHandle = v8::SafeCast<Number>(Params.mParams[0]);
	auto TimeoutMs = TimeoutMsHandle->Uint32Value();
	
	Params.mContainer.Yield( TimeoutMs );
	
	return Undefined(Params.mIsolate);
}



static Local<Value> GetComputerName(CallbackInfo& Params)
{
	auto Name = ::Platform::GetComputerName();
	auto NameHandle = v8::GetString( Params.GetIsolate(), Name );

	return NameHandle;
}



static Local<Value> ShowFileInFinder(CallbackInfo& Params)
{
	auto FilenameHandle = Params.mParams[0];
	auto Filename = Params.GetRootDirectory() + v8::GetString(FilenameHandle);
	
	::Platform::ShowFileExplorer(Filename);
	
	return Undefined(Params.mIsolate);
}


static Local<Value> GetImageHeapSize(CallbackInfo& Params)
{
	auto& Heap = Params.mContainer.GetImageHeap();
	auto Value = Heap.mAllocBytes;
	auto ValueHandle = v8::Number::New( &Params.GetIsolate(), Value );
	return ValueHandle;
}

static Local<Value> GetImageHeapCount(CallbackInfo& Params)
{
	auto& Heap = Params.mContainer.GetImageHeap();
	auto Value = Heap.mAllocCount;
	auto ValueHandle = v8::Number::New( &Params.GetIsolate(), Value );
	return ValueHandle;
}


static Local<Value> GetV8HeapSize(CallbackInfo& Params)
{
	auto& Heap = Params.mContainer.GetV8Heap();
	auto Value = Heap.mAllocBytes;
	auto ValueHandle = v8::Number::New( &Params.GetIsolate(), Value );
	return ValueHandle;
}

static Local<Value> GetV8HeapCount(CallbackInfo& Params)
{
	auto& Heap = Params.mContainer.GetV8Heap();
	auto Value = Heap.mAllocCount;
	auto ValueHandle = v8::Number::New( &Params.GetIsolate(), Value );
	return ValueHandle;
}





static Local<Value> CompileAndRun(CallbackInfo& Params)
{
	auto& args = Params.mParams;
	
	if (args.Length() != 1)
	{
		throw Soy::AssertException("Expected source as first argument");
	}
	
	auto Source = Local<String>::Cast( args[0] );

	return Params.mContainer.LoadScript( Params.mContext, Source, "RuntimeFile" );
}



static Local<Value> LoadFileAsString(CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	if (Arguments.Length() < 1)
		throw Soy::AssertException("LoadFileAsString(Filename) with no args");

	auto Filename = Params.GetRootDirectory() + v8::GetString( Arguments[0] );
	std::string Contents;
	Soy::FileToString( Filename, Contents);
	
	auto ContentsString = v8::GetString( Params.GetIsolate(), Contents );
	return ContentsString;
}


static Local<Value> LoadFileAsArrayBuffer(CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	if (Arguments.Length() < 1)
		throw Soy::AssertException("LoadFileAsArrayBuffer(Filename) with no args");

	
	auto Filename = Params.GetRootDirectory() + v8::GetString( Arguments[0] );
	Array<char> FileContents;
	Soy::FileToArray( GetArrayBridge(FileContents), Filename );
	auto FileContentsu8 = GetArrayBridge(FileContents).GetSubArray<uint8_t>(0,FileContents.GetDataSize());

	//	gr: way too slow to set for big files.
	//	make a typed array
	auto ArrayBuffer = v8::GetTypedArray( Params.GetIsolate(), GetArrayBridge(FileContentsu8) );
/*
	auto ArrayBuffer = v8::ArrayBuffer::New( Params.mIsolate, FileContents.GetSize() );
	auto& Isolate = *Params.mIsolate;
	
	//	like v8::GetArray
	auto& Values = FileContents;
	auto& ArrayHandle = ArrayBuffer;
	for ( auto i=0;	i<Values.GetSize();	i++ )
	{
		double Value = Values[i];
		auto ValueHandle = Number::New( &Isolate, Value );
		ArrayHandle->Set( i, ValueHandle );
	}
*/
	return ArrayBuffer;
}



static Local<Value> WriteStringToFile(CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	if (Arguments.Length() < 2)
	{
		std::stringstream Error;
		Error << "WriteStringToFile(Filename,String,[Append]) " << Arguments.Length() << " args";
		throw Soy::AssertException(Error.str());
	}
	auto FilenameHandle = Arguments[0];
	auto ContentsHandle = Arguments[1];
	auto AppendHandle = Arguments[2];

	auto Filename = Params.GetRootDirectory() + v8::GetString( FilenameHandle );
	auto Contents = v8::GetString( ContentsHandle );
	bool Append = false;
	if ( AppendHandle->IsBoolean() )
		Append = v8::SafeCast<v8::Boolean>(AppendHandle)->BooleanValue();
	
	Soy::StringToFile( Filename, Contents, Append );

	return v8::Undefined(Params.mIsolate);
}


TImageWrapper::~TImageWrapper()
{
	/*
	try
	{
		std::Debug << "~TImageWrapper " << mName << ", " << this->GetMeta() << std::endl;
	}
	catch(std::exception& e)
	{
		std::Debug << "~TImageWrapper " << mName << ", unknown meta (" << e.what() << ")" << std::endl;
	}
	 */
	Free();
}

void TImageWrapper::Construct(const v8::CallbackInfo& Arguments)
{
	auto NameHandle = Arguments.mParams[1];
	if ( NameHandle->IsUndefined() )
		mName = "undefined";
	else
		mName = v8::GetString(NameHandle);
	
	//	construct with filename
	if ( Arguments.mParams[0]->IsString() )
	{
		LoadFile(Arguments);
		return;
	}
		
	//	construct with size
	if ( Arguments.mParams[0]->IsArray() )
	{
		Alloc(Arguments);
		return;
	}

	
}

Local<FunctionTemplate> TImageWrapper::CreateTemplate(TV8Container& Container)
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
	
	Container.BindFunction<LoadFile_FunctionName>( InstanceTemplate, TImageWrapper::LoadFile );
	Container.BindFunction<Alloc_FunctionName>( InstanceTemplate, TImageWrapper::Alloc );
	Container.BindFunction<Flip_FunctionName>( InstanceTemplate, TImageWrapper::Flip );
	Container.BindFunction<GetWidth_FunctionName>( InstanceTemplate, TImageWrapper::GetWidth );
	Container.BindFunction<GetHeight_FunctionName>( InstanceTemplate, TImageWrapper::GetHeight );
	Container.BindFunction<GetRgba8_FunctionName>( InstanceTemplate, TImageWrapper::GetRgba8 );
	Container.BindFunction<SetLinearFilter_FunctionName>( InstanceTemplate, TImageWrapper::SetLinearFilter );
	Container.BindFunction<Copy_FunctionName>( InstanceTemplate, TImageWrapper::Copy );
	Container.BindFunction<Resize_FunctionName>( InstanceTemplate, TImageWrapper::Resize );
	Container.BindFunction<Clip_FunctionName>( InstanceTemplate, TImageWrapper::Clip );
	Container.BindFunction<Clear_FunctionName>( InstanceTemplate, TImageWrapper::Clear );
	Container.BindFunction<SetFormat_FunctionName>( InstanceTemplate, TImageWrapper::SetFormat );
	Container.BindFunction<GetFormat_FunctionName>( InstanceTemplate, TImageWrapper::GetFormat );

	return ConstructorFunc;
}


v8::Local<v8::Value> TImageWrapper::Alloc(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );

	BufferArray<int,2> IntArray;
	if ( Arguments[0]->IsArray() )
	{
		v8::EnumArray( Arguments[0], GetArrayBridge(IntArray), "Image( [w,h] )" );
	}
	else if ( Arguments[0]->IsNumber() && Arguments[1]->IsNumber() )
	{
		v8::EnumArray( Arguments[0], GetArrayBridge(IntArray), "Image( w*, h )" );
		v8::EnumArray( Arguments[1], GetArrayBridge(IntArray), "Image( w, h* )" );
	}
	else
		throw Soy::AssertException("Invalid params Alloc(width,height) or Alloc( [width,height] )");

	auto Width = IntArray[0];
	auto Height = IntArray[1];
	auto Format = SoyPixelsFormat::Type::RGBA;
	auto& Heap = Params.mContainer.GetImageHeap();
	auto Pixels = std::make_shared<SoyPixels>( SoyPixelsMeta( Width, Height, Format ), Heap );
	This.SetPixels(Pixels);

	return v8::Undefined(Params.mIsolate);
}


v8::Local<v8::Value> TImageWrapper::Flip(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	
	auto& Pixels = This.GetPixels();
	Pixels.Flip();
	
	return v8::Undefined(Params.mIsolate);
}



v8::Local<v8::Value> TImageWrapper::LoadFile(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );

	//	if first arg is filename...
	
	auto Filename = Params.GetRootDirectory() + v8::GetString(Arguments[0]);
	This.DoLoadFile( Filename );
	return v8::Undefined(Params.mIsolate);
}

void TImageWrapper::DoLoadFile(const std::string& Filename)
{
	//	load file
	Array<char> Bytes;
	Soy::FileToArray( GetArrayBridge(Bytes), Filename );
	TStreamBuffer BytesBuffer;
	BytesBuffer.Push( GetArrayBridge(Bytes) );

	//	alloc pixels
	std::shared_ptr<SoyPixels> NewPixels( new SoyPixels(mContainer.GetImageHeap()) );
	
	if ( Soy::StringEndsWith( Filename, Png::FileExtensions, false ) )
	{
		Png::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		mPixelsVersion = GetLatestVersion()+1;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Jpeg::FileExtensions, false ) )
	{
		Jpeg::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		mPixelsVersion = GetLatestVersion()+1;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Gif::FileExtensions, false ) )
	{
		Gif::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		mPixelsVersion = GetLatestVersion()+1;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Tga::FileExtensions, false ) )
	{
		Tga::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		mPixelsVersion = GetLatestVersion()+1;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Bmp::FileExtensions, false ) )
	{
		Bmp::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		mPixelsVersion = GetLatestVersion()+1;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Psd::FileExtensions, false ) )
	{
		Psd::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		mPixelsVersion = GetLatestVersion()+1;
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


v8::Local<v8::Value> TImageWrapper::Copy(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	auto& That = v8::GetObject<TImageWrapper>( Arguments[0] );
	
	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	std::lock_guard<std::recursive_mutex> ThatLock(That.mPixelsLock);

	auto& ThisPixels = This.GetPixels();
	auto& ThatPixels = That.GetPixels();

	ThisPixels.Copy(ThatPixels);
	
	return v8::Undefined(Params.mIsolate);
}


v8::Local<v8::Value> TImageWrapper::Resize(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	auto NewWidth = v8::SafeCast<Number>( Arguments[0] )->Uint32Value();
	auto NewHeight = v8::SafeCast<Number>( Arguments[1] )->Uint32Value();

	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	
	auto& ThisPixels = This.GetPixels();
	
	ThisPixels.ResizeFastSample( NewWidth, NewHeight );
	
	return v8::Undefined(Params.mIsolate);
}


v8::Local<v8::Value> TImageWrapper::Clear(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	
	This.Free();
	
	return v8::Undefined(Params.mIsolate);
}



v8::Local<v8::Value> TImageWrapper::Clip(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	auto RectHandle = Arguments[0];
	
	Soy::TScopeTimerPrint Timer(__func__,5);

	BufferArray<int,4> RectPx;
	v8::EnumArray( RectHandle, GetArrayBridge(RectPx), __FUNCTION__ );
	
	if ( RectPx.GetSize() != 4 )
	{
		std::stringstream Error;
		Error << "Expected 4 values for cliping rect (got " << RectPx.GetSize() << ")";
		throw Soy::AssertException(Error.str());
	}
	
	if ( RectPx[0] != 0 || RectPx[1] != 0 )
	{
		std::stringstream Error;
		Error << "Current Clip() only works on width & height. xy needs to be zero. Clip(" << RectPx[0] << "," << RectPx[1] << "," << RectPx[2] << "," << RectPx[3] << ")";
		throw Soy::AssertException(Error.str());
	}
	
	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	
	auto& ThisPixels = This.GetPixels();
	
	ThisPixels.ResizeClip( RectPx[2], RectPx[3] );
	
	return v8::Undefined(Params.mIsolate);
}


v8::Local<v8::Value> TImageWrapper::SetFormat(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );

	auto FormatNameHandle = Arguments[0];
	auto FormatName = v8::GetString(FormatNameHandle);
	auto NewFormat = SoyPixelsFormat::ToType(FormatName);
	
	//	gr: currently only handling pixels
	std::lock_guard<std::recursive_mutex> ThisLock(This.mPixelsLock);
	if ( This.mPixelsVersion != This.GetLatestVersion() )
		throw Soy::AssertException("Image.SetFormat only works on pixels at the moment, and that's not the latest version");

	auto& Pixels = This.GetPixels();
	Pixels.SetFormat(NewFormat);
	This.mPixelsVersion++;
	
	return v8::Undefined(Params.mIsolate);
}

v8::Local<v8::Value> TImageWrapper::GetFormat(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );


	auto Meta = This.GetMeta();

	auto Format = Meta.GetFormat();
	auto FormatString = SoyPixelsFormat::ToString(Format);
	auto FormatStr = v8::GetString( Params.GetIsolate(), FormatString );
	return FormatStr;
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

v8::Local<v8::Value> TImageWrapper::GetWidth(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );

	auto Meta = This.GetMeta();
	return Number::New( Params.mIsolate, Meta.GetWidth() );
}


v8::Local<v8::Value> TImageWrapper::GetHeight(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	
	auto Meta = This.GetMeta();
	return Number::New( Params.mIsolate, Meta.GetHeight() );
}


v8::Local<v8::Value> TImageWrapper::GetRgba8(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	auto& mContainer = Params.mContainer;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	
	auto AllowBgraAsRgbaHandle = Arguments[0];
	bool AllowBgraAsRgba = false;
	if ( AllowBgraAsRgbaHandle->IsBoolean() )
		AllowBgraAsRgba = v8::SafeCast<v8::Boolean>(AllowBgraAsRgbaHandle)->BooleanValue();
	auto TargetArrayHandle = Arguments[1];
	
	Soy::TScopeTimerPrint Timer(__func__,5);
	
	//	gr: this func will probably need to return a promise if reading from opengl etc (we want it to be async anyway!)
	auto& CurrentPixels = This.GetPixels();
	
	//	convert pixels if they're in the wrong format
	std::shared_ptr<SoyPixels> ConvertedPixels;
	SoyPixels* pPixels = nullptr;
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
		ConvertedPixels.reset( new SoyPixels(CurrentPixels,mContainer.GetImageHeap()) );
		ConvertedPixels->SetFormat( SoyPixelsFormat::RGBA );
		pPixels = ConvertedPixels.get();
	}	
	auto& Pixels = *pPixels;
	
	auto& PixelsArray = Pixels.GetPixelsArray();
	
	if ( !TargetArrayHandle->IsUndefined() )
	{
		v8::CopyToTypedArray( Params.GetIsolate(), GetArrayBridge(PixelsArray), TargetArrayHandle );
		return TargetArrayHandle;
	}
	else
	{
		auto Rgba8 = v8::GetTypedArray( Params.GetIsolate(), GetArrayBridge(PixelsArray) );
		return Rgba8;
	}
}


v8::Local<v8::Value> TImageWrapper::SetLinearFilter(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	
	if ( Arguments.Length() != 1 )
		throw Soy::AssertException( "SetLinearFilter(true/false) expected 1 argument");
	
	if ( !Arguments[0]->IsBoolean() )
		throw Soy::AssertException( "SetLinearFilter(true/false) expected boolean argument");

	auto ValueBool = Local<v8::Boolean>::Cast( Arguments[0] );
	auto LinearFilter = ValueBool->Value();
	This.DoSetLinearFilter( LinearFilter );

	return v8::Undefined(Params.mIsolate);
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
			Soy::TScopeTimerPrint Timer("TImageWrapper::GetTexture::Alloc/Upload", 5 );

			auto AllocTexture = [&](const SoyPixelsMeta& Meta)
			{
				if ( mOpenglTexture == nullptr )
				{
					mOpenglTexture.reset( new Opengl::TTexture( Meta, GL_TEXTURE_2D ) );
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

SoyPixels& TImageWrapper::GetPixels()
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);

	if ( mPixelsVersion < GetLatestVersion() && mPixelBufferVersion == GetLatestVersion() )
	{
		//	grab pixels from image buffer
		auto CopyPixels = [&](const ArrayBridge<SoyPixelsImpl*>& Pixels,float3x3& Transform)
		{
			mPixels.reset( new SoyPixels(mContainer.GetImageHeap()) );
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
		mPixels.reset( new SoyPixels(mContainer.GetImageHeap()) );
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
	mOpenglTexture->RefreshMeta();
}

void TImageWrapper::SetPixels(const SoyPixelsImpl& NewPixels)
{
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixels.reset( new SoyPixels(NewPixels,mContainer.GetImageHeap()) );
	mPixelsVersion = GetLatestVersion()+1;
}

void TImageWrapper::SetPixels(std::shared_ptr<SoyPixels> NewPixels)
{
	//if ( NewPixels->GetFormat() != SoyPixelsFormat::RGB )
	//	std::Debug << "Setting image to pixels: " << NewPixels->GetMeta() << std::endl;
	
	std::lock_guard<std::recursive_mutex> Lock(mPixelsLock);
	mPixels = NewPixels;
	mPixelsVersion = GetLatestVersion()+1;
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

	//	warning in case we haven't actually updated
	if ( mPixelsVersion >= mOpenglTextureVersion )
		std::Debug << "Warning, overwriting newer/same pixels(v" << mPixelsVersion << ") with gl texture (v" << mOpenglTextureVersion << ")";
	//	if we have no pixels, alloc
	if ( mPixels == nullptr )
		mPixels.reset( new SoyPixels(mContainer.GetImageHeap()) );

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

