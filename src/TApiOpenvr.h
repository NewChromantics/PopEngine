#pragma once
#include "TBind.h"
#include "Libs/OpenVr/headers/openvr.h"
#include "SoyVector.h"

class TImageWrapper;

namespace Opengl
{
	class TContext;
	class TTexture;
}

namespace Openvr
{
	class TApp;		//	common openvr access for hmd+overlay
	class TOverlay;
	class THmd;
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

	class TAppWrapper;
	class THmdWrapper;
	class TSkeletonWrapper;
	class TOverlayWrapper;
	DECLARE_BIND_TYPENAME(Hmd);
	DECLARE_BIND_TYPENAME(Overlay);
	DECLARE_BIND_TYPENAME(Skeleton);
}

class Openvr::TDeviceState
{
public:
	int32_t					mDeviceIndex = -1;
	vr::TrackedDevicePose_t	mPose;
	std::string				mTrackedName;
	std::string				mClassName;
	bool					mIsKeyFrame = false;	//	set keyframe for button changes, connect/disconnects
	Soy::Bounds3f			mLocalBounds;

	//	eye "devices" and cameras have extra data
	float4x4				mProjectionMatrix;	//	local to screen

	bool					HasLocalBounds() const
	{
		float w = mLocalBounds.max.x - mLocalBounds.min.x;
		float h = mLocalBounds.max.y - mLocalBounds.min.y;
		float d = mLocalBounds.max.z - mLocalBounds.min.z;
		return (w*h*d) != 0.f;
	}

	bool					HasProjectionMatrix() const
	{
		return true;
	}
};

//	fix this naming, poses or devices
class Openvr::TDeviceStates
{
public:
	bool							HasKeyframe();
	SoyTime							mTime;
	BufferArray<TDeviceState,99>	mDevices;
};



class ApiOpenvr::TAppWrapper
{
public:
	void			OnNewPoses(ArrayBridge<Openvr::TDeviceState>&& Poses);

	//	get a promise for new poses, this lets us not clog up JS job queues with callbacks
	void			WaitForPoses(Bind::TCallback& Params);

	//	this NEEDS to be called from a window render on the opengl thread...
	void			SubmitFrame(Bind::TCallback& Params);

	//	promise to fetch texture and request pixel-read
	void			WaitForMirrorImage(Bind::TCallback& Params);

protected:
	virtual Openvr::TApp&	GetApp() = 0;
	virtual void			SubmitFrame(BufferArray<Opengl::TTexture*,2>& Textures) = 0;

private:
	void					FlushPendingPoses();
	Openvr::TDeviceStates	PopPose();				//	get latest keyframe pose, or just last pose if no keyframes

	void					FlushPendingMirrors();

private:
	void					UpdateMirrorTexture(Opengl::TContext& Context,TImageWrapper& MirrorImage,bool ReadBackPixels);
	void					SetMirrorError(const char* Error);

protected:
	Bind::TPersistent				mMirrorImage;
	Bind::TPromiseQueue				mOnMirrorPromises;
	bool							mMirrorChanged = false;
	std::string						mMirrorError;
	
private:
	Bind::TPromiseQueue				mOnPosePromises;
	std::mutex						mPosesLock;
	Array<Openvr::TDeviceStates>	mPoses;
};


class ApiOpenvr::THmdWrapper : public Bind::TObjectWrapper<ApiOpenvr::BindType::Hmd,Openvr::THmd>, public TAppWrapper
{
public:
	THmdWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void			GetEyeMatrix(Bind::TCallback& Params);

protected:
	virtual void	SubmitFrame(BufferArray<Opengl::TTexture*, 2>& Textures) override;
	virtual Openvr::TApp&	GetApp() override;

public:
	std::shared_ptr<Openvr::THmd>&	mHmd = mObject;
};



class ApiOpenvr::TOverlayWrapper : public Bind::TObjectWrapper<ApiOpenvr::BindType::Overlay, Openvr::TOverlay>, public TAppWrapper
{
public:
	TOverlayWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;
	
	void			SetTransform(Bind::TCallback& Params);

protected:
	virtual void	SubmitFrame(BufferArray<Opengl::TTexture*, 2>& Textures) override;
	virtual Openvr::TApp&	GetApp() override;

public:
	std::shared_ptr<Openvr::TOverlay>&	mOverlay = mObject;
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

