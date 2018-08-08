#include "TApiDlib.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

//	if dgesvd is missing, link with the accelerate.framework
#include <dlib/image_processing/frontal_face_detector.h>
//#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
//#include <dlib/gui_widgets.h>
//#include <dlib/image_io.h>


#define TIMER_WARNING_MIN_MS	30

using namespace v8;

const char FindFaces_FunctionName[] = "FindFaces";
const char FindFaceFeatures_FunctionName[] = "FindFaceFeatures";


void ApiDlib::Bind(TV8Container& Container)
{
	Container.BindObjectType("Dlib", TDlibWrapper::CreateTemplate, nullptr );
}


TDlibWrapper::TDlibWrapper(size_t ThreadCount) :
	mContainer		( nullptr )
{
	if ( ThreadCount < 1 )
		ThreadCount = 1;

	for ( int i=0;	i<ThreadCount;	i++ )
	{
		std::stringstream Name;
		Name << "Dlib Job Queue " << i;
		std::shared_ptr<TDlib> Queue( new TDlib(Name.str() ) );
		mDlibJobQueues.PushBack(Queue);
		Queue->Start();
	}
}


void TDlibWrapper::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
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
	auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
	
	auto ThreadCountArg = Arguments[1];
	auto LandmarksDatArg = Arguments[0];
	
	size_t ThreadCount = 1;
	if ( ThreadCountArg->IsNumber() )
		ThreadCount = ThreadCountArg.As<Number>()->Uint32Value();
	
	
	//	alloc window
	//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
	//		but it also needs to know of the V8container to run stuff
	//		cyclic hell!
	auto* NewWrapper = new TDlibWrapper(ThreadCount);
	
	//	store persistent handle to the javascript object
	NewWrapper->mHandle.Reset( Isolate, Arguments.This() );
	NewWrapper->mContainer = &Container;

	//	first argument is the landmarks data as bytes
	Array<int> LandmarksDatBytes;
	v8::EnumArray( LandmarksDatArg, GetArrayBridge(LandmarksDatBytes), "DLib arg0 (shape_predictor_68_face_landmarks.dat)" );
	NewWrapper->SetShapePredictorFaceLandmarks( GetArrayBridge(LandmarksDatBytes) );
	
	
	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewWrapper ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}


Local<FunctionTemplate> TDlibWrapper::CreateTemplate(TV8Container& Container)
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
	
	//	add members
	Container.BindFunction<FindFaces_FunctionName>( InstanceTemplate, FindFaces );
	Container.BindFunction<FindFaceFeatures_FunctionName>( InstanceTemplate, FindFaceFeatures );
	
	return ConstructorFunc;
}

TDlib& TDlibWrapper::GetDlibJobQueue()
{
	//	get queue with least jobs
	auto LeastJobQueue = 0;
	for ( int i=0;	i<mDlibJobQueues.GetSize();	i++ )
	{
		auto& Queue = *mDlibJobQueues[i];
		auto& BestQueue = *mDlibJobQueues[LeastJobQueue];
		if ( Queue.GetJobCount() < BestQueue.GetJobCount() )
			LeastJobQueue = i;
	}
	
	return *mDlibJobQueues[LeastJobQueue];
}

//	this loads the shape predictors etc and copies to each thread
void TDlibWrapper::SetShapePredictorFaceLandmarks(ArrayBridge<int>&& LandmarksDatBytes)
{
	//	setup first one, then copy to others
	auto& Dlib0 = *mDlibJobQueues[0];
	Dlib0.SetShapePredictorFaceLandmarks(LandmarksDatBytes);
	for ( int i=1;	i<mDlibJobQueues.GetSize();	i++ )
	{
		auto& DlibN = *mDlibJobQueues[i];
		DlibN.SetShapePredictorFaceLandmarks(Dlib0);
	}
}


template<typename TYPE>
v8::Persistent<TYPE,CopyablePersistentTraits<TYPE>> MakeLocal(v8::Isolate* Isolate,Local<TYPE> LocalHandle)
{
	Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
	PersistentHandle.Reset( Isolate, LocalHandle );
	return PersistentHandle;
}

v8::Local<v8::Value> TDlibWrapper::FindFaces(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TDlibWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;

	auto* pThis = &This;
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = std::make_shared<V8Storage<Promise::Resolver>>( Params.GetIsolate(), Resolver );

	auto TargetPersistent = v8::GetPersistent( *Isolate, Arguments[0] );
	auto* TargetImage = &v8::GetObject<TImageWrapper>(Arguments[0]);
	auto* Container = &Params.mContainer;
	
	auto& Dlib = This.GetDlibJobQueue();
	auto RunFaceDetector = [=,&Dlib]
	{
		try
		{
			auto& Pixels = TargetImage->GetPixels();
			BufferArray<TFace,100> Faces;
			Dlib.GetFaceLandmarks(Pixels, GetArrayBridge(Faces) );
		
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
			
			auto OnCompleted = [=](Local<Context> Context)
			{
				//	return face points here
				//	gr: can't do this unless we're in the javascript thread...
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent->mPersistent );
				auto LandmarksArray = v8::GetArray( *Context->GetIsolate(), GetArrayBridge(Features) );
				ResolverLocal->Resolve( LandmarksArray );
				//auto Message = String::NewFromUtf8( Isolate, "Yay!");
				//ResolverLocal->Resolve( Message );
			};

			//	queue the completion, doesn't need to be done instantly
			Container->QueueScoped( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Local<Context> Context)
			{
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent->mPersistent );
				//	gr: does this need to be an exception? string?
				auto Error = String::NewFromUtf8( Isolate, ExceptionString.c_str() );
				//auto Exception = v8::GetException( *Context->GetIsolate(), ExceptionString)
				//ResolverLocal->Reject( Exception );
				ResolverLocal->Reject( Error );
			};
			Container->QueueScoped( OnError );
		}
	};
	Dlib.PushJob( RunFaceDetector );

	//	return the promise
	auto Promise = Resolver->GetPromise();
	return Promise;
}


v8::Local<v8::Value> TDlibWrapper::FindFaceFeatures(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TDlibWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;
	
	auto* pThis = &This;
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto pResolverPersistent = std::make_shared<V8Storage<v8::Promise::Resolver>>( *Isolate, Resolver );
	
	auto TargetPersistent = v8::GetPersistent( *Isolate, Arguments[0] );
	auto* TargetImage = &v8::GetObject<TImageWrapper>(Arguments[0]);
	auto* Container = &Params.mContainer;

	BufferArray<float,4> RectFloats;
	v8::EnumArray( Arguments[1], GetArrayBridge(RectFloats), "FindFaceFeatures(img,rect)" );
	Soy::Rectf TargetRect( RectFloats[0], RectFloats[1], RectFloats[2], RectFloats[3] );

	auto& Dlib = This.GetDlibJobQueue();
	auto RunFaceDetector = [=,&Dlib]
	{
		try
		{
			//	gr: copy pixels
			SoyPixels Pixels;
			TargetImage->GetPixels(Pixels);
			auto Face = Dlib.GetFaceLandmarks(Pixels, TargetRect );
			
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
			
			auto OnCompleted = [=](Local<Context> Context)
			{
				//	return face points here
				//	gr: can't do this unless we're in the javascript thread...
				auto ResolverLocal = v8::GetLocal( *Isolate, pResolverPersistent->mPersistent );
				auto LandmarksArray = v8::GetArray( *Context->GetIsolate(), GetArrayBridge(Features) );

				//	gr: these seem to be getting cleaned up on garbage collect, I think
				ResolverLocal->Resolve( LandmarksArray );
				//auto Message = String::NewFromUtf8( Isolate, "Yay!");
				//ResolverLocal->Resolve( Message );
			};
			
			//	queue the completion, doesn't need to be done instantly
			Container->QueueScoped( OnCompleted );
		}
		catch(std::exception& e)
		{
			//	queue the error callback
			std::string ExceptionString(e.what());
			auto OnError = [=](Local<Context> Context)
			{
				auto ResolverLocal = v8::GetLocal( *Isolate, pResolverPersistent->mPersistent );
				//	gr: does this need to be an exception? string?
				auto Error = String::NewFromUtf8( Isolate, ExceptionString.c_str() );
				//auto Exception = v8::GetException( *Context->GetIsolate(), ExceptionString)
				//ResolverLocal->Reject( Exception );
				ResolverLocal->Reject( Error );
			};
			Container->QueueScoped( OnError );
		}
	};
	Dlib.PushJob( RunFaceDetector );
	
	//	return the promise
	auto Promise = Resolver->GetPromise();
	return Promise;
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


dlib::array2d<dlib::rgb_pixel> GetImageFromPixels(const SoyPixelsImpl &Pixels)
{
	using namespace dlib;
	array2d<rgb_pixel> img;
	img.set_size( Pixels.GetHeight(), Pixels.GetWidth() );
	
	if ( Pixels.GetFormat() == SoyPixelsFormat::RGB )
	{
		Soy::TScopeTimerPrint Timer_1("FindFace: Copying pixels to img",TIMER_WARNING_MIN_MS);
		auto* ImgPixelsByte = &img.begin()->red;
		SoyPixelsRemote imgPixels( ImgPixelsByte, Pixels.GetWidth(), Pixels.GetHeight(), Pixels.GetMeta().GetDataSize(), Pixels.GetFormat() );
		imgPixels.Copy( Pixels );
	}
	else
	{
		std::Debug << "dlib converting " << Pixels.GetFormat() << " pixels to " << SoyPixelsFormat::RGB << "..." << std::endl;
		for ( int y=0;	y<img.nr();	y++ )
		{
			auto Row = img[y];
			auto* FirstDstPixel = &Row[0].red;
			auto* FirstSrcPixel = &Pixels.GetPixelPtr( 0, y, 0 );
			
			auto DstStep = 3;
			auto SrcStep = Pixels.GetChannels();
			
			for ( int x=0;	x<Row.nc();	x++ )
			{
				FirstDstPixel[0] = FirstSrcPixel[0%SrcStep];
				FirstDstPixel[1] = FirstSrcPixel[1%SrcStep];
				FirstDstPixel[2] = FirstSrcPixel[2%SrcStep];
				FirstSrcPixel += SrcStep;
				FirstDstPixel += DstStep;
			}
		}
	}
	return img;
}


void TDlib::GetFaceLandmarks(const SoyPixelsImpl &Pixels,ArrayBridge<TFace>&& Faces)
{
	Soy::TScopeTimerPrint Timer_FindFace("TDlib::GetFaceLandmarks",TIMER_WARNING_MIN_MS);

	using namespace dlib;
	
	// We need a face detector.  We will use this to get bounding boxes for
	// each face in an image.
	auto& detector = *mFaceDetector;

	auto img = GetImageFromPixels( Pixels );

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
		std::Debug << "Extracting face " << f << "/" << FaceRects.size() << " landmarks... Score = " << FaceDetected.detection_confidence << std::endl;
		
		Soy::Rectf FaceRectf( FaceRect.left(), FaceRect.top(), FaceRect.width(), FaceRect.height() );
		Soy::Rectf ImageRect( 0, 0, Width, Height );
		FaceRectf.Normalise( ImageRect );
		
		TFace NewFace = GetFaceLandmarks( img, FaceRectf );
		Faces.PushBack(NewFace);
	}

	std::Debug << "found " << Faces.GetSize() << " faces" << std::endl;
}


TFace TDlib::GetFaceLandmarks(const SoyPixelsImpl &Pixels,Soy::Rectf FaceRect)
{
	auto img = GetImageFromPixels( Pixels );
	
	return GetFaceLandmarks( img, FaceRect );
}



TFace TDlib::GetFaceLandmarks(const dlib::array2d<dlib::rgb_pixel>& Image,Soy::Rectf FaceRectf)
{
	using namespace dlib;
	
	Soy::TScopeTimerPrint Timer_1("auto& sp = *mShapePredictor;",TIMER_WARNING_MIN_MS);
	auto& sp = *mShapePredictor;
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

void TDlib::SetShapePredictorFaceLandmarks(ArrayBridge<int>& LandmarksDatBytes)
{
	mFaceLandmarksDat.Clear();
	for ( int i=0;	i<LandmarksDatBytes.GetSize();	i++ )
	{
		auto Byte = LandmarksDatBytes[i];
		mFaceLandmarksDat.PushBack( size_cast<uint8_t>(Byte) );
	}

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

