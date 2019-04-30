#include "TApiCoreMl.h"
#include "TApiCommon.h"
#include "SoyAvf.h"
#include "SoyLib/src/SoyScope.h"

#if defined(ENABLE_COREML_MODELS)

#import "TApiCoreMlModels.mm"

#else

class CoreMl::TInstance
{
};

#endif


#import <Vision/Vision.h>

const char Yolo_FunctionName[] = "Yolo";
const char Hourglass_FunctionName[] = "Hourglass";
const char Cpm_FunctionName[] = "Cpm";
const char OpenPose_FunctionName[] = "OpenPose";
const char SsdMobileNet_FunctionName[] = "SsdMobileNet";
const char MaskRcnn_FunctionName[] = "MaskRcnn";
const char FaceDetect_FunctionName[] = "FaceDetect";

const char CoreMl_TypeName[] = "CoreMl";


void ApiCoreMl::Bind(Bind::TContext& Context)
{
	Context.BindObjectType<TCoreMlWrapper>( ApiPop::Namespace );
}


namespace CoreMl
{
	class TObject;
}

class CoreMl::TObject
{
public:
	float			mScore = 0;
	std::string		mLabel;
	Soy::Rectf		mRect = Soy::Rectf(0,0,0,0);
	vec2x<size_t>	mGridPos;
};



void TCoreMlWrapper::Construct(Bind::TCallback& Arguments)
{
#if defined(ENABLE_COREML_MODELS)
	mCoreMl.reset( new CoreMl::TInstance );
#endif
}

void TCoreMlWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<Yolo_FunctionName>( Yolo );
	Template.BindFunction<Hourglass_FunctionName>( Hourglass );
	Template.BindFunction<Cpm_FunctionName>( Cpm );
	Template.BindFunction<OpenPose_FunctionName>( OpenPose );
	Template.BindFunction<SsdMobileNet_FunctionName>( SsdMobileNet );
	Template.BindFunction<MaskRcnn_FunctionName>( MaskRcnn );
	Template.BindFunction<FaceDetect_FunctionName>( FaceDetect );
}


template<typename COREML_FUNC>
void RunModel(COREML_FUNC CoreMlFunc,Bind::TCallback& Params,std::shared_ptr<CoreMl::TInstance> CoreMl)
{
	auto* pImage = &Params.GetArgumentPointer<TImageWrapper>(0);
	auto Promise = Params.mContext.CreatePromise(__FUNCTION__);
	auto* pContext = &Params.mContext;
	
	auto RunModel = [=]
	{
		try
		{
			//	do all the work on the thread
			auto& Image = *pImage;
			auto& CurrentPixels = Image.GetPixels();
			SoyPixels TempPixels;
			SoyPixelsImpl* pPixels = &TempPixels;
			if ( CurrentPixels.GetFormat() == SoyPixelsFormat::RGBA )
			{
				pPixels = &CurrentPixels;
			}
			else
			{
				pImage->GetPixels(TempPixels);
				TempPixels.SetFormat( SoyPixelsFormat::RGBA );
				pPixels = &TempPixels;
			}
			auto& Pixels = *pPixels;
			
			Array<CoreMl::TObject> Objects;
			
			auto PushObject = [&](const CoreMl::TObject& Object)
			{
				Objects.PushBack(Object);
			};
			CoreMlFunc( *CoreMl, Pixels, PushObject );
			
			auto OnCompleted = [=](Bind::TLocalContext& Context)
			{
				Array<Bind::TObject> Elements;
				for ( auto i=0;	i<Objects.GetSize();	i++)
				{
					auto& Object = Objects[i];
					auto ObjectJs = Context.mGlobalContext.CreateObjectInstance(Context);
					ObjectJs.SetString("Label", Object.mLabel );
					ObjectJs.SetFloat("Score", Object.mScore );
					ObjectJs.SetFloat("x", Object.mRect.x );
					ObjectJs.SetFloat("y", Object.mRect.y );
					ObjectJs.SetFloat("w", Object.mRect.w );
					ObjectJs.SetFloat("h", Object.mRect.h );
					ObjectJs.SetInt("GridX", Object.mGridPos.x );
					ObjectJs.SetInt("GridY", Object.mGridPos.y );
					Elements.PushBack( ObjectJs );
				};
				Promise.Resolve( GetArrayBridge(Elements) );
			};
			
			pContext->Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			Promise.Reject( ExceptionString );
		}
	};
	
#if defined(ENABLE_COREML_MODELS)
	CoreMl->PushJob(RunModel);
#endif
	Params.Return( Promise );
}


void TCoreMlWrapper::Yolo(Bind::TCallback& Params)
{
#if defined(ENABLE_COREML_MODELS)
	auto& This = Params.This<TCoreMlWrapper>();
	auto& CoreMl = This.mCoreMl;

	auto CoreMlFunc = std::mem_fn( &CoreMl::TInstance::RunYolo );
	RunModel( CoreMlFunc, Params, CoreMl );
#endif
	throw Soy::AssertException("CoreML Models not built");
}


void TCoreMlWrapper::Hourglass(Bind::TCallback& Params)
{
#if defined(ENABLE_COREML_MODELS)
	auto& This = Params.This<TCoreMlWrapper>();
	auto& CoreMl = This.mCoreMl;

	auto CoreMlFunc = std::mem_fn( &CoreMl::TInstance::RunHourglass );
	return RunModel( CoreMlFunc, Params, CoreMl );
#endif
	throw Soy::AssertException("CoreML Models not built");
}



void TCoreMlWrapper::Cpm(Bind::TCallback& Params)
{
#if defined(ENABLE_COREML_MODELS)
	auto& This = Params.This<TCoreMlWrapper>();
	auto& CoreMl = This.mCoreMl;

	auto CoreMlFunc = std::mem_fn( &CoreMl::TInstance::RunCpm );
	return RunModel( CoreMlFunc, Params, CoreMl );
#endif
	throw Soy::AssertException("CoreML Models not built");
}



void TCoreMlWrapper::OpenPose(Bind::TCallback& Params)
{
#if defined(ENABLE_COREML_MODELS)
	auto& This = Params.This<TCoreMlWrapper>();
	auto& CoreMl = This.mCoreMl;

	auto CoreMlFunc = std::mem_fn( &CoreMl::TInstance::RunOpenPose );
	return RunModel( CoreMlFunc, Params, CoreMl );
#endif
	throw Soy::AssertException("CoreML Models not built");
	
}



void TCoreMlWrapper::SsdMobileNet(Bind::TCallback& Params)
{
#if defined(ENABLE_COREML_MODELS)
	auto& This = Params.This<TCoreMlWrapper>();
	auto& CoreMl = This.mCoreMl;

	auto CoreMlFunc = std::mem_fn( &CoreMl::TInstance::RunSsdMobileNet );
	return RunModel( CoreMlFunc, Params, CoreMl );
#endif
	throw Soy::AssertException("CoreML Models not built");
}


void TCoreMlWrapper::MaskRcnn(Bind::TCallback& Params)
{
#if defined(ENABLE_COREML_MODELS)
	auto& This = Params.This<TCoreMlWrapper>();
	auto& CoreMl = This.mCoreMl;

	auto CoreMlFunc = std::mem_fn( &CoreMl::TInstance::RunMaskRcnn );
	return RunModel( CoreMlFunc, Params, CoreMl );
#endif
	throw Soy::AssertException("CoreML Models not built");
}


//	apple's Vision built-in face detection
void TCoreMlWrapper::FaceDetect(Bind::TCallback& Params)
{
	Soy::TScopeTimerPrint FaceDetectTimer("TCoreMlWrapper::FaceDetect", 10);
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	std::shared_ptr<SoyPixels> Pixels( new SoyPixels() );
	Image.GetPixels( *Pixels );
	Pixels->SetFormat( SoyPixelsFormat::Greyscale );
	Pixels->SetFormat( SoyPixelsFormat::RGBA );
	auto Promise = Params.mContext.CreatePromise(__FUNCTION__);

	auto Run = [=](Bind::TLocalContext& Context)
	{
		//	make a face request
		VNDetectFaceLandmarksRequest* Request = [[VNDetectFaceLandmarksRequest alloc] init];
		VNSequenceRequestHandler* Handler = [[VNSequenceRequestHandler alloc] init];
		NSArray<VNDetectFaceLandmarksRequest*>* Requests = @[Request];

		auto PixelBuffer = Avf::PixelsToPixelBuffer(*Pixels);
		CVImageBufferRef ImageBuffer = PixelBuffer;

		auto Orientation = kCGImagePropertyOrientationUp;
		NSError* Error = nullptr;
		{
			Soy::TScopeTimerPrint Timer("Perform Requests",5);
			[Handler performRequests:Requests onCVPixelBuffer:ImageBuffer orientation:Orientation error:&Error];
		}
		NSArray<VNFaceObservation*>* Results = Request.results;
		
		if ( !Results )
			throw Soy::AssertException("Missing results");

		Array<Bind::TObject> ResultObjects;
		for ( auto r=0;	r<Results.count;	r++ )
		{
			for ( VNFaceObservation* Observation in Results )
			{
				std::Debug << "Got observation" << std::endl;
				//	features are normalised to bounds
				Array<vec2f> Features;
				Array<float> FeatureFloats;
				Soy::Rectf Bounds;
				
				auto FlipNormalisedY = [](float y,float h)
				{
					auto Bottom = y + h;
					return 1 - Bottom;
				};
				
				VNFaceLandmarkRegion2D* Landmarks = Observation.landmarks.allPoints;
				Bounds.x = Observation.boundingBox.origin.x;
				Bounds.y = Observation.boundingBox.origin.y;
				Bounds.w = Observation.boundingBox.size.width;
				Bounds.h = Observation.boundingBox.size.height;

				for( auto l=0;	l<Landmarks.pointCount;	l++ )
				{
					auto Point = Landmarks.normalizedPoints[l];
					//	gr: this may be upside down as bounds are
					Features.PushBack( vec2f(Point.x,Point.y) );
					FeatureFloats.PushBack( Point.x );
					FeatureFloats.PushBack( Point.y );
				}
			
				auto w = Pixels->GetWidth();
				auto h = Pixels->GetHeight();
				BufferArray<float,4> RectValues;
				RectValues.PushBack( Bounds.x );
				RectValues.PushBack( FlipNormalisedY(Bounds.y,Bounds.h) );
				RectValues.PushBack( Bounds.w );
				RectValues.PushBack( Bounds.h );
				
				auto Object = Context.mGlobalContext.CreateObjectInstance( Context );
				Object.SetArray("Bounds", GetArrayBridge(RectValues) );
				Object.SetArray("Features", GetArrayBridge(FeatureFloats) );
				ResultObjects.PushBack(Object);
			}
		}
		
		Promise.Resolve( GetArrayBridge(ResultObjects) );
	};
	
	Run( Params.mLocalContext );
	
	Params.Return( Promise );
}


