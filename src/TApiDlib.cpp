#include "TApiDlib.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"

using namespace v8;

const char FindFace_FunctionName[] = "FindFace";


void ApiDlib::Bind(TV8Container& Container)
{
	Container.BindObjectType("Dlib", TDlibWrapper::CreateTemplate );
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
	
	//	alloc window
	//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
	//		but it also needs to know of the V8container to run stuff
	//		cyclic hell!
	auto* NewWrapper = new TDlibWrapper();
	
	//	store persistent handle to the javascript object
	NewWrapper->mHandle.Reset( Isolate, Arguments.This() );
	
	NewWrapper->mContainer = &Container;
	
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
			BufferArray<vec2f,100> Landmark2s;
			pThis->mDlib.GetFaceLandmarks(Pixels, GetArrayBridge(Landmark2s) );
			BufferArray<float,200> Landmarkfs;
			for ( int i=0;	i<Landmark2s.GetSize();	i++ )
			{
				Landmarkfs.PushBack( Landmark2s[i].x );
				Landmarkfs.PushBack( Landmark2s[i].y );
			}
			
			auto OnCompleted = [=](Local<Context> Context)
			{
				//	return face points here
				//	gr: can't do this unless we're in the javascript thread...
				auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent );
				auto LandmarksArray = v8::GetArray( *Context->GetIsolate(), GetArrayBridge(Landmarkfs) );
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
	auto& Dlib = This.mDlibJobQueue;
	Dlib.PushJob( RunFaceDetector );

	//	return the promise
	auto Promise = Resolver->GetPromise();
	return Promise;
}


void TDlib::GetFaceLandmarks(const SoyPixelsImpl &Pixels,ArrayBridge<vec2f>&& Landmarks)
{
	Landmarks.PushBack( vec2f(123,456) );
	//using namespace dlib;
	//throw Soy::AssertException("todo! Dlib::GetFaceLandmarks");
	/*
	// We need a face detector.  We will use this to get bounding boxes for
	// each face in an image.
	auto detector = get_frontal_face_detector();
	// And we also need a shape_predictor.  This is the tool that will predict face
	// landmark positions given an image and face bounding box.  Here we are just
	// loading the model from the shape_predictor_68_face_landmarks.dat file you gave
	// as a command line argument.
	shape_predictor sp;
	deserialize(argv[1]) >> sp;

	// Loop over all the images provided on the command line.
	for (int i = 2; i < argc; ++i)
	{
	cout << "processing image " << argv[i] << endl;
	array2d<rgb_pixel> img;
	load_image(img, argv[i]);
	// Make the image larger so we can detect small faces.
	pyramid_up(img);

	// Now tell the face detector to give us a list of bounding boxes
	// around all the faces in the image.
	std::vector<rectangle> dets = detector(img);
	cout << "Number of faces detected: " << dets.size() << endl;

	// Now we will go ask the shape_predictor to tell us the pose of
	// each face we detected.
	std::vector<full_object_detection> shapes;
	for (unsigned long j = 0; j < dets.size(); ++j)
	{
	full_object_detection shape = sp(img, dets[j]);
	cout << "number of parts: "<< shape.num_parts() << endl;
	cout << "pixel position of first part:  " << shape.part(0) << endl;
	cout << "pixel position of second part: " << shape.part(1) << endl;
	// You get the idea, you can get all the face part locations if
	// you want them.  Here we just store them in shapes so we can
	// put them on the screen.
	shapes.push_back(shape);
	}

	// Now let's view our face poses on the screen.
	win.clear_overlay();
	win.set_image(img);
	win.add_overlay(render_face_detecti
	}
	*/

}
