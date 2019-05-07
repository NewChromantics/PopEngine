#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"

#if defined(TARGET_WINDOWS)
#error not supported on this platform
#endif


namespace ApiDlib
{
	void	Bind(Bind::TContext& Context);
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
	void			SetShapePredictorFaceLandmarks(ArrayBridge<uint8_t>& LandmarksDatBytes);

public:
	Array<uint8_t>		mFaceLandmarksDat;

	//	preloaded data
	std::shared_ptr<dlib::shape_predictor>			mShapePredictor;
	std::shared_ptr<dlib::frontal_face_detector>	mFaceDetector;
};

class TDlibThreads
{
public:
	TDlibThreads(size_t ThreadCount);
	~TDlibThreads();

	//	this loads the shape predictors etc and copies to each thread
	void				SetShapePredictorFaceLandmarks(ArrayBridge<uint8_t>&& LandmarksDatBytes);
	TDlib&				GetJobQueue();
	
	Array<std::shared_ptr<TDlib>>	mThreads;
};


extern const char DlibWrapper_TypeName[];
class TDlibWrapper : public Bind::TObjectWrapper<DlibWrapper_TypeName,TDlibThreads>
{
public:
	TDlibWrapper(Bind::TContext& Context) :
		TObjectWrapper			( Context )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);

	virtual void		Construct(Bind::TCallback& Params) override;
	
	static void			FindFaces(Bind::TCallback& Params);
	static void			FindFaceFeatures(Bind::TCallback& Params);


private:
	std::shared_ptr<TDlibThreads>&	mDlib = mObject;
	TDlib&				GetDlibJobQueue()	{	return mDlib->GetJobQueue();	}
};

