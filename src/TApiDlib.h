#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"



namespace ApiDlib
{
	void	Bind(TV8Container& Container);
}

class TFace
{
public:
	Soy::Rectf				mRect;
	BufferArray<vec2f,100>	mFeatures;
};

//	gr: this may need to have a job queue, see if it's thread safe etc
class TDlib
{
public:
	void			GetFaceLandmarks(const SoyPixelsImpl& Pixels,ArrayBridge<TFace>&& Faces);

	void			SetShapePredictorFaceLandmarks(ArrayBridge<int>&& LandmarksDatBytes);
	
public:
	Array<uint8_t>		mFaceLandmarksDat;
};



class TDlibWrapper
{
public:
	TDlibWrapper() :
		mContainer		( nullptr ),
		mDlibJobQueue	( "DLibJobQueue")
	{
		mDlibJobQueue.Start();
	}
	~TDlibWrapper();
	
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	
	static v8::Local<v8::Value>				FindFace(const v8::CallbackInfo& Arguments);

	

	
public:
	v8::Persistent<v8::Object>	mHandle;
	TV8Container*				mContainer;

	TDlib						mDlib;
	SoyWorkerJobThread			mDlibJobQueue;
};

