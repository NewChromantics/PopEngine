#include "TApiCoreMl.h"
#include "TApiCommon.h"

using namespace v8;

const char DetectObjects_FunctionName[] = "DetectObjects";

const char CoreMlMobileNet_TypeName[] = "CoreMlMobileNet";


void ApiCoreMl::Bind(TV8Container& Container)
{
	Container.BindObjectType( TCoreMlMobileNetWrapper::GetObjectTypeName(), TCoreMlMobileNetWrapper::CreateTemplate, TV8ObjectWrapperBase::Allocate<TCoreMlMobileNetWrapper> );
}


void TCoreMlMobileNetWrapper::Construct(const v8::CallbackInfo& Arguments)
{
}

Local<FunctionTemplate> TCoreMlMobileNetWrapper::CreateTemplate(TV8Container& Container)
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
	
	Container.BindFunction<DetectObjects_FunctionName>( InstanceTemplate, DetectObjects );

	return ConstructorFunc;
}



v8::Local<v8::Value> TCoreMlMobileNetWrapper::DetectObjects(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	
	auto ThisHandle = Arguments.This()->GetInternalField(0);
	auto& This = v8::GetObject<CoreMl::TMobileNet>( ThisHandle );
	auto& Image = v8::GetObject<TImageWrapper>( Arguments[0] );

	throw Soy::AssertException("Todo TCoreMlMobileNetWrapper::DetectObjects");
	
	return v8::Undefined(Params.mIsolate);
}
