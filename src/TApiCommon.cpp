#include "TApiCommon.h"
#include <SoyDebug.h>
#include <SoyImage.h>
#include <SoyFilesystem.h>
#include <SoyStream.h>

using namespace v8;

const char Log_FunctionName[] = "log";
const char LoadFile_FunctionName[] = "Load";

static v8::Local<v8::Value> OnLog(v8::CallbackInfo& Params);


void ApiCommon::Bind(TV8Container& Container)
{
	//  load api's before script & executions
	Container.BindGlobalFunction<Log_FunctionName>(OnLog);
	Container.BindObjectType("Image", TImageWrapper::CreateTemplate );
}

static Local<Value> OnLog(CallbackInfo& Params)
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
		auto* NewImage = new TImageWrapper();
		NewImage->mHandle.Reset( Isolate, Arguments.This() );
		NewImage->mContainer = &Container;
		This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewImage ) );
		
		auto FilenameHandle = Arguments[0];
		if ( !FilenameHandle->IsUndefined() )
		{
			String::Utf8Value FilenameStr( Arguments[0] );
			std::string Filename( *FilenameStr );
			NewImage->DoLoadFile(Filename);
		}
		
		// return the new object back to the javascript caller
		Arguments.GetReturnValue().Set( This );
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

	return ConstructorFunc;
}


v8::Local<v8::Value> TImageWrapper::LoadFile(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TImageWrapper>( ThisHandle );

	std::string Filename( *String::Utf8Value(Arguments[0]) );
	//This.LoadFile( Filename );
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
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Jpeg::FileExtensions, false ) )
	{
		Png::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Gif::FileExtensions, false ) )
	{
		Png::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Tga::FileExtensions, false ) )
	{
		Png::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Bmp::FileExtensions, false ) )
	{
		Png::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		return;
	}
	
	if ( Soy::StringEndsWith( Filename, Psd::FileExtensions, false ) )
	{
		Png::Read( *NewPixels, BytesBuffer );
		mPixels = NewPixels;
		return;
	}


	
	throw Soy::AssertException( std::string("Unhandled image file extension of ") + Filename );
}


