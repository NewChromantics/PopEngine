#include "TApiEzsift.h"
#include "TApiCommon.h"

#include "Ezsift/include/ezsift.h"


const char GetFeatures_FunctionName[] = "GetFeatures";

const char Ezsift_TypeName[] = "Ezsift";


void ApiEzsift::Bind(Bind::TContext& Context)
{
	Context.BindObjectType<TEzsiftWrapper>("Pop");
}

class Ezsift::TInstance
{
};

void TEzsiftWrapper::Construct(Bind::TCallback& Arguments)
{
	mEzsift.reset( new Ezsift::TInstance );
}

void TEzsiftWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<GetFeatures_FunctionName>(GetFeatures);
}


void TEzsiftWrapper::GetFeatures(Bind::TCallback& Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	
	SoyPixels Pixels;
	Image.GetPixels(Pixels);
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
	Array<Bind::TObject> FeatureObjects;
	for ( auto i=0;	i<Features.GetSize();	i++ )
	{
		auto ObjectJs = Params.mContext.CreateObjectInstance( Params.mLocalContext );
		auto x = Features[i].x;
		auto y = Features[i].y;
		ObjectJs.SetFloat("x",x);
		ObjectJs.SetFloat("y",y);
		FeatureObjects.PushBack( ObjectJs );
	};
	Params.Return( GetArrayBridge(FeatureObjects) );
}

