#pragma once

#include "SoyVector.h"
#include "SoyThread.h"
#include "Libs/ViveHandTracking/include/interface_gesture.hpp"


namespace Vive
{
	class THandTracker;
	class THandPose;

	namespace Joint
	{
		enum TYPE
		{
			Joint0 = 0,
			Joint1,
			Joint2,
			Joint3,
			Joint4,
			Joint5,
			Joint6,
			Joint7,
			Joint8,
			Joint9,
			Joint10,
			Joint11,
			Joint12,
			Joint13,
			Joint14,
			Joint15,
			Joint16,
			Joint17,
			Joint18,
			Joint19,
			Joint20,
			Joint21,

			Count
		};
	}

	namespace Gesture
	{
		enum TYPE
		{
			None = 0,
			Point,
			Fist,
			Okay,
			Like,
			Five,

			Count
		};
	}
}



class Vive::THandTracker : public SoyWorkerThread
{
public:
	THandTracker();
	~THandTracker();

	virtual bool	Iteration() override;

	void			PushGesture(const GestureResult& Gesture, int FrameIndex);
	bool			PopGesture(THandPose& LeftHand, THandPose& RightHand,bool GetLatest);

public:
	std::function<void()>	mOnNewGesture;
	std::mutex				mLastGestureLock;
	Array<THandPose>		mLastLeftPoses;
	Array<THandPose>		mLastRightPoses;
};


class Vive::THandPose
{
public:
	vec3f			mJoints[Joint::Count];
	Gesture::TYPE	mGesture = Gesture::None;	//	like a button!

public:
	void			EnumJoints(std::function<void(const vec3f&, const std::string&)> EnumJoint, const std::string& Prefix) const;
};

