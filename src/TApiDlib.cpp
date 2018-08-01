#include "TApiDlib.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

//	if dgesvd is missing, link with the accelerate.framework
#include <dlib/image_processing/frontal_face_detector.h>
//#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
//#include <dlib/gui_widgets.h>
//#include <dlib/image_io.h>

using namespace v8;

const char FindFace_FunctionName[] = "FindFace";


void ApiDlib::Bind(TV8Container& Container)
{
	Container.BindObjectType("Dlib", TDlibWrapper::CreateTemplate );
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
		std::shared_ptr<SoyWorkerJobThread> Queue( new SoyWorkerJobThread(Name.str() ) );
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
	NewWrapper->mDlib.SetShapePredictorFaceLandmarks( GetArrayBridge(LandmarksDatBytes) );
	
	
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
	Container.BindFunction<FindFace_FunctionName>( InstanceTemplate, FindFace );
	
	return ConstructorFunc;
}

SoyWorkerJobThread& TDlibWrapper::GetDlibJobQueue()
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


template<typename TYPE>
v8::Persistent<TYPE,CopyablePersistentTraits<TYPE>> MakeLocal(v8::Isolate* Isolate,Local<TYPE> LocalHandle)
{
	Persistent<TYPE,CopyablePersistentTraits<TYPE>> PersistentHandle;
	PersistentHandle.Reset( Isolate, LocalHandle );
	return PersistentHandle;
}

v8::Local<v8::Value> TDlibWrapper::FindFace(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TDlibWrapper>( Arguments.This() );
	auto* Isolate = Params.mIsolate;

	auto* pThis = &This;
	
	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
	auto ResolverPersistent = v8::GetPersistent( *Isolate, Resolver );

	auto TargetPersistent = v8::GetPersistent( *Isolate, Arguments[0] );
	auto* TargetImage = &v8::GetObject<TImageWrapper>(Arguments[0]);
	auto* Container = &Params.mContainer;
	
	auto RunFaceDetector = [=]
	{
		try
		{
			auto& Pixels = TargetImage->GetPixels();
			BufferArray<TFace,100> Faces;
			pThis->mDlib.GetFaceLandmarks(Pixels, GetArrayBridge(Faces) );
		
			//	temp
			BufferArray<float,1000> Features;
			for ( int f=0;	f<Faces.GetSize();	f++ )
			{
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
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent );
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
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent );
				//	gr: does this need to be an exception? string?
				auto Error = String::NewFromUtf8( Isolate, ExceptionString.c_str() );
				//auto Exception = v8::GetException( *Context->GetIsolate(), ExceptionString)
				//ResolverLocal->Reject( Exception );
				ResolverLocal->Reject( Error );
			};
			Container->QueueScoped( OnError );
		}
	};
	
	auto& Dlib = This.GetDlibJobQueue();
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



void TDlib::GetFaceLandmarks(const SoyPixelsImpl &Pixels,ArrayBridge<TFace>&& Faces)
{
	Soy::TScopeTimerPrint Timer_FindFace("TDlib::GetFaceLandmarks",10);

	using namespace dlib;
#define COPY_DETECTORS
	
#if defined(COPY_DETECTORS)
	auto detector = *mFaceDetector;
#else
	auto& detector = *mFaceDetector;
#endif
	// We need a face detector.  We will use this to get bounding boxes for
	// each face in an image.
	
	// And we also need a shape_predictor.  This is the tool that will predict face
	// landmark positions given an image and face bounding box.  Here we are just
	// loading the model from the shape_predictor_68_face_landmarks.dat file you gave
	// as a command line argument.
	//gr: load once
#if defined(COPY_DETECTORS)
	auto sp = *mShapePredictor;
#else
	auto& sp = *mShapePredictor;
#endif

	//	gr: switch to soypixels fast rgba->rgb conversion and copy rows!
	array2d<rgb_pixel> img;
	img.set_size( Pixels.GetHeight(), Pixels.GetWidth() );
	
	if ( Pixels.GetFormat() == SoyPixelsFormat::RGB )
	{
		Soy::TScopeTimerPrint Timer_1("FindFace: Copying pixels to img",10);
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
	//load_image(img, argv[i]);
	// Make the image larger so we can detect small faces.
	//std::Debug << "scaling up for pyramid..." << std::endl;
	//pyramid_up(img);

	//	use the resized image
	auto NormaliseCoord = [&](const point& PositionPx)
	{
		vec2f Pos2( PositionPx.x(), PositionPx.y() );
		Pos2.x /= img.nc();
		Pos2.y /= img.nr();
		return Pos2;
	};
	
	// Now tell the face detector to give us a list of bounding boxes
	// around all the faces in the image.
	Soy::TScopeTimerPrint Timer_2("FindFace: detector(img)",10);
	std::vector<rect_detection> FaceRects;
	auto adjust_threshold = 0;
	detector(img, FaceRects, adjust_threshold);
	Timer_2.Stop();
	
	for ( int f=0;	f<FaceRects.size();	f++ )
	{
		auto& FaceDetected = FaceRects[f];
		auto& FaceRect = FaceDetected.rect;
		std::Debug << "Extracting face " << f << "/" << FaceRects.size() << " landmarks... Score = " << FaceDetected.detection_confidence << std::endl;
		
		Soy::TScopeTimerPrint Timer_3("FindFace: get shape(img)",1);
		full_object_detection shape = sp(img, FaceRect);
		Timer_3.Stop();
		
		TFace NewFace;
		NewFace.mRect = Soy::Rectf( FaceRect.left(), FaceRect.top(), FaceRect.width(), FaceRect.height() );
			
		Soy::TScopeTimerPrint Timer_4("FindFace: getting parts",1);
		auto PartCount = shape.num_parts();
		for ( int p=0;	p<PartCount;	p++ )
		{
			auto PositionPx = shape.part(p);
			auto Position2 = NormaliseCoord( PositionPx );
			NewFace.mFeatures.PushBack( Position2 );
		}
		Timer_4.Stop();
		
		Faces.PushBack(NewFace);
	}

	std::Debug << "found " << Faces.GetSize() << " faces" << std::endl;
}
					 
void TDlib::SetShapePredictorFaceLandmarks(ArrayBridge<int>&& LandmarksDatBytes)
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
/*
	get_frontal_face_detector() );
	auto& sp = *mShapePredictor;
	imemstream LandmarkDataMemStream( mFaceLandmarksDat.GetArray(), mFaceLandmarksDat.GetDataSize() );
	deserialize( sp, LandmarkDataMemStream );
	auto detector = get_frontal_face_detector();
*/
	
	std::Debug << "loading landmarks data..." << std::endl;
	mShapePredictor.reset( new dlib::shape_predictor() );
	auto& sp = *mShapePredictor;
	imemstream LandmarkDataMemStream( mFaceLandmarksDat.GetArray(), mFaceLandmarksDat.GetDataSize() );
	deserialize( sp, LandmarkDataMemStream );

}

