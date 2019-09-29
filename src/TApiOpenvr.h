#pragma once
#include "TBind.h"


namespace Openvr
{
	class THmd;
	class THmdFrame;
}

namespace Vive
{
	class THandTracker;
}

namespace ApiOpenvr
{
	void	Bind(Bind::TContext& Context);

	class THmdWrapper;
	class TSkeletonWrapper;
	DECLARE_BIND_TYPENAME(Hmd);
	DECLARE_BIND_TYPENAME(Skeleton);
}


class ApiOpenvr::THmdWrapper : public Bind::TObjectWrapper<ApiOpenvr::BindType::Hmd,Openvr::THmd>
{
public:
	THmdWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void			OnRender(Openvr::THmdFrame& Left,Openvr::THmdFrame& Right);
	
	void			SubmitEyeTexture(Bind::TCallback& Params);
	void			GetEyeMatrix(Bind::TCallback& Params);

	//	todo: make this a promise, then JS can wait until we know we have a new pose, ready for
	//		a frame, and render as soon as possible
	//	currently blocks until ready to draw
	void			BeginFrame(Bind::TCallback& Params);


public:
	std::shared_ptr<Openvr::THmd>&	mHmd = mObject;
};


class ApiOpenvr::TSkeletonWrapper : public Bind::TObjectWrapper<ApiOpenvr::BindType::Skeleton, Vive::THandTracker>
{
public:
	TSkeletonWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	//	returns promise for next frame
	void			GetNextFrame(Bind::TCallback& Params);

	void			OnNewGesture();

public:
	std::shared_ptr<Vive::THandTracker>&	mHandTracker = mObject;
	Bind::TPromiseQueue					mNextFramePromiseQueue;
};

