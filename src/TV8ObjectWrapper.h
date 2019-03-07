#pragma once
#include "TBind.h"

#error Dont include me

template<const char* TYPENAME,class TYPE>
class TObjectWrapper : public TV8ObjectWrapperBase
{
public:
	typedef std::function<TObjectWrapper<TYPENAME,TYPE>*(TV8Container&,v8::Local<v8::Object>)> ALLOCATORFUNC;
	
public:
	TObjectWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>());

	static std::string						GetObjectTypeName()	{	return TYPENAME;	}
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	v8::Local<v8::Object>					GetHandle() const;
	
protected:
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static void								OnFree(const v8::WeakCallbackInfo<TObjectWrapper>& data);
	
protected:
	v8::Persist<v8::Object>				mHandle;
	TV8Container&						mContainer;
	std::shared_ptr<TYPE>				mObject;
	
protected:
};


//	we can create this outside of a context, but maybe needs to be in isolate scope?
template<const char* TYPENAME,class TYPE>
TObjectWrapper<TYPENAME,TYPE>::TObjectWrapper(TV8Container& Container,v8::Local<v8::Object> This) :
	mContainer	( Container )
{
	if ( This.IsEmpty() )
	{
		This = Container.CreateObjectInstancev8( TYPENAME );
	}
	
	auto* Isolate = &Container.GetIsolate();
	This->SetInternalField( 0, v8::External::New( Isolate, this ) );

	mHandle.Reset( Isolate, This );

	//	make it weak
	//	https://itnext.io/v8-wrapped-objects-lifecycle-42272de712e0
	mHandle.SetWeak( this, OnFree, v8::WeakCallbackType::kInternalFields );
}



template<const char* TYPENAME,class TYPE>
inline void TObjectWrapper<TYPENAME,TYPE>::OnFree(const v8::WeakCallbackInfo<TObjectWrapper>& WeakCallbackData)
{
	//std::Debug << "Freeing " << TYPENAME << "..." << std::endl;
	auto* ObjectWrapper = WeakCallbackData.GetParameter();
	delete ObjectWrapper;
}



template<const char* TYPENAME,class TYPE>
inline v8::Local<v8::Object> TObjectWrapper<TYPENAME,TYPE>::GetHandle() const
{
	using namespace v8;
	auto* Isolate = &this->mContainer.GetIsolate();
	auto LocalHandle = Local<Object>::New( Isolate, mHandle );
	return LocalHandle;
}



template<const char* TYPENAME,class TYPE>
void TObjectWrapper<TYPENAME,TYPE>::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	using namespace v8;
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );

	//	gr: auto catch this
	try
	{
		auto& Container = GetObject<TV8Container>( Arguments.Data() );
		
		auto Allocator = Container.GetAllocator(TYPENAME);
		auto* NewObject = Allocator( Container, This );

		v8::TCallback ArgumentsWrapper( Arguments, Container );

		NewObject->Construct( ArgumentsWrapper );
	}
	catch(std::exception& e)
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, e.what() ));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
}

template<const char* TYPENAME,class TYPE>
v8::Local<v8::FunctionTemplate> TObjectWrapper<TYPENAME,TYPE>::CreateTemplate(TV8Container& Container)
{
	using namespace v8;
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container ).As<Value>();
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
	/*
	Container.BindFunction<LoadFile_FunctionName>( InstanceTemplate, TImageWrapper::LoadFile );
	Container.BindFunction<Alloc_FunctionName>( InstanceTemplate, TImageWrapper::Alloc );
	Container.BindFunction<Flip_FunctionName>( InstanceTemplate, TImageWrapper::Flip );
	Container.BindFunction<GetWidth_FunctionName>( InstanceTemplate, TImageWrapper::GetWidth );
	Container.BindFunction<GetHeight_FunctionName>( InstanceTemplate, TImageWrapper::GetHeight );
	Container.BindFunction<GetRgba8_FunctionName>( InstanceTemplate, TImageWrapper::GetRgba8 );
	Container.BindFunction<SetLinearFilter_FunctionName>( InstanceTemplate, TImageWrapper::SetLinearFilter );
	*/
	return ConstructorFunc;
}
