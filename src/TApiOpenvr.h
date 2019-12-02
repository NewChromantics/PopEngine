#pragma once
#include "TBind.h"
#include "Libs/OpenVr/headers/openvr.h"


namespace Opengl
{
	class TContext;
	class TTexture;
}

namespace Openvr
{
	class THmd;
	class THmdFrame;
	class TDeviceState;
	class TDeviceStates;

	typedef std::function<void(Opengl::TContext&,Opengl::TTexture&,Opengl::TTexture&)>	TFinishedEyesFunction;
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

class Openvr::TDeviceState
{
public:
	uint32_t				mDeviceIndex = 0;
	vr::TrackedDevicePose_t	mPose;
	std::string				mTrackedName;
	std::string				mClassName;
	bool					mIsKeyFrame = false;	//	set keyframe for button changes, connect/disconnects
};

//	fix this naming, poses or devices
class Openvr::TDeviceStates
{
public:
	bool							HasKeyframe();
	SoyTime							mTime;
	BufferArray<TDeviceState,64>	mDevices;
};


class ApiOpenvr::THmdWrapper : public Bind::TObjectWrapper<ApiOpenvr::BindType::Hmd,Openvr::THmd>
{
public:
	THmdWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void			OnNewPoses(ArrayBridge<Openvr::TDeviceState>&& Poses);
	void			OnRender(Openvr::TFinishedEyesFunction& SubmitEyeTextures);
	
	//	get a promise for new poses, this lets us not clog up JS job queues with callbacks
	//	todo: send eye matrix's with this
	void			WaitForPoses(Bind::TCallback& Params);


	//	todo: newer; make submit-eye-textures an explicit call, expected inside a render callback
	//		to immediately send textures to HMD
	//		this is better than a callback which then needs to find an opengl job from openvr,
	//		but need to make sure that fits web (which may be a different context)
	void			SubmitEyeTexture(Bind::TCallback& Params);
	void			GetEyeMatrix(Bind::TCallback& Params);
	//	todo: make this a promise, then JS can wait until we know we have a new pose, ready for
	//		a frame, and render as soon as possible
	//	currently blocks until ready to draw
	void			BeginFrame(Bind::TCallback& Params);

protected:
	void					FlushPendingPoses();
	Openvr::TDeviceStates	PopPose();				//	get latest keyframe pose, or just last pose if no keyframes

public:
	std::shared_ptr<Openvr::THmd>&	mHmd = mObject;
	
	//	todo: remove this
	Bind::TPersistent		mRenderContext;
	
	Bind::TPromiseQueue				mOnPosePromises;
	std::mutex						mPosesLock;
	Array<Openvr::TDeviceStates>	mPoses;
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

