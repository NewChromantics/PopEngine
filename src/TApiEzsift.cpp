#include "TApiEzsift.h"
#include "TApiCommon.h"

#include "Ezsift/include/ezsift.h"

using namespace v8;

const char GetFeatures_FunctionName[] = "GetFeatures";

const char Ezsift_TypeName[] = "Ezsift";


void ApiEzsift::Bind(TV8Container& Container)
{
	Container.BindObjectType( TEzsiftWrapper::GetObjectTypeName(), TEzsiftWrapper::CreateTemplate, TV8ObjectWrapperBase::Allocate<TEzsiftWrapper> );
}

class Ezsift::TInstance
{
};

void TEzsiftWrapper::Construct(const v8::CallbackInfo& Arguments)
{
	mEzsift.reset( new Ezsift::TInstance );
}

Local<FunctionTemplate> TEzsiftWrapper::CreateTemplate(TV8Container& Container)
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
	
	Container.BindFunction<GetFeatures_FunctionName>( InstanceTemplate, GetFeatures );
	
	return ConstructorFunc;
}


v8::Local<v8::Value> TEzsiftWrapper::GetFeatures(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	/*
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	
	auto& This = v8::GetObject<TEzsiftWrapper>( ThisHandle );
	auto& CoreMl = This.mCoreMl;
	
	auto CoreMlFunc = std::mem_fn( &CoreMl::TInstance::RunOpenPose );
	return RunModel( CoreMlFunc, Params, CoreMl );
	*/
	
	BufferArray<vec2f,4> Features;
	Features.PushBack( vec2f( 0.1f, 0.1f ) );
	Features.PushBack( vec2f( 0.5f, 0.1f ) );
	Features.PushBack( vec2f( 0.5f, 0.5f ) );
	Features.PushBack( vec2f( 0.1f, 0.5f ) );
	
	//	return array for testing
	auto GetElement = [&](size_t Index)
	{
		auto ObjectJs = v8::Object::New( Params.mIsolate );
		auto x = Features[Index].x;
		auto y = Features[Index].y;
		ObjectJs->Set( v8::String::NewFromUtf8( Params.mIsolate, "x"), v8::Number::New(Params.mIsolate, x) );
		ObjectJs->Set( v8::String::NewFromUtf8( Params.mIsolate, "y"), v8::Number::New(Params.mIsolate, y) );
		return ObjectJs;
	};
	auto ObjectsArray = v8::GetArray( *Params.mIsolate, Features.GetSize(), GetElement);
	return ObjectsArray;
}

