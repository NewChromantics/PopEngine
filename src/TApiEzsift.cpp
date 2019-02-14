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
	auto* pImage = &v8::GetObject<TImageWrapper>( Arguments[0] );
	
	SoyPixels Pixels;
	pImage->GetPixels(Pixels);
	Pixels.SetFormat( SoyPixelsFormat::Greyscale );
	Pixels.ResizeFastSample( Pixels.GetWidth()/2, Pixels.GetHeight()/2 );
	
	Array<vec2f> Features;

	
	auto w = static_cast<int>( Pixels.GetWidth() );
	auto h = static_cast<int>( Pixels.GetHeight() );
	ezsift::Image<unsigned char> image( w,h );
	SoyPixelsRemote ImagePixels( image.data, w, h, w*h*1, SoyPixelsFormat::Greyscale );
	ImagePixels.Copy( Pixels );
	
	// Double the original image as the first octive.
	ezsift::double_original_image(true);
	std::list<ezsift::SiftKeypoint> kpt_list;
	ezsift::sift_cpu(image, kpt_list, true);
	
	//std::list<ezsift::SiftKeypoint>::iterator it;
	for ( auto it = kpt_list.begin(); it != kpt_list.end(); it++)
	{
		auto Octave = it->octave;
		auto Layer = it->layer;
		
		//	row & col are normalised
		//	... in docs, but not here
		auto y = it->r / static_cast<float>(h);
		auto x = it->c / static_cast<float>(w);
		auto Scale = it->scale;
		auto Orig = it->ori;

		Features.PushBack( vec2f(x,y) );
		/*
		if (bIncludeDescpritor) {
			for (int i = 0; i < 128; i++) {
				fprintf(fp, "%d\t", (int)(it->descriptors[i]));
			}
		}
		*/
	}
	std::Debug << "Found " << Features.GetSize() << " features" << std::endl;
	
	
	static bool UseTestFeatures = false;
	if ( UseTestFeatures )
	{
		Features.PushBack( vec2f( 0.1f, 0.1f ) );
		Features.PushBack( vec2f( 0.5f, 0.1f ) );
		Features.PushBack( vec2f( 0.5f, 0.5f ) );
		Features.PushBack( vec2f( 0.1f, 0.5f ) );
	}
	
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

