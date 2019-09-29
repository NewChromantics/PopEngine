#include "SoyViveHandTracker.h"
#include "Libs/ViveHandTracking/include/interface_gesture.hpp"
#include "MagicEnum/include/magic_enum.hpp"


namespace Vive
{
	void	IsOkay(GestureFailure Error,const char* Context);
}

void Vive::IsOkay(GestureFailure Error, const char* Context)
{
	if ( Error == GestureFailureNone )
		return;

	std::stringstream ErrorStr;
	ErrorStr << "ViveHandTracker Error " << magic_enum::enum_name<GestureFailure>(Error) << " in " << Context;
	throw Soy::AssertException(ErrorStr);
}


Vive::THandTracker::THandTracker() :
	SoyWorkerThread	(__PRETTY_FUNCTION__,SoyWorkerWaitMode::Sleep)
{
	//	load dll!
	GestureOption Mode;
	auto Error = StartGestureDetection(&Mode);
	IsOkay(Error,"StartGestureDetection");

	Start();
}

Vive::THandTracker::~THandTracker()
{
	StopGestureDetection();
}


bool Vive::THandTracker::Iteration()
{
	const GestureResult* Poses = nullptr;
	int FrameIndex = -99;
	auto PoseCount = GetGestureResult(&Poses, &FrameIndex);

	if (FrameIndex == -1)
	{
		//	detection not started or stopped due to error
		std::Debug << __PRETTY_FUNCTION__ << " got error frame index(" << FrameIndex << ")" << std::endl;
		return false;
	}

	if (PoseCount <= 0)
	{
		//std::Debug << __PRETTY_FUNCTION__ << " pose count is " << PoseCount << std::endl;
		return true;
	}

	//	make a new pose from each gesture
	for (auto p = 0; p < PoseCount; p++)
	{
		auto& Pose = Poses[p];
		PushGesture(Pose, FrameIndex);
	}

	if (mOnNewGesture)
		mOnNewGesture();

	return true;
}

Vive::Gesture::TYPE GetGesture(GestureType GestureType)
{
	return static_cast<Vive::Gesture::TYPE>(GestureType );
}

void Vive::THandTracker::PushGesture(const GestureResult& Gesture,int FrameIndex)
{
	THandPose Pose;
	Pose.mGesture = GetGesture( Gesture.gesture );

	auto PointSize = 3;
	for ( auto PointIndex=0;	PointIndex<std::size(Gesture.points);	PointIndex+= PointSize)
	{
		auto JointIndex = PointIndex / 3;
		auto& Joint = Pose.mJoints[JointIndex];
		//	todo: make sure gesture is in skeleton mode
		//	+x is right, +y is up, +z is front. Unit is meter.
		Joint.x = Gesture.points[PointIndex + 0];
		Joint.y = Gesture.points[PointIndex + 1];
		Joint.z = Gesture.points[PointIndex + 2];

		//	0,0,0 is invalid
	}

	std::lock_guard<std::mutex> Lock(mLastGestureLock);
	if ( Gesture.isLeft )
		mLastLeftPoses.PushBack(Pose);
	else
		mLastRightPoses.PushBack(Pose);
}

bool Vive::THandTracker::PopGesture(THandPose& LeftHand, THandPose& RightHand,bool Latest)
{
	std::lock_guard<std::mutex> Lock(mLastGestureLock);

	//	gr: this needs to have a "no pose" thing
	auto GetPose = [&](Array<THandPose>& Poses, THandPose& Pose)
	{
		if ( Poses.IsEmpty() )
			return false;

		if (!Latest)
		{
			Pose = Poses.PopAt(0);
			return true;
		}

		Pose = Poses.GetBack();
		Poses.Clear(false);
		return true;
	};
	auto ValidPoseCount = 0;
	ValidPoseCount += GetPose(mLastLeftPoses, LeftHand);
	ValidPoseCount += GetPose(mLastRightPoses, RightHand);

	return ValidPoseCount > 0;
}



void Vive::THandPose::EnumJoints(std::function<void(const vec3f&, const std::string&)> EnumJoint, const std::string& Prefix) const
{
	for (auto i = 0; i < std::size(mJoints); i++)
	{
		auto& xyz = mJoints[i];
		auto Joint = static_cast<Vive::Joint::TYPE>(i);
		std::string Name;
		Name += Prefix;
		Name += magic_enum::enum_name<Vive::Joint::TYPE>(Joint);
		EnumJoint( xyz, Name );
	}
}
