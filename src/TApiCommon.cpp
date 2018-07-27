#include "TApiCommon.h"
#include <SoyDebug.h>
#include <SoyImage.h>
#include <SoyFilesystem.h>
#include <SoyStream.h>
#include <SoyOpengl.h>

using namespace v8;

const char Debug_FunctionName[] = "Debug";
const char CompileAndRun_FunctionName[] = "CompileAndRun";
const char LoadFileAsString_FunctionName[] = "LoadFileAsString";
const char LoadFileAsArrayBuffer_FunctionName[] = "LoadFileAsArrayBuffer";
const char WriteStringToFile_FunctionName[] = "WriteStringToFile";

const char LoadFile_FunctionName[] = "Load";
const char Alloc_FunctionName[] = "Create";
const char Flip_FunctionName[] = "Flip";
const char GetWidth_FunctionName[] = "GetWidth";
const char GetHeight_FunctionName[] = "GetHeight";
const char GetRgba8_FunctionName[] = "GetRgba8";
const char SetLinearFilter_FunctionName[] = "SetLinearFilter";

static v8::Local<v8::Value> Debug(v8::CallbackInfo& Params);
static v8::Local<v8::Value> CompileAndRun(v8::CallbackInfo& Params);
static v8::Local<v8::Value> LoadFileAsString(v8::CallbackInfo& Params);
static v8::Local<v8::Value> LoadFileAsArrayBuffer(v8::CallbackInfo& Params);
static v8::Local<v8::Value> WriteStringToFile(v8::CallbackInfo& Params);


void ApiCommon::Bind(TV8Container& Container)
{
	//  load api's before script & executions
	Container.BindGlobalFunction<Debug_FunctionName>(Debug);
	Container.BindGlobalFunction<CompileAndRun_FunctionName>(CompileAndRun);
	Container.BindGlobalFunction<LoadFileAsString_FunctionName>(LoadFileAsString);
	Container.BindGlobalFunction<LoadFileAsArrayBuffer_FunctionName>(LoadFileAsArrayBuffer);
	Container.BindGlobalFunction<WriteStringToFile_FunctionName>(WriteStringToFile);
	Container.BindObjectType( TImageWrapper::GetObjectTypeName(), TImageWrapper::CreateTemplate );
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

static Local<Value> CompileAndRun(CallbackInfo& Params)
{
	auto& args = Params.mParams;
	
	if (args.Length() != 1)
	{
		throw Soy::AssertException("Expected source as first argument");
	}
	
	auto Source = Local<String>::Cast( args[0] );

	Params.mContainer.LoadScript( Params.mContext, Source );
	
	return Undefined(Params.mIsolate);
}



static Local<Value> LoadFileAsString(CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	if (Arguments.Length() < 1)
		throw Soy::AssertException("LoadFileAsString(Filename) with no args");

	auto Filename = v8::GetString( Arguments[0] );
	std::string Contents;
	if ( !Soy::FileToString( Filename, Contents) )
	{
		std::stringstream Error;
		Error << "Failed to read " << Filename;
		throw Soy::AssertException( Error.str() );
	}
	
	auto ContentsString = v8::GetString( Params.GetIsolate(), Contents );
	return ContentsString;
}


static Local<Value> LoadFileAsArrayBuffer(CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	if (Arguments.Length() < 1)
		throw Soy::AssertException("LoadFileAsArrayBuffer(Filename) with no args");

	
	auto Filename = v8::GetString( Arguments[0] );
	Array<char> FileContents;
	Soy::FileToArray( GetArrayBridge(FileContents), Filename );
	
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
	
	
	return ArrayBuffer;
}



static Local<Value> WriteStringToFile(CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	if (Arguments.Length() < 2)
		throw Soy::AssertException("WriteStringToFile(Filename,String) with no args");
	
	auto Filename = v8::GetString( Arguments[0] );
	auto Contents = v8::GetString( Arguments[1] );

	if ( !Soy::StringToFile( Filename, Contents ) )
	{
		std::stringstream Error;
		Error << "Failed to write " << Filename;
		throw Soy::AssertException( Error.str() );
	}

	return v8::Undefined(Params.mIsolate);
}




void TImageWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	
	//	gr: auto catch this
	try
	{
		auto& Container = GetObject<TV8Container>( Arguments.Data() );
		
		//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
		//		but it also needs to know of the V8container to run stuff
		//		cyclic hell!
		auto* NewImage = new TImageWrapper(Container);
		NewImage->mHandle.Reset( Isolate, Arguments.This() );
		This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewImage ) );
		// return the new object back to the javascript caller
		Arguments.GetReturnValue().Set( This );
		
		//	construct with filename
		if ( Arguments[0]->IsString() )
		{
			auto ThisLoadFile = [&](v8::CallbackInfo& Args)
			{
				return NewImage->LoadFile(Args);
			};
			CallFunc( ThisLoadFile, Arguments, Container );
		}
		
		//	construct with size
		if ( Arguments[0]->IsArray() )
		{
			auto ThisAlloc = [&](v8::CallbackInfo& Args)
			{
				return NewImage->Alloc(Args);
			};
			CallFunc( ThisAlloc, Arguments, Container );
		}
		
	}
	catch(std::exception& e)
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, e.what() ));
		Arguments.GetReturnValue().Set(Exception);
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
	This.mPixels.reset( new SoyPixels( SoyPixelsMeta( Width, Height, Format ) ) );

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
	
	std::string Filename( *String::Utf8Value(Arguments[0]) );
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
	std::shared_ptr<SoyPixels> NewPixels( new SoyPixels );
	
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

v8::Local<v8::Value> TImageWrapper::GetWidth(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );

	size_t Width = 0;
	if ( This.mPixels )
		Width = This.mPixels->GetWidth();
	else if ( This.mOpenglTexture )
		Width = This.mOpenglTexture->GetWidth();
	else
		throw Soy::AssertException("Image not allocated");
	
	return Number::New( Params.mIsolate, Width );
}


v8::Local<v8::Value> TImageWrapper::GetHeight(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	
	size_t Height = 0;
	if ( This.mPixels )
		Height = This.mPixels->GetHeight();
	else if ( This.mOpenglTexture )
		Height = This.mOpenglTexture->GetHeight();
	else
		throw Soy::AssertException("Image not allocated");
	
	return Number::New( Params.mIsolate, Height );
}


v8::Local<v8::Value> TImageWrapper::GetRgba8(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );
	
	//	gr: this func will probably need to return a promise if reading from opengl etc (we want it to be async anyway!)
	auto& CurrentPixels = This.GetPixels();
	
	//	convert pixels if they're in the wrong format
	std::shared_ptr<SoyPixels> ConvertedPixels;
	SoyPixels* pPixels = nullptr;
	if ( CurrentPixels.GetFormat() == SoyPixelsFormat::RGBA )
	{
		pPixels = &CurrentPixels;
	}
	else
	{
		ConvertedPixels.reset( new SoyPixels(CurrentPixels) );
		ConvertedPixels->SetFormat( SoyPixelsFormat::RGBA );
		pPixels = ConvertedPixels.get();
	}	
	auto& Pixels = *pPixels;
	
	//	we have some more efficient parallel funcs for image conversion, so throw if not rgba for now
	auto Meta = Pixels.GetMeta();
	
	auto Rgba8Buffer = v8::ArrayBuffer::New( &Params.GetIsolate(), Meta.GetDataSize() );
	auto Rgba8BufferContents = Rgba8Buffer->GetContents();
	auto Rgba8DataArray = GetRemoteArray( static_cast<uint8_t*>( Rgba8BufferContents.Data() ), Rgba8BufferContents.ByteLength() );
	Rgba8DataArray.Copy( Pixels.GetPixelsArray() );

	auto Rgba8 = v8::Uint8ClampedArray::New( Rgba8Buffer, 0, Rgba8Buffer->ByteLength() );
	
	return Rgba8;
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

void TImageWrapper::GetTexture(std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError)
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
	
	if ( !mPixels )
		throw Soy::AssertException("Trying to get opengl texture when we have no pixels");
	
	//	gr: this will need to be on the context's thread
	//		need to fail here if we're not
	try
	{
		mOpenglTexture.reset( new Opengl::TTexture( mPixels->GetMeta(), GL_TEXTURE_2D ) );
		mOpenglTexture->SetFilter( mLinearFilter );
		mOpenglTexture->SetRepeat( mRepeating );

		SoyGraphics::TTextureUploadParams UploadParams;
		mOpenglTexture->Write( *mPixels, UploadParams );
		mOpenglTextureVersion = mPixelsVersion;
		OnTextureLoaded();
	}
	catch(std::exception& e)
	{
		OnError( e.what() );
	}
}

const Opengl::TTexture& TImageWrapper::GetTexture()
{
	if ( !mOpenglTexture )
		throw Soy::AssertException("Image missing opengl texture. Accessing before generating.");
	
	return *mOpenglTexture;
}


SoyPixels& TImageWrapper::GetPixels()
{
	if ( mPixelsVersion < GetLatestVersion() )
	{
		std::stringstream Error;
		Error << "Image pixels(v" << mPixelsVersion <<") are out of date (v" << GetLatestVersion() << ")";
		throw Soy::AssertException(Error.str());
	}
	
	//	is latest and not allocated, this is okay, lets just alloc
	if ( mPixelsVersion == 0 && mPixels == nullptr )
	{
		mPixels.reset( new SoyPixels );
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

	return MaxVersion;
}


void TImageWrapper::OnOpenglTextureChanged()
{
	//	is now latest version
	auto LatestVersion = GetLatestVersion();
	mOpenglTextureVersion = LatestVersion+1;
}

void TImageWrapper::ReadOpenglPixels()
{
	//	gr: this needs to be in the opengl thread!
	//Context.IsInThread
	
	if ( !mOpenglTexture )
		throw Soy::AssertException("Trying to ReadOpenglPixels with no texture");

		//	warning in case we haven't actually updated
	if ( mPixelsVersion >= mOpenglTextureVersion )
		std::Debug << "Warning, overwriting newer/same pixels(v" << mPixelsVersion << ") with gl texture (v" << mOpenglTextureVersion << ")";
	//	if we have no pixels, alloc
	if ( mPixels == nullptr )
		mPixels.reset( new SoyPixels );

	auto Format = SoyPixelsFormat::Invalid;
	auto Flip = false;
	mOpenglTexture->Read( *mPixels, Format, Flip );
	mPixelsVersion = mOpenglTextureVersion;
}

