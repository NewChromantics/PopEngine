#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"



namespace ApiDlib
{
	void	Bind(TV8Container& Container);
}


#include <dlib/image_processing/frontal_face_detector.h>
namespace dlib
{
	class shape_predictor;
	//class frontal_face_detector;
}

class TFace
{
public:
	Soy::Rectf				mRect;
	BufferArray<vec2f,100>	mFeatures;
};


//	gr: this may need to have a job queue, see if it's thread safe etc
class TDlib : public SoyWorkerJobThread
{
public:
	TDlib(const std::string& ThreadName) :
		SoyWorkerJobThread	( ThreadName )
	{
	}
	
	void			GetFaceLandmarks(const SoyPixelsMeta& PixelsMeta,std::function<void(SoyPixelsImpl&)> CopyPixels,ArrayBridge<TFace>&& Faces);
	TFace			GetFaceLandmarks(const SoyPixelsMeta& PixelsMeta,std::function<void(SoyPixelsImpl&)> CopyPixels,Soy::Rectf FaceRect);
	TFace			GetFaceLandmarks(const dlib::array2d<dlib::rgb_pixel>& Image,Soy::Rectf FaceRect);
	TFace			GetFaceLandmarks(const dlib::array2d<uint8_t>& Image,Soy::Rectf FaceRect);

	void			SetShapePredictorFaceLandmarks(TDlib& Copy);
	void			SetShapePredictorFaceLandmarks(ArrayBridge<int>& LandmarksDatBytes);

public:
	Array<uint8_t>		mFaceLandmarksDat;

	//	preloaded data
	std::shared_ptr<dlib::shape_predictor>			mShapePredictor;
	std::shared_ptr<dlib::frontal_face_detector>	mFaceDetector;
};



class TDlibWrapper
{
public:
	TDlibWrapper(size_t ThreadCount);
	~TDlibWrapper();
	
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	
	static v8::Local<v8::Value>				FindFaces(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				FindFaceFeatures(v8::TCallback& Arguments);

	//	this loads the shape predictors etc and copies to each thread
	void									SetShapePredictorFaceLandmarks(ArrayBridge<int>&& LandmarksDatBytes);

private:
	TDlib&									GetDlibJobQueue();
	
public:
	v8::Persistent<v8::Object>	mHandle;
	TV8Container*				mContainer;

private:
	Array<std::shared_ptr<TDlib>>	mDlibJobQueues;
};

