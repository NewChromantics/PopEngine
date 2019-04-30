#include "TApiDlib.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

//	if dgesvd is missing, link with the accelerate.framework
#include <dlib/image_processing/frontal_face_detector.h>
//#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
//#include <dlib/gui_widgets.h>
//#include <dlib/image_io.h>


#define TIMER_WARNING_MIN_MS	50


const char FindFaces_FunctionName[] = "FindFaces";
const char FindFaceFeatures_FunctionName[] = "FindFaceFeatures";

const char DlibWrapper_TypeName[] = "Dlib";

void ApiDlib::Bind(Bind::TContext& Context)
{
	Context.BindObjectType<TDlibWrapper>( ApiPop::Namespace );
}


TDlibThreads::TDlibThreads(size_t ThreadCount)
{
	if ( ThreadCount < 1 )
		ThreadCount = 1;
		
	for ( int i=0;	i<ThreadCount;	i++ )
	{
		std::stringstream Name;
		Name << "Dlib Job Queue " << i;
		std::shared_ptr<TDlib> Queue( new TDlib(Name.str() ) );
		this->mThreads.PushBack(Queue);
		Queue->Start();
	}
}

TDlibThreads::~TDlibThreads()
{
	//	todo: cleanup threads
}


TDlib& TDlibThreads::GetJobQueue()
{
	//	get queue with least jobs
	auto LeastJobQueue = 0;
	for ( int i=0;	i<mThreads.GetSize();	i++ )
	{
		auto& Queue = *mThreads[i];
		auto& BestQueue = *mThreads[LeastJobQueue];
		if ( Queue.GetJobCount() < BestQueue.GetJobCount() )
			LeastJobQueue = i;
	}
	
	return *mThreads[LeastJobQueue];
}

void TDlibWrapper::Construct(Bind::TCallback& Params)
{
	auto& This = Params.This<TDlibWrapper>();
	
	Array<uint8_t> LandmarksData;
	Params.GetArgumentArray( 0, GetArrayBridge(LandmarksData) );

	size_t ThreadCount = 1;
	if ( !Params.IsArgumentUndefined(1) )
		ThreadCount = Params.GetArgumentInt(1);
	
	This.mDlib.reset( new TDlibThreads(ThreadCount) );
	This.mDlib->SetShapePredictorFaceLandmarks( GetArrayBridge(LandmarksData) );
}


void TDlibWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<FindFaces_FunctionName>( FindFaces );
	Template.BindFunction<FindFaceFeatures_FunctionName>( FindFaceFeatures );
}


//	this loads the shape predictors etc and copies to each thread
void TDlibThreads::SetShapePredictorFaceLandmarks(ArrayBridge<uint8_t>&& LandmarksDatBytes)
{
	//	setup first one, then copy to others
	auto& Dlib0 = *mThreads[0];
	Dlib0.SetShapePredictorFaceLandmarks(LandmarksDatBytes);
	
	
	for ( int i=1;	i<mThreads.GetSize();	i++ )
	{
		auto& DlibN = *mThreads[i];
		DlibN.SetShapePredictorFaceLandmarks(Dlib0);
	}
}


void TDlibWrapper::FindFaces(Bind::TCallback& Params)
{
	auto& This = Params.This<TDlibWrapper>();
	
	//	make a promise resolver (persistent to copy to thread)
	auto Promise = Params.mContext.CreatePromise(__FUNCTION__);

	auto ImageObject = Params.GetArgumentObject(0);
	auto ImagePersistent = Params.mContext.CreatePersistent( ImageObject );
	auto* pImage = &ImageObject.This<TImageWrapper>();
	auto* pContext = &Params.mContext;
	
	auto& Dlib = This.GetDlibJobQueue();
	auto RunFaceDetector = [=,&Dlib]
	{
		try
		{
			
			auto PixelsMeta = pImage->GetPixels().GetMeta();
			auto CopyPixels = [pImage](SoyPixelsImpl& Pixels)
			{
				pImage->GetPixels(Pixels);
			};
			BufferArray<TFace,100> Faces;
			Dlib.GetFaceLandmarks( PixelsMeta, CopyPixels, GetArrayBridge(Faces) );
		
			//	temp
			BufferArray<float,1000> Features;
			for ( int f=0;	f<Faces.GetSize();	f++ )
			{
				auto& FaceRect = Faces[f].mRect;
				Features.PushBack( FaceRect.Left() );
				Features.PushBack( FaceRect.Top() );
				Features.PushBack( FaceRect.GetWidth() );
				Features.PushBack( FaceRect.GetHeight() );
				
				auto& FaceFeatures = Faces[f].mFeatures;
				for ( int i=0;	i<FaceFeatures.GetSize();	i++ )
				{
					Features.PushBack( FaceFeatures[i].x );
					Features.PushBack( FaceFeatures[i].y );
				}
			}
			
			auto OnCompleted = [=](Bind::TLocalContext& Context)
			{
				Promise.Resolve( GetArrayBridge(Features) );
			};

			//	queue the completion, doesn't need to be done instantly
			pContext->Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			Promise.Reject( ExceptionString );
		}
	};
	Dlib.PushJob( RunFaceDetector );

	//	return the promise
	Params.Return( Promise );
}


void TDlibWrapper::FindFaceFeatures(Bind::TCallback& Params)
{
	auto& This = Params.This<TDlibWrapper>();

	auto* pThis = &This;
	
	auto Promise = Params.mContext.CreatePromise(__FUNCTION__);
	
	auto ImageObject = Params.GetArgumentObject(0);
	auto ImagePersistent = Params.mContext.CreatePersistent( ImageObject );
	auto* pImage = &ImageObject.This<TImageWrapper>();
	auto* pContext = &Params.mContext;

	BufferArray<float,4> RectFloats;
	Params.GetArgumentArray(0, GetArrayBridge(RectFloats) );
	Soy::Rectf TargetRect( RectFloats[0], RectFloats[1], RectFloats[2], RectFloats[3] );

	auto& Dlib = This.GetDlibJobQueue();
	auto RunFaceDetector = [=,&Dlib]
	{
		try
		{
			//	gr: copy pixels
			auto PixelsMeta = pImage->GetPixels().GetMeta();
			auto CopyPixels = [pImage](SoyPixelsImpl& Pixels)
			{
				pImage->GetPixels(Pixels);
			};
			BufferArray<TFace,100> Faces;
			auto Face = Dlib.GetFaceLandmarks( PixelsMeta, CopyPixels, TargetRect );

			//	temp
			BufferArray<float,1000> Features;
			{
				auto& FaceRect = Face.mRect;
				Features.PushBack( FaceRect.Left() );
				Features.PushBack( FaceRect.Top() );
				Features.PushBack( FaceRect.GetWidth() );
				Features.PushBack( FaceRect.GetHeight() );
				
				auto& FaceFeatures = Face.mFeatures;
				for ( int i=0;	i<FaceFeatures.GetSize();	i++ )
				{
					Features.PushBack( FaceFeatures[i].x );
					Features.PushBack( FaceFeatures[i].y );
				}
			}
			
			auto OnCompleted = [=](Bind::TLocalContext& Context)
			{
				Promise.Resolve( GetArrayBridge(Features) );
			};
			
			//	queue the completion, doesn't need to be done instantly
			pContext->Queue( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			Promise.Reject( ExceptionString );
		}
	};
	Dlib.PushJob( RunFaceDetector );
	
	Params.Return( Promise );
}


#include <streambuf>
#include <istream>

class membuf: public std::streambuf
{
public:
	membuf(const char* base, size_t size)
	{
		char* p( const_cast<char*>( base ) );
		this->setg(p, p, p + size);
	}
};

class imemstream: public membuf, public std::istream
{
public:
	imemstream(const uint8_t* base, size_t size) :
		membuf( reinterpret_cast<const char*>(base), size),
		std::istream(static_cast<std::streambuf*>(this) )
	{
	}
};


template<SoyPixelsFormat::Type OutputFormat,typename PIXELTYPE>
void GetImageFromPixels(const SoyPixelsMeta& PixelsMeta,dlib::array2d<PIXELTYPE>& DlibImage,std::function<void(SoyPixelsImpl&)>& CopyPixels)
{
	using namespace dlib;
	
	//	alloc image
	DlibImage.set_size( PixelsMeta.GetHeight(), PixelsMeta.GetWidth() );
	auto it = DlibImage.begin();
	//auto* ImgPixelsByte = &DlibImage.begin()->red;
	auto* ImgPixelsByte = reinterpret_cast<uint8_t*>(&(*it));
	auto DlibImageDataSize = DlibImage.width_step() * DlibImage.nr();
	SoyPixelsRemote DlibPixels( ImgPixelsByte, DlibImage.nc(), DlibImage.nr(), DlibImageDataSize, OutputFormat );
	
	if ( PixelsMeta.GetFormat() == DlibPixels.GetFormat() )
	{
		CopyPixels( DlibPixels );
	}
	else
	{
		std::stringstream TimerName;
		TimerName << "dlib converting " << PixelsMeta.GetFormat() << " pixels to " << DlibPixels.GetFormat();
		Soy::TScopeTimerPrint Timer_Convert(TimerName.str().c_str(),TIMER_WARNING_MIN_MS);
		
		SoyPixels NonRgbPixels;
		CopyPixels( NonRgbPixels );
		NonRgbPixels.SetFormat(DlibPixels.GetFormat());
		DlibPixels.Copy( NonRgbPixels );
	}
}


void TDlib::GetFaceLandmarks(const SoyPixelsMeta& PixelsMeta,std::function<void(SoyPixelsImpl&)> CopyPixels,ArrayBridge<TFace>&& Faces)
{
	Soy::TScopeTimerPrint Timer_FindFace("TDlib::GetFaceLandmarks",TIMER_WARNING_MIN_MS);

	using namespace dlib;
	
	// We need a face detector.  We will use this to get bounding boxes for
	// each face in an image.
	auto& detector = *mFaceDetector;

	
	dlib::array2d<uint8_t> img;
	GetImageFromPixels<SoyPixelsFormat::Greyscale>( PixelsMeta, img, CopyPixels );
	//dlib::array2d<dlib::rgb_pixel> img;
	//GetImageFromPixels<SoyPixelsFormat::RGB>( PixelsMeta, img, CopyPixels );
	//load_image(img, argv[i]);
	// Make the image larger so we can detect small faces.
	//std::Debug << "scaling up for pyramid..." << std::endl;
	//pyramid_up(img);
	
	
	//	use the resized image
	auto Width = static_cast<float>(img.nc());
	auto Height = static_cast<float>(img.nr());

	
	// Now tell the face detector to give us a list of bounding boxes
	// around all the faces in the image.
	Soy::TScopeTimerPrint Timer_2("FindFace: detector(img)",TIMER_WARNING_MIN_MS);
	std::vector<rect_detection> FaceRects;
	auto adjust_threshold = 0;
	detector(img, FaceRects, adjust_threshold);
	Timer_2.Stop();
	
	for ( int f=0;	f<FaceRects.size();	f++ )
	{
		auto& FaceDetected = FaceRects[f];
		auto& FaceRect = FaceDetected.rect;
		//std::Debug << "Extracting face " << f << "/" << FaceRects.size() << " landmarks... Score = " << FaceDetected.detection_confidence << std::endl;
		
		Soy::Rectf FaceRectf( FaceRect.left(), FaceRect.top(), FaceRect.width(), FaceRect.height() );
		Soy::Rectf ImageRect( 0, 0, Width, Height );
		FaceRectf.Normalise( ImageRect );
		
		TFace NewFace = GetFaceLandmarks( img, FaceRectf );
		Faces.PushBack(NewFace);
	}

	//std::Debug << "found " << Faces.GetSize() << " faces" << std::endl;
}


TFace TDlib::GetFaceLandmarks(const SoyPixelsMeta& PixelsMeta,std::function<void(SoyPixelsImpl&)> CopyPixels,Soy::Rectf FaceRect)
{
	if ( PixelsMeta.GetFormat() == SoyPixelsFormat::RGB )
	{
		dlib::array2d<dlib::rgb_pixel> img;
		GetImageFromPixels<SoyPixelsFormat::RGB>( PixelsMeta, img, CopyPixels );
		return GetFaceLandmarks( img, FaceRect );
	}
	else
	{
		dlib::array2d<uint8_t> img;
		GetImageFromPixels<SoyPixelsFormat::Greyscale>( PixelsMeta, img, CopyPixels );
		return GetFaceLandmarks( img, FaceRect );
	}
}


template<typename PIXELTYPE>
TFace GetFaceLandmarks(dlib::shape_predictor& ShapePredictor,const dlib::array2d<PIXELTYPE>& Image,Soy::Rectf FaceRectf)
{
	using namespace dlib;
	
	Soy::TScopeTimerPrint Timer_1("auto& sp = *mShapePredictor;",TIMER_WARNING_MIN_MS);
	auto& sp = ShapePredictor;
	Timer_1.Stop();
	
	auto Width = static_cast<float>(Image.nc());
	auto Height = static_cast<float>(Image.nr());
	
	//	use the resized image
	auto NormaliseCoord = [&](const point& PositionPx)
	{
		vec2f Pos2( PositionPx.x(), PositionPx.y() );
		Pos2.x /= Width;
		Pos2.y /= Height;
		return Pos2;
	};
	
	TFace NewFace;
	NewFace.mRect = FaceRectf;
	
	Soy::Rectf ImageRect( 0, 0, Width, Height );
	FaceRectf.ScaleTo( ImageRect );
	
	rectangle FaceRect( FaceRectf.Left(), FaceRectf.Top(), FaceRectf.Right(), FaceRectf.Bottom() );
	
	Soy::TScopeTimerPrint Timer_3("FindFace: get shape(img)",TIMER_WARNING_MIN_MS);
	full_object_detection shape = sp( Image, FaceRect );
	Timer_3.Stop();
	
	auto PartCount = shape.num_parts();
	for ( int p=0;	p<PartCount;	p++ )
	{
		auto PositionPx = shape.part(p);
		auto Position2 = NormaliseCoord( PositionPx );
		NewFace.mFeatures.PushBack( Position2 );
	}
	
	return NewFace;
}

TFace TDlib::GetFaceLandmarks(const dlib::array2d<dlib::rgb_pixel>& Image,Soy::Rectf FaceRectf)
{
	Soy::TScopeTimerPrint Timer_1("auto& sp = *mShapePredictor;",TIMER_WARNING_MIN_MS);
	auto& sp = *mShapePredictor;
	Timer_1.Stop();
	return ::GetFaceLandmarks( sp, Image, FaceRectf );
}

TFace TDlib::GetFaceLandmarks(const dlib::array2d<uint8_t>& Image,Soy::Rectf FaceRectf)
{
	Soy::TScopeTimerPrint Timer_1("auto& sp = *mShapePredictor;",TIMER_WARNING_MIN_MS);
	auto& sp = *mShapePredictor;
	Timer_1.Stop();
	return ::GetFaceLandmarks( sp, Image, FaceRectf );
}

void TDlib::SetShapePredictorFaceLandmarks(TDlib& Copy)
{
	std::Debug << "copying facedetector data..." << std::endl;
	mFaceDetector.reset( new dlib::frontal_face_detector(*Copy.mFaceDetector) );
	//auto& fd = *mFaceDetector;
	//fd = *Copy.mFaceDetector;
	
	std::Debug << "copying shape_predictor data..." << std::endl;
	mShapePredictor.reset( new dlib::shape_predictor() );
	auto& sp = *mShapePredictor;
	sp = *Copy.mShapePredictor;
}

void TDlib::SetShapePredictorFaceLandmarks(ArrayBridge<uint8_t>& LandmarksDatBytes)
{
	mFaceLandmarksDat.Clear();
	mFaceLandmarksDat.Copy(LandmarksDatBytes);

	std::Debug << "loading facedetector data..." << std::endl;
	mFaceDetector.reset( new dlib::frontal_face_detector() );
	auto& fd = *mFaceDetector;
	fd = dlib::get_frontal_face_detector();

	std::Debug << "loading landmarks data..." << std::endl;
	mShapePredictor.reset( new dlib::shape_predictor() );
	auto& sp = *mShapePredictor;
	imemstream LandmarkDataMemStream( mFaceLandmarksDat.GetArray(), mFaceLandmarksDat.GetDataSize() );
	deserialize( sp, LandmarkDataMemStream );

}

