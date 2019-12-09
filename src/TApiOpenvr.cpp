#include "TApiOpenvr.h"
#include "Libs/OpenVr/headers/openvr.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"
#include "SoyOpengl.h"
#include "SoyViveHandTracker.h"
#include "MagicEnum/include/magic_enum.hpp"
#include <string_view>
using namespace std::literals;
//	on OSX, make sure you link to the bin/osx32/dylib NOT the one in /lib/

namespace ApiOpenvr
{
	const char Namespace[] = "Pop.Openvr";

	DEFINE_BIND_TYPENAME(Hmd);
	DEFINE_BIND_TYPENAME(Overlay);
	DEFINE_BIND_TYPENAME(Skeleton);

	//	deprecate these
	DEFINE_BIND_FUNCTIONNAME(GetEyeMatrix);
	DEFINE_BIND_FUNCTIONNAME(GetNextFrame);

	DEFINE_BIND_FUNCTIONNAME( WaitForPoses);
	DEFINE_BIND_FUNCTIONNAME( SubmitFrame);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(OverlayWaitForPoses, WaitForPoses);
	DEFINE_BIND_FUNCTIONNAME_OVERRIDE(OverlaySubmitFrame, SubmitFrame);


	const std::string LeftHandJointPrefix = "Left";
	const std::string RightHandJointPrefix = "Right";
}

namespace Openvr
{
	const std::string	EyeName_Left = "Left";
	const std::string	EyeName_Right = "Right";

	void	IsOkay(vr::EVRInitError Error,const std::string& Context);
	void	IsOkay(vr::EVRCompositorError Error,const std::string& Context);
	void	IsOkay(vr::ETrackedPropertyError Error, const std::string& Context);
	void	IsOkay(vr::EVROverlayError Error, const std::string& Context);

	class TDeviceState;
}

namespace ViveHandTracker
{
#include "Libs/ViveHandTracking/include/interface_gesture.hpp"
}



//	maybe rename to camera
class TEyeMatrix
{
public:
	std::string			mName;
	vr::HmdMatrix44_t	mProjection;
	vr::HmdMatrix44_t	mPose;
	uint32_t			mRenderTargetWidth = 0;		//	"recommended"
	uint32_t			mRenderTargetHeight = 0;	//	"recommended"
};

class Openvr::THmd : public SoyThread
{
public:
	THmd(bool OverlayApp);
	~THmd();

	virtual void	Thread() override;

	void			SubmitFrame(Opengl::TTexture& Left, Opengl::TTexture& Right);
	TEyeMatrix		GetEyeMatrix(const std::string& EyeName);

private:
	void			SubmitEyeFrame(vr::Hmd_Eye Eye,Opengl::TAsset Texture);
	void			EnumEyes(std::function<void(const TEyeMatrix& Eye)>& Enum);
	void			WaitForFrameStart();

public:
	std::function<void(ArrayBridge<TDeviceState>&&)>	mOnNewPoses;
	vr::IVRSystem*	mHmd = nullptr;
	float			mNearClip = 0.1f;
	float			mFarClip = 40.0f;
};




class Openvr::TOverlay : public SoyWorkerThread
{
public:
	TOverlay(const std::string& Name);
	~TOverlay();

	virtual bool	Iteration() override;
	void			SubmitFrame(Opengl::TTexture& Image);

protected:
	void			HandleEvent(vr::VREvent_t& Event);

public:
	vr::VROverlayHandle_t	mHandle = vr::k_ulOverlayHandleInvalid;
	vr::VROverlayHandle_t	mThumbnailHandle = vr::k_ulOverlayHandleInvalid;
	vr::IVRSystem*			mSystem = nullptr;
	vr::IVROverlay*			mOverlay = nullptr;
};



void ApiOpenvr::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<THmdWrapper>(Namespace);
	Context.BindObjectType<TOverlayWrapper>(Namespace);
	Context.BindObjectType<TSkeletonWrapper>(Namespace);
}


void ApiOpenvr::THmdWrapper::Construct(Bind::TCallback& Params)
{
	//auto DeviceName = Params.GetArgumentFilename(0);

	bool IsOverlay = false;
	if ( !Params.IsArgumentUndefined(1) )
		IsOverlay = Params.GetArgumentBool(1);
	
	mHmd.reset( new Openvr::THmd(IsOverlay) );

	mHmd->mOnNewPoses = std::bind(&THmdWrapper::OnNewPoses, this, std::placeholders::_1);
}


void ApiOpenvr::THmdWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::WaitForPoses>(&TAppWrapper::WaitForPoses);
	Template.BindFunction<BindFunction::SubmitFrame>(&TAppWrapper::SubmitFrame);
}

void ApiOpenvr::TOverlayWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::OverlayWaitForPoses>(&TAppWrapper::WaitForPoses);
	Template.BindFunction<BindFunction::OverlaySubmitFrame>(&TAppWrapper::SubmitFrame);
}



void ApiOpenvr::TAppWrapper::WaitForPoses(Bind::TCallback& Params)
{
	auto NewPromise = mOnPosePromises.AddPromise( Params.mLocalContext );
	Params.Return( NewPromise );
	
	FlushPendingPoses();
}

void ApiOpenvr::TAppWrapper::SubmitFrame(Bind::TCallback& Params)
{
	//	either an array of images, or just one image
	//	support 2 args too
	//	gr: we're expecting this to be called whilst on a render thread...
	//		are we? and the JS engine is locked to that thread?
	//		check this (openvr will fail I guess)
	BufferArray<Opengl::TTexture*, 2> Textures;
	for (auto i = 0; i < Params.GetArgumentCount(); i++)
	{
		auto& Image = Params.GetArgumentPointer<TImageWrapper>(i);
		auto& Texture = Image.GetTexture();
		Textures.PushBack(&Texture);
	}

	SubmitFrame(Textures);
}

void ApiOpenvr::THmdWrapper::SubmitFrame(BufferArray<Opengl::TTexture*, 2>& Textures)
{
	auto& Hmd = *mHmd;
	Hmd.SubmitFrame(*Textures[0], *Textures[1]);
}


void ApiOpenvr::TOverlayWrapper::SubmitFrame(BufferArray<Opengl::TTexture*, 2>& Textures)
{
	auto& Overlay = *mOverlay;
	Overlay.SubmitFrame(*Textures[0]);
}


bool Openvr::TDeviceStates::HasKeyframe()
{
	for ( auto i=0;	i<mDevices.GetSize();	i++ )
	{
		auto& Device = mDevices[i];
		if ( Device.mIsKeyFrame )
			return true;
	}
	return false;
}

void SetPoseObject(Bind::TObject& Object,Openvr::TDeviceState& Pose)
{
	//	this needs to be faster really, v8 exposes that constructing a JSON string
	//	and evaluting that is faster, and this is probably a good candidate!
	static const std::string IsValidPose("IsValidPose");
	static const std::string IsConnected("IsConnected");
	static const std::string DeviceToAbsoluteTracking("DeviceToAbsoluteTracking");
	static const std::string Velocity("Velocity");
	static const std::string AngularVelocity("AngularVelocity");
	static const std::string TrackingState("TrackingState");
	static const std::string Class("Class");
	static const std::string Name("Name");
	static const std::string DeviceIndex("DeviceIndex");

	Object.SetBool(IsValidPose, Pose.mPose.bPoseIsValid);
	Object.SetBool(IsConnected, Pose.mPose.bDeviceIsConnected);
	
	auto DeviceToAbsoluteTrackingMatrix = GetRemoteArray( &Pose.mPose.mDeviceToAbsoluteTracking.m[0][0], 3 * 4);
	Object.SetArray(DeviceToAbsoluteTracking, GetArrayBridge(DeviceToAbsoluteTrackingMatrix) );
	
	auto VelocityArray = GetRemoteArray(Pose.mPose.vVelocity.v);
	Object.SetArray(Velocity, GetArrayBridge(VelocityArray) );
	
	auto AngularVelocityArray = GetRemoteArray(Pose.mPose.vAngularVelocity.v);
	Object.SetArray(AngularVelocity, GetArrayBridge(AngularVelocityArray) );
	
	//auto TrackingStateValue = std::string(magic_enum::enum_name<vr::ETrackingResult>(Pose.mPose.eTrackingResult));
	//Object.SetString(TrackingState, TrackingStateValue);
	
	Object.SetString(Class, Pose.mClassName);
	Object.SetString(Name, Pose.mTrackedName);

	Object.SetInt(DeviceIndex, Pose.mDeviceIndex);

};

Openvr::TDeviceStates ApiOpenvr::TAppWrapper::PopPose()
{
	if ( mPoses.IsEmpty() )
		throw Soy::AssertException("PopPose() with none in the queue");
								   
	std::lock_guard<std::mutex> Lock(mPosesLock);
	
	//	discard non-keyframe states if we've accumulated more poses
	//	than we've processed
	while ( mPoses.GetSize() > 1 )
	{
		auto& Pose0 = mPoses[0];
		//	keep keyframe and let that be popped next
		if ( Pose0.HasKeyframe() )
			break;
		
		mPoses.RemoveBlock(0,1);
	}

	//	pop next pose
	auto Pose0 = mPoses[0];
	mPoses.RemoveBlock(0,1);
	return Pose0;
}

void ApiOpenvr::TAppWrapper::FlushPendingPoses()
{
	//	either no data, or no-one waiting yet
	if (!mOnPosePromises.HasPromises())
		return;
	if (mPoses.IsEmpty() )
		return;
	
	auto Flush = [this](Bind::TLocalContext& Context)
	{
		//	gr: not sure how we're getting here, something is flushing out poses?
		if (mPoses.IsEmpty())
			return;

		//	we want this really fast
		Soy::TScopeTimerPrint Timer("Flush poses",3);
		auto Poses = PopPose();
		auto Object = Context.mGlobalContext.CreateObjectInstance(Context);

		//	64bit issue, so make time a string for now
		//Object.SetString("TimeMs", std::to_string(Poses.mTime.GetTime()) );
		
		BufferArray<Bind::TObject, 64> PoseObjects;
		auto EnumPoseObject = [&](Openvr::TDeviceState& Pose)
		{
			if (!Pose.mPose.bDeviceIsConnected)
				return true;
			auto PoseObject = Context.mGlobalContext.CreateObjectInstance(Context);
			SetPoseObject(PoseObject, Pose);
			PoseObjects.PushBack(PoseObject);
			return true;
		};
		GetArrayBridge(Poses.mDevices).ForEach(EnumPoseObject);
		Object.SetArray("Devices",GetArrayBridge(PoseObjects) );

		auto HandlePromise = [&](Bind::TLocalContext& LocalContext, Bind::TPromise& Promise)
		{
			Promise.Resolve(LocalContext, Object);
		};
		mOnPosePromises.Flush(HandlePromise);
	};
	auto& Context = mOnPosePromises.GetContext();
	Context.Queue(Flush);
}

void ApiOpenvr::TAppWrapper::OnNewPoses(ArrayBridge<Openvr::TDeviceState>&& Poses)
{
	{
		Openvr::TDeviceStates States;
		States.mDevices.Copy( Poses );
		States.mTime = SoyTime(true);
		std::lock_guard<std::mutex> Lock(mPosesLock);
		mPoses.PushBack(States);
	}

	FlushPendingPoses();	
}



void ApiOpenvr::THmdWrapper::GetEyeMatrix(Bind::TCallback& Params)
{
	auto& This = Params.This<ApiOpenvr::THmdWrapper>();
	auto EyeName = Params.GetArgumentString(0);

	auto EyeMatrix = This.mHmd->GetEyeMatrix(EyeName);

	auto Obj = Params.mContext.CreateObjectInstance(Params.mLocalContext);
	//Obj.SetArray("
	Params.Return(Obj);
}




std::ostream& operator<<(std::ostream &out,const vr::EVRInitError& in)
{
	auto* ErrorString = vr::VR_GetVRInitErrorAsEnglishDescription(in);
	if ( !ErrorString )
	{
		out << "<vr::EVRInitError=" << static_cast<int>(in) << ">";
		return out;
	}
	
	out << ErrorString;
	return out;
}


std::ostream& operator<<(std::ostream &out,const vr::EVRCompositorError& in)
{
	switch(in)
	{
		case vr::VRCompositorError_None:					out << "VRCompositorError_None";	break;
		case vr::VRCompositorError_RequestFailed:			out << "VRCompositorError_RequestFailed";	break;
		case vr::VRCompositorError_IncompatibleVersion:		out << "VRCompositorError_IncompatibleVersion";	break;
		case vr::VRCompositorError_DoNotHaveFocus:			out << "VRCompositorError_DoNotHaveFocus";	break;
		case vr::VRCompositorError_InvalidTexture:			out << "VRCompositorError_InvalidTexture";	break;
		case vr::VRCompositorError_IsNotSceneApplication:	out << "VRCompositorError_IsNotSceneApplication";	break;
		case vr::VRCompositorError_TextureIsOnWrongDevice:	out << "VRCompositorError_TextureIsOnWrongDevice";	break;
		case vr::VRCompositorError_TextureUsesUnsupportedFormat:	out << "VRCompositorError_TextureUsesUnsupportedFormat";	break;
		case vr::VRCompositorError_SharedTexturesNotSupported:	out << "VRCompositorError_SharedTexturesNotSupported";	break;
		case vr::VRCompositorError_IndexOutOfRange:			out << "VRCompositorError_IndexOutOfRange";	break;
		case vr::VRCompositorError_AlreadySubmitted:		out << "VRCompositorError_AlreadySubmitted";	break;
		case vr::VRCompositorError_InvalidBounds:			out << "VRCompositorError_InvalidBounds";	break;
		default:
			out << "VRCompositorError_" << static_cast<int>(in);
			break;
	}
	return out;
}


void Openvr::IsOkay(vr::ETrackedPropertyError Error, const std::string& Context)
{
	if (Error == vr::TrackedProp_Success)
		return;

	std::stringstream Exception;
	Exception << Context << " " << magic_enum::enum_name<vr::ETrackedPropertyError>(Error);
	throw Soy::AssertException(Exception);
}

void Openvr::IsOkay(vr::EVRInitError Error,const std::string& Context)
{
	if ( Error == vr::VRInitError_None )
		return;

	std::stringstream Exception;
	Exception << Context << " openvr error: " << Error;
	throw Soy::AssertException(Exception);
}

void Openvr::IsOkay(vr::EVRCompositorError Error,const std::string& Context)
{
	if ( Error == vr::VRCompositorError_None )
		return;
	
	std::stringstream Exception;
	Exception << Context << " openvr error: " << Error;
	throw Soy::AssertException(Exception);
}

void Openvr::IsOkay(vr::EVROverlayError Error, const std::string& Context)
{
	if (Error == vr::VROverlayError_None)
		return;

	std::stringstream Exception;
	Exception << Context << " openvr error: " << Error;
	throw Soy::AssertException(Exception);
}


Openvr::THmd::THmd(bool OverlayApp) :
	SoyThread	( "Openvr::THmd" )
{
#if defined(TARGET_OSX)
	throw Soy::AssertException("No openvr on osx... can't get library to sign atm");
#endif
	vr::EVRInitError Error = vr::VRInitError_None;
	auto AppType = OverlayApp ? vr::VRApplication_Overlay : vr::VRApplication_Scene;
	mHmd = vr::VR_Init( &Error, AppType );
	IsOkay( Error, "VR_Init" );
	
	Start();
	//m_strDriver = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String );
	//m_strDisplay = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String );
}

Openvr::THmd::~THmd()
{
	Stop(true);
	
	if ( mHmd )
	{
		vr::VR_Shutdown();
		mHmd = nullptr;
	}
}


void Openvr::THmd::Thread()
{
	//	loop
	while ( IsThreadRunning() )
	{
		try
		{
			//	this blocks for us
			WaitForFrameStart();
		}
		catch (std::exception& e)
		{
			std::Debug << "Openvr thread exception" << e.what() << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		}
	}
}


void Openvr::THmd::EnumEyes(std::function<void(const TEyeMatrix& Eye)>& Enum)
{
	//auto& Hmd = *mHmd;
	
	auto Left = GetEyeMatrix(EyeName_Left);
	Enum(Left);
	auto Right = GetEyeMatrix(EyeName_Right);
	Enum(Right);
}

vr::Hmd_Eye GetHmdEye(const std::string& EyeName)
{
	if ( EyeName == Openvr::EyeName_Left )
		return vr::Hmd_Eye::Eye_Left;
	if ( EyeName == Openvr::EyeName_Right )
		return vr::Hmd_Eye::Eye_Right;

	std::stringstream Error;
	Error << "Unknown eye name " << EyeName;
	throw Soy::AssertException(Error);
}

TEyeMatrix Openvr::THmd::GetEyeMatrix(const std::string& EyeName)
{
	auto& Hmd = *mHmd;
	auto Eye = GetHmdEye(EyeName);

	TEyeMatrix EyeMatrix;
	EyeMatrix.mName = EyeName;

	//	demo for opengl transposes this, maybe
	EyeMatrix.mProjection = Hmd.GetProjectionMatrix( Eye, mNearClip, mFarClip );

	//	3x4 matrix eye pose
	//vr::HmdMatrix34_t
	//auto EyeToHeadMatrix = Hmd.GetEyeToHeadTransform( Eye );
	//EyeMatrix.mPose = EyeToHeadMatrix.invert();

	Hmd.GetRecommendedRenderTargetSize( &EyeMatrix.mRenderTargetWidth, &EyeMatrix.mRenderTargetHeight );

	return EyeMatrix;
}


void Openvr::THmd::WaitForFrameStart()
{
	auto& Compositor = *vr::VRCompositor();
	vr::TrackedDevicePose_t TrackedDevicePoses[vr::k_unMaxTrackedDeviceCount];

	//	game poses are in the future (I think)
	vr::TrackedDevicePose_t* GamePoseArray = nullptr;
	size_t GamePoseArraySize = 0;

	auto Error = Compositor.WaitGetPoses(TrackedDevicePoses,std::size(TrackedDevicePoses), GamePoseArray, GamePoseArraySize);
	Openvr::IsOkay(Error, "WaitGetPoses");

	auto& System = *mHmd;

	auto GetString = [&](uint32_t DeviceIndex, vr::ETrackedDeviceProperty Property)
	{
		char Buffer[256] = { 0 };
		vr::ETrackedPropertyError Error = vr::TrackedProp_Success;
		try
		{
			auto StringLength = System.GetStringTrackedDeviceProperty(DeviceIndex, Property, Buffer, std::size(Buffer), &Error);
			Openvr::IsOkay(Error, "GetStringTrackedDeviceProperty");
			return std::string(Buffer, StringLength);
		}
		catch (std::exception& e)
		{
			return std::string(e.what());
		}
	};

	BufferArray<Openvr::TDeviceState, vr::k_unMaxTrackedDeviceCount> Devices;
	for (auto i = 0; i < Devices.MaxSize(); i++)
	{
		auto& Device = Devices.PushBack();
		Device.mDeviceIndex = i;
		Device.mPose = TrackedDevicePoses[i];
		
		//	is there a better unique identifier so we can cache this?
		auto Type = System.GetTrackedDeviceClass(Device.mDeviceIndex);
		Device.mClassName = magic_enum::enum_name<vr::ETrackedDeviceClass>(Type);
		
		Device.mTrackedName = GetString(Device.mDeviceIndex, vr::Prop_TrackingSystemName_String);
	}

	//	gr: add eye's to devices using GetEyeMatrix() so each eye has it's own device & extra properties

	mOnNewPoses(GetArrayBridge(Devices));
}

void Openvr::THmd::SubmitFrame(Opengl::TTexture& Left, Opengl::TTexture& Right)
{
	auto& Compositor = *vr::VRCompositor();

	SubmitEyeFrame(vr::Hmd_Eye::Eye_Left, Left.mTexture);
	SubmitEyeFrame(vr::Hmd_Eye::Eye_Right, Right.mTexture);
}


void Openvr::THmd::SubmitEyeFrame(vr::Hmd_Eye Eye,Opengl::TAsset Texture)
{
	auto& Compositor = *vr::VRCompositor();

	vr::Texture_t EyeTexture;
	EyeTexture.handle = reinterpret_cast<void*>( static_cast<uintptr_t>(Texture.mName) );
	EyeTexture.eType = vr::TextureType_OpenGL;
	EyeTexture.eColorSpace = vr::ColorSpace_Auto;

	auto Error = Compositor.Submit( Eye, &EyeTexture );
	if ( Error == vr::VRCompositorError_TextureUsesUnsupportedFormat )
	{
		std::stringstream ErrorString;
		//	gr: this meta was useful!
		//ErrorString << "Eye Texture Submit (" << Texture.mMeta << ")";
		ErrorString << "Eye Texture Submit (" << Texture.mName << ")";
		Openvr::IsOkay(Error, ErrorString.str());
	}
	if ( Error == vr::VRCompositorError_DoNotHaveFocus )
	{
		//	gr: this could cause a deadlock?
		WaitForFrameStart();
	
		//	submit again
		Error = Compositor.Submit( Eye, &EyeTexture );
		Openvr::IsOkay(Error, "Eye Texture Submit B");
	}
	Openvr::IsOkay( Error, "Eye Texture Submit A");

}

/*
 
 //-----------------------------------------------------------------------------
 // Purpose:
 //-----------------------------------------------------------------------------
 void CMainApplication::UpdateHMDMatrixPose()
 {
 if ( !m_pHMD )
 return;
 
 vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0 );
 
 m_iValidPoseCount = 0;
 m_strPoseClasses = "";
 for ( int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice )
 {
 if ( m_rTrackedDevicePose[nDevice].bPoseIsValid )
 {
 m_iValidPoseCount++;
 m_rmat4DevicePose[nDevice] = ConvertSteamVRMatrixToMatrix4( m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking );
 if (m_rDevClassChar[nDevice]==0)
 {
 switch (m_pHMD->GetTrackedDeviceClass(nDevice))
 {
 case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
 case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
 case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
 case vr::TrackedDeviceClass_GenericTracker:    m_rDevClassChar[nDevice] = 'G'; break;
 case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
 default:                                       m_rDevClassChar[nDevice] = '?'; break;
 }
 }
 m_strPoseClasses += m_rDevClassChar[nDevice];
 }
 }
 
 if ( m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
 {
 m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
 m_mat4HMDPose.invert();
 }
 }

 */

void ApiOpenvr::TSkeletonWrapper::Construct(Bind::TCallback& Params)
{
	mHandTracker.reset(new Vive::THandTracker());

	mHandTracker->mOnNewGesture = std::bind(&TSkeletonWrapper::OnNewGesture, this);
}


void ApiOpenvr::TSkeletonWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::GetNextFrame>(&TSkeletonWrapper::GetNextFrame);
}

void ApiOpenvr::TSkeletonWrapper::OnNewGesture()
{
	//	flush promises
	if (!mNextFramePromiseQueue.HasPromises())
		return;

	//	grab frame
	Vive::THandPose LeftHand, RightHand;
	bool GetLatest = true;
	if (!mHandTracker->PopGesture(LeftHand, RightHand,GetLatest))
		return;

	auto Resolve = [=](Bind::TLocalContext& Context,Bind::TPromise& Promise)
	{
		auto ResultObject = Context.mGlobalContext.CreateObjectInstance(Context);
		
		auto SetJoint = [&](const vec3f& Position,const std::string& JointName)
		{
			auto PositionArray = Position.GetArray();
			ResultObject.SetArray(JointName, GetArrayBridge(PositionArray));
		};

		LeftHand.EnumJoints(SetJoint, LeftHandJointPrefix );
		RightHand.EnumJoints(SetJoint,RightHandJointPrefix);

		Promise.Resolve(Context, ResultObject);
	};

	mNextFramePromiseQueue.Flush(Resolve);
}

void ApiOpenvr::TSkeletonWrapper::GetNextFrame(Bind::TCallback& Params)
{
	auto Promise = mNextFramePromiseQueue.AddPromise(Params.mLocalContext);
	Params.Return(Promise);

	//	flush any current gestures
	OnNewGesture();
}


void ApiOpenvr::TOverlayWrapper::Construct(Bind::TCallback& Params)
{
	auto Name = Params.GetArgumentString(0);
	mOverlay.reset(new Openvr::TOverlay(Name));
}



Openvr::TOverlay::TOverlay(const std::string& Name) :
	SoyWorkerThread	("Openvr::Overlay",SoyWorkerWaitMode::Sleep)
{
#if defined(TARGET_OSX)
	throw Soy::AssertException("No openvr on osx... can't get library to sign atm");
#endif
	vr::EVRInitError Error = vr::VRInitError_None;
	mSystem = vr::VR_Init(&Error, vr::VRApplication_Overlay);
	IsOkay(Error, "VR_Init");

	mOverlay = vr::VROverlay();
	if ( !mOverlay )
		throw Soy::AssertException("Failed to allocate overlay");

	auto OError = mOverlay->CreateDashboardOverlay(Name.c_str(), Name.c_str(), &mHandle, &mThumbnailHandle );
	IsOkay(OError, "CreateDashboardOverlay");

	const float Width = 1.5;
	mOverlay->SetOverlayWidthInMeters(mHandle, Width);
	mOverlay->SetOverlayInputMethod(mHandle, vr::VROverlayInputMethod_Mouse);

	mOverlay->ShowOverlay(mHandle);

	Start();
}

Openvr::TOverlay::~TOverlay()
{
	Stop();

	if (mOverlay)
	{
		vr::VR_Shutdown();
		mOverlay = nullptr;
	}
}


void Openvr::TOverlay::HandleEvent(vr::VREvent_t& Event)
{
	std::Debug << "Event " << Event.eventType << std::endl;
	/*
	switch (vrEvent.eventType)
	{
	case vr::VREvent_MouseMove:
	{
		QPointF ptNewMouse(vrEvent.data.mouse.x, vrEvent.data.mouse.y);
		QPoint ptGlobal = ptNewMouse.toPoint();
		QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMouseMove);
		mouseEvent.setWidget(NULL);
		mouseEvent.setPos(ptNewMouse);
		mouseEvent.setScenePos(ptGlobal);
		mouseEvent.setScreenPos(ptGlobal);
		mouseEvent.setLastPos(m_ptLastMouse);
		mouseEvent.setLastScenePos(m_pWidget->mapToGlobal(m_ptLastMouse.toPoint()));
		mouseEvent.setLastScreenPos(m_pWidget->mapToGlobal(m_ptLastMouse.toPoint()));
		mouseEvent.setButtons(m_lastMouseButtons);
		mouseEvent.setButton(Qt::NoButton);
		mouseEvent.setModifiers(0);
		mouseEvent.setAccepted(false);

		m_ptLastMouse = ptNewMouse;
		QApplication::sendEvent(m_pScene, &mouseEvent);

		OnSceneChanged(QList<QRectF>());
	}
	break;

	case vr::VREvent_MouseButtonDown:
	{
		Qt::MouseButton button = vrEvent.data.mouse.button == vr::VRMouseButton_Right ? Qt::RightButton : Qt::LeftButton;

		m_lastMouseButtons |= button;

		QPoint ptGlobal = m_ptLastMouse.toPoint();
		QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMousePress);
		mouseEvent.setWidget(NULL);
		mouseEvent.setPos(m_ptLastMouse);
		mouseEvent.setButtonDownPos(button, m_ptLastMouse);
		mouseEvent.setButtonDownScenePos(button, ptGlobal);
		mouseEvent.setButtonDownScreenPos(button, ptGlobal);
		mouseEvent.setScenePos(ptGlobal);
		mouseEvent.setScreenPos(ptGlobal);
		mouseEvent.setLastPos(m_ptLastMouse);
		mouseEvent.setLastScenePos(ptGlobal);
		mouseEvent.setLastScreenPos(ptGlobal);
		mouseEvent.setButtons(m_lastMouseButtons);
		mouseEvent.setButton(button);
		mouseEvent.setModifiers(0);
		mouseEvent.setAccepted(false);

		QApplication::sendEvent(m_pScene, &mouseEvent);
	}
	break;

	case vr::VREvent_MouseButtonUp:
	{
		Qt::MouseButton button = vrEvent.data.mouse.button == vr::VRMouseButton_Right ? Qt::RightButton : Qt::LeftButton;
		m_lastMouseButtons &= ~button;

		QPoint ptGlobal = m_ptLastMouse.toPoint();
		QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMouseRelease);
		mouseEvent.setWidget(NULL);
		mouseEvent.setPos(m_ptLastMouse);
		mouseEvent.setScenePos(ptGlobal);
		mouseEvent.setScreenPos(ptGlobal);
		mouseEvent.setLastPos(m_ptLastMouse);
		mouseEvent.setLastScenePos(ptGlobal);
		mouseEvent.setLastScreenPos(ptGlobal);
		mouseEvent.setButtons(m_lastMouseButtons);
		mouseEvent.setButton(button);
		mouseEvent.setModifiers(0);
		mouseEvent.setAccepted(false);

		QApplication::sendEvent(m_pScene, &mouseEvent);
	}
	break;

	case vr::VREvent_OverlayShown:
	{
		m_pWidget->repaint();
	}
	break;

	case vr::VREvent_Quit:
		QApplication::exit();
		break;
	}
}
switch (vrEvent.eventType)
{
case vr::VREvent_OverlayShown:
{
	m_pWidget->repaint();
}
break;
}
			}
			*/
}


bool Openvr::TOverlay::Iteration()
{
	auto& Overlay = *mOverlay;
	
	vr::VREvent_t Event;
	while (Overlay.PollNextOverlayEvent(mHandle, &Event, sizeof(Event)))
	{
		HandleEvent(Event);
	}

	//	update thumbnail events
	if (mThumbnailHandle != vr::k_ulOverlayHandleInvalid)
	{
		while (Overlay.PollNextOverlayEvent(mThumbnailHandle, &Event, sizeof(Event)))
		{
			HandleEvent(Event);
		}
	}

	return true;
}

void Openvr::TOverlay::SubmitFrame(Opengl::TTexture& OpenglTexture)
{
	auto& Overlay = *mOverlay;

	vr::Texture_t Texture;
	Texture.handle = reinterpret_cast<void*>(static_cast<uintptr_t>(OpenglTexture.mTexture.mName));
	Texture.eType = vr::TextureType_OpenGL;
	Texture.eColorSpace = vr::ColorSpace_Auto;
	
	Overlay.SetOverlayTexture(mHandle, &Texture);
}
