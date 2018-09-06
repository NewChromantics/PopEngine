#include "TApiFile.h"
#include <SoyFilesystem.h>

using namespace v8;


const char File_TypeName[] = "File";

const char GetString_FunctionName[] = "GetString";
const char GetBytes_FunctionName[] = "GetBytes";



void ApiFile::Bind(TV8Container& Container)
{
	Container.BindObjectType<TFileWrapper>();
}




Local<FunctionTemplate> TFileWrapper::CreateTemplate(TV8Container& Container)
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
	
	Container.BindFunction<GetString_FunctionName>( InstanceTemplate, GetString );
	Container.BindFunction<GetBytes_FunctionName>( InstanceTemplate, GetBytes );
	
	return ConstructorFunc;
}


void TFileWrapper::Construct(const v8::CallbackInfo& Arguments)
{
	auto FilenameHandle = Arguments.mParams[0];
	auto Filename = v8::GetString( FilenameHandle );

	auto& This = Arguments.GetThis<TFileWrapper>();
	
	auto OnFileChanged = [this]
	{
		
	};
	This.mFileHandle.reset( new TFileHandle(Filename), OnFileChanged );
}


v8::Local<v8::Value> TFileWrapper::GetString(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<TFileWrapper>( ThisHandle );
	auto& FileHandle = This.GetFileHandle();
	
	auto Contents = FileHandle.GetContentsString();
	auto ContentsHandle = v8::GetString( Arguments.GetIsolate(), Contents );
	
	return ContentsHandle;
}


void TFileWrapper::OnFileChanged()
{
	auto Runner = [this](Local<Context> context)
	{
		auto This = this->mHandle->GetLocal(*isolate);
		
		BufferArray<Local<Value>,2> Args;
		Args.PushBack( This );
		
		auto FuncHandle = v8::GetFunction( context, This, "OnChanged" );
		
		//std::Debug << "Starting Media::OnNewFrame Javascript" << std::endl;
		//	this func should be fast really and setup promises to defer processing
		Soy::TScopeTimerPrint Timer("Media::OnNewFrame Javascript callback",5);
		mContainer->ExecuteFunc( context, FuncHandle, This, GetArrayBridge(Args) );
		Timer.Stop();
	};
	mContainer->QueueScoped( Runner );
}

