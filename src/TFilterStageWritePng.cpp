#include "TFilterStageWritePng.h"
#include "SoyPng.h"


void TFilterStage_WritePng::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	get source data
	auto& ImageData = Frame.GetData<TFilterStageRuntimeData>( mImageStage );
	
	//	extract the texture data
	auto& OpenglContext = mFilter.GetOpenglContext();
	auto ImagePixels = ImageData.GetPixels( OpenglContext );

	if ( !ImagePixels )
	{
		std::stringstream Error;
		Error << "Could not get pixels from " << mImageStage << " to export to " << mFilename;
		throw Soy::AssertException( Error.str() );
	}

	Array<char> PngData;
	ImagePixels->GetPng( GetArrayBridge(PngData) );
	Soy::ArrayToFile( GetArrayBridge(PngData), mFilename );
}

