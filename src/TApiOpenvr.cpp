#include "TApiOpenvr.h"
#include "Libs/OpenVr/headers/openvr.h"
#include "TApiCommon.h"
#if defined(ENABLE_OPENGL)
#include "TApiOpengl.h"
#include "SoyOpengl.h"
#endif
#include "SoyViveHandTracker.h"
#include <magic_enum.hpp>
#include "SoyRuntimeLibrary.h"
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

	DEFINE_BIND_FUNCTIONNAME(SetTransform);
	DEFINE_BIND_FUNCTIONNAME(WaitForMirrorImage);

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

	class TApp;
	class TDeviceState;

	vr::HmdMatrix34_t	MultiplyMatrix(const vr::HmdMatrix34_t& a, const vr::HmdMatrix34_t& b);
	float4x4			GetMatrix(const vr::HmdMatrix44_t& a);
}

namespace ViveHandTracker
{
#include "Libs/ViveHandTracking/include/interface_gesture.hpp"
}



class Openvr::TApp
{
public:
	~TApp();

protected:
	vr::IVRCompositor&	GetCompositor() {	return *mCompositor;	}
	vr::IVRSystem&	GetSystem() {	return *mSystem;	}
	void			GetPoses(ArrayBridge<Openvr::TDeviceState>&& States, bool Blocking);
	void			HandleEvent(vr::VREvent_t& Event);

	virtual void	GetSubDevices(const Openvr::TDeviceState& ParentDevice, vr::ETrackedDeviceClass ParentDeviceType, ArrayBridge<Openvr::TDeviceState>&& SubDevices) {}

public:
	std::function<void(ArrayBridge<TDeviceState>&&)>	mOnNewPoses;

	vr::IVRCompositor*	mCompositor = nullptr;
	vr::glUInt_t		mMirrorTexture = 0;
	vr::glSharedTextureHandle_t	mMirrorTextureHandle = nullptr;

protected:
	vr::IVRSystem*		mSystem = nullptr;
};

class Openvr::THmd : public SoyThread, public TApp
{
public:
	THmd(bool OverlayApp);
	~THmd();

	virtual bool	ThreadIteration() override;

	void			SubmitFrame(Opengl::TTexture& Left, Opengl::TTexture& Right);

protected:
	virtual void	GetSubDevices(const Openvr::TDeviceState& ParentDevice, vr::ETrackedDeviceClass ParentDeviceType, ArrayBridge<Openvr::TDeviceState>&& SubDevices) override;

private:
	void			SubmitEyeFrame(vr::Hmd_Eye Eye,Opengl::TAsset Texture);
	void			WaitForFrameStart();

public:
	std::function<void(ArrayBridge<TDeviceState>&&)>	mOnNewPoses;
	float			mNearClip = 0.1f;
	float			mFarClip = 40.0f;
};




class Openvr::TOverlay : public SoyWorkerThread, public TApp
{
public:
	TOverlay(const std::string& Name);
	~TOverlay();

	virtual bool	Iteration() override;
	void			SubmitFrame(Opengl::TTexture& Image);
	void			SetTransform(vr::HmdMatrix34_t& Transform);

protected:
	void			UpdatePoses();
	TDeviceState	GetOverlayWindowPose();

public:
	vr::VROverlayHandle_t	mHandle = vr::k_ulOverlayHandleInvalid;
	vr::VROverlayHandle_t	mThumbnailHandle = vr::k_ulOverlayHandleInvalid;
	vr::IVROverlay*			mOverlay = nullptr;
};


vr::HmdMatrix34_t Openvr::MultiplyMatrix(const vr::HmdMatrix34_t& a, const vr::HmdMatrix34_t& b)
{
	auto get44 = [](const vr::HmdMatrix34_t& x34)
	{
		vr::HmdMatrix44_t x44;
		auto& X34 = x34.m;
		auto& X44 = x44.m;
		std::copy(X34[0], X34[0] + 4, X44[0]);
		std::copy(X34[1], X34[1] + 4, X44[1]);
		std::copy(X34[2], X34[2] + 4, X44[2]);
		X44[3][0] = 0;
		X44[3][1] = 0;
		X44[3][2] = 0;
		X44[3][3] = 1;
		return x44;
	};
	auto get34 = [](const vr::HmdMatrix44_t& x44)
	{
		vr::HmdMatrix34_t x34;
		auto& X34 = x34.m;
		auto& X44 = x44.m;
		std::copy(X44[0], X44[0] + 4, X34[0]);
		std::copy(X44[1], X44[1] + 4, X34[1]);
		std::copy(X44[2], X44[2] + 4, X34[2]);
		return x34;
	};
	vr::HmdMatrix44_t C;
	vr::HmdMatrix44_t A = get44(a);
	vr::HmdMatrix44_t B = get44(b);

	auto Cols = 4;
	auto Rows = 4;

	for (int i = 0; i < Cols; i++)
	{
		for (int j = 0; j <Rows; j++) 
		{
			float num = 0;
			for (int k = 0; k <Cols; k++) 
			{
				num += a.m[i][k] * a.m[k][j];
			}
			C.m[i][j] = num;
		}
	}
	auto c = get34(C);
	return c;
}

float4x4 Openvr::GetMatrix(const vr::HmdMatrix44_t& a)
{
	float4x4 b;
	std::copy(a.m[0], a.m[0] + 4, &b.rows[0].x);
	std::copy(a.m[1], a.m[1] + 4, &b.rows[1].x);
	std::copy(a.m[2], a.m[2] + 4, &b.rows[2].x);
	std::copy(a.m[3], a.m[3] + 4, &b.rows[3].x);
	return b;
}


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
	Template.BindFunction<BindFunction::SetTransform>(&TOverlayWrapper::SetTransform);
	Template.BindFunction<BindFunction::WaitForMirrorImage>(&TOverlayWrapper::WaitForMirrorImage);
}

Openvr::TApp& ApiOpenvr::TOverlayWrapper::GetApp()
{
	return *mOverlay;
}

Openvr::TApp& ApiOpenvr::THmdWrapper::GetApp()
{
	return *mHmd;
}

void ApiOpenvr::TAppWrapper::UpdateMirrorTexture(Opengl::TContext& Context,TImageWrapper& MirrorImage,bool ReadBackPixels)
{
	//	setup texture from openvr & assign to image
	auto& Overlay = GetApp();
	auto& MirrorTextureName = Overlay.mMirrorTexture;
	auto& MirrorTextureHandle = Overlay.mMirrorTextureHandle;
	//	fetching this texture needs to be done from opengl thread
	if (MirrorTextureName == 0 || !MirrorTextureHandle)
	{
		auto CError = Overlay.mCompositor->GetMirrorTextureGL(vr::Eye_Left, &MirrorTextureName, &MirrorTextureHandle);
		Openvr::IsOkay(CError, "GetMirrorTextureGL");

		//	this makes the texture update!
		Overlay.mCompositor->LockGLSharedTextureForAccess(MirrorTextureHandle);

		Opengl::TAsset TextureName(MirrorTextureName);
		MirrorImage.SetOpenglTexture(TextureName);
	}

	//	copy it to our own texture?

	//	read back pixels
	MirrorImage.OnOpenglTextureChanged(Context);
	if (ReadBackPixels)
	{
		MirrorImage.ReadOpenglPixels(SoyPixelsFormat::RGBA);
	}

	//	update state for promises
	this->mMirrorChanged = true;
	this->SetMirrorError(nullptr);
	FlushPendingMirrors();
}

void ApiOpenvr::TAppWrapper::SetMirrorError(const char* Error)
{
	if (!Error)
	{
		mMirrorError = std::string();
		FlushPendingMirrors();
		return;
	}

	mMirrorError = Error;
	FlushPendingMirrors();
}

void ApiOpenvr::TAppWrapper::WaitForMirrorImage(Bind::TCallback& Params)
{
	//	setup a job to fetch & update texture & pixels
	//	get render context
	auto& RenderContext = Params.GetArgumentPointer<TWindowWrapper>(0);
	bool ReadPixels = false;
	if (!Params.IsArgumentUndefined(1))
		ReadPixels = Params.GetArgumentBool(1);

	//	setup image object if we don't have one whilst on JS thread
	if (!mMirrorImage)
	{
		auto MirrorImage = Params.mLocalContext.mGlobalContext.CreateObjectInstance(Params.mLocalContext, TImageWrapper::GetTypeName());
		mMirrorImage = Bind::TPersistent(Params.mLocalContext, MirrorImage, "MirrorImage");
	}
	//	gr: we're passing the raw image to the opengl thread
	//		I think if we passed the persistent object, we should be able to make sure it's not deallocated?
	//		maybe we should have a queue of objects to release in the js context which all get unprotected in
	//		next thread iteration and can be added to from any thread
	auto MirrorImageObject = mMirrorImage.GetObject(Params.mLocalContext);
	auto& MirrorImage = MirrorImageObject.This<TImageWrapper>();


	auto OpenglContext = RenderContext.GetOpenglContext();
	auto UpdateTextureJob = [=,&MirrorImage]()
	{
		if (!OpenglContext->IsLockedToThisThread())
			throw Soy::AssertException("Function not being called on opengl thread");
		try
		{
			this->UpdateMirrorTexture(*OpenglContext, MirrorImage, ReadPixels);
		}
		catch (std::exception& e)
		{
			this->SetMirrorError(e.what());
		}
	};
	OpenglContext->PushJob(UpdateTextureJob);

	auto NewPromise = mOnMirrorPromises.AddPromise(Params.mLocalContext);
	Params.Return(NewPromise);

	FlushPendingMirrors();
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

BufferArray<float, 4 * 4> Transpose43To44(const vr::HmdMatrix34_t& Matrix43)
{
	auto* m = &Matrix43.m[0][0];

	float m44[] =
	{
		m[0],m[4],m[8],0,
		m[1],m[5],m[9],0,
		m[2],m[6],m[10],0,
		m[3],m[7],m[11],1,
	};
	return BufferArray<float,16>(m44);
}

BufferArray<float,4*4> Transpose44(const vr::HmdMatrix44_t& Matrix44)
{
	auto* m = &Matrix44.m[0][0];

	float m44[] =
	{
		m[0],m[4],m[8],m[12],
		m[1],m[5],m[9],m[13],
		m[2],m[6],m[10],m[14],
		m[3],m[7],m[11],m[15],
	};
	return BufferArray<float, 16>(m44);
}


void SetPoseObject(Bind::TObject& Object,Openvr::TDeviceState& Pose)
{
	//	this needs to be faster really, v8 exposes that constructing a JSON string
	//	and evaluting that is faster, and this is probably a good candidate!
	static const std::string IsValidPose("IsValidPose");
	static const std::string IsConnected("IsConnected");
	static const std::string DeviceToAbsoluteTracking("DeviceToAbsoluteTracking");
	static const std::string LocalToWorld("LocalToWorld");
	static const std::string Velocity("Velocity");
	static const std::string AngularVelocity("AngularVelocity");
	static const std::string TrackingState("TrackingState");
	static const std::string Class("Class");
	static const std::string Name("Name");
	static const std::string DeviceIndex("DeviceIndex");
	static const std::string LocalBounds("LocalBounds");
	static const std::string ProjectionMatrix("ProjectionMatrix");

	Object.SetBool(IsValidPose, Pose.mPose.bPoseIsValid);
	Object.SetBool(IsConnected, Pose.mPose.bDeviceIsConnected);
	
	auto LocalToWorldMatrix = Transpose43To44(Pose.mPose.mDeviceToAbsoluteTracking);
	Object.SetArray(LocalToWorld, GetArrayBridge(LocalToWorldMatrix) );
	//auto DeviceToAbsoluteTrackingMatrix = GetRemoteArray( &Pose.mPose.mDeviceToAbsoluteTracking.m[0][0], 3 * 4);
	//Object.SetArray(DeviceToAbsoluteTracking, GetArrayBridge(DeviceToAbsoluteTrackingMatrix) );
	

	auto VelocityArray = GetRemoteArray(Pose.mPose.vVelocity.v);
	Object.SetArray(Velocity, GetArrayBridge(VelocityArray) );
	
	auto AngularVelocityArray = GetRemoteArray(Pose.mPose.vAngularVelocity.v);
	Object.SetArray(AngularVelocity, GetArrayBridge(AngularVelocityArray) );
	
	//auto TrackingStateValue = std::string(magic_enum::enum_name<vr::ETrackingResult>(Pose.mPose.eTrackingResult));
	//Object.SetString(TrackingState, TrackingStateValue);
	
	Object.SetString(Class, Pose.mClassName);
	Object.SetString(Name, Pose.mTrackedName);

	Object.SetInt(DeviceIndex, Pose.mDeviceIndex);

	if (Pose.HasLocalBounds())
	{
		BufferArray<float, 6> LocalBoundsFloats;
		LocalBoundsFloats.PushBack(Pose.mLocalBounds.min.x);
		LocalBoundsFloats.PushBack(Pose.mLocalBounds.min.y);
		LocalBoundsFloats.PushBack(Pose.mLocalBounds.min.z);
		LocalBoundsFloats.PushBack(Pose.mLocalBounds.max.x);
		LocalBoundsFloats.PushBack(Pose.mLocalBounds.max.y);
		LocalBoundsFloats.PushBack(Pose.mLocalBounds.max.z);
		Object.SetArray(LocalBounds, GetArrayBridge(LocalBoundsFloats));
	}

	if (Pose.HasProjectionMatrix())
	{
		auto Array = Pose.mProjectionMatrix.GetArray();
		Object.SetArray(ProjectionMatrix, GetArrayBridge(Array));
	}
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


void ApiOpenvr::TAppWrapper::FlushPendingMirrors()
{
	//	either no data, or no-one waiting yet
	if (!mOnMirrorPromises.HasPromises())
		return;
	if (!mMirrorChanged)
		return;

	auto Flush = [this](Bind::TLocalContext& Context)
	{
		auto HandlePromise = [&](Bind::TLocalContext& LocalContext, Bind::TPromise& Promise)	
		{
			auto MirrorImageObject = this->mMirrorImage.GetObject(LocalContext);
			Promise.Resolve(LocalContext, MirrorImageObject );
		};
		mOnMirrorPromises.Flush(HandlePromise);
	};
	auto& Context = mOnMirrorPromises.GetContext();
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



std::ostream& operator<<(std::ostream &out,const vr::EVRInitError& in)
{
	auto* ErrorString = vr::VR_GetVRInitErrorAsEnglishDescription(in);
	if (ErrorString)
	{
		out << ErrorString;
		return out;
	}
	
	out << magic_enum::enum_name(in);
	return out;
}


std::ostream& operator<<(std::ostream &out, const vr::EVRCompositorError& in)
{
	out << magic_enum::enum_name(in);
	return out;
}


std::ostream& operator<<(std::ostream &out, const vr::EVROverlayError& in)
{
	out << magic_enum::enum_name(in);
	return out;
}


std::ostream& operator<<(std::ostream &out, const vr::ETrackedPropertyError& in)
{
	out << magic_enum::enum_name(in);
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



Openvr::TApp::~TApp()
{
	if (mSystem)
	{
		vr::VR_Shutdown();
		mSystem = nullptr;
}
}

Openvr::THmd::THmd(bool OverlayApp) :
	SoyThread	( "Openvr::THmd" )
{
#if defined(TARGET_OSX)
	throw Soy::AssertException("No openvr on osx... can't get library to sign atm");
#endif
	Soy::TRuntimeLibrary Dll("openvr_api.dll");
	vr::EVRInitError Error = vr::VRInitError_None;
	auto AppType = OverlayApp ? vr::VRApplication_Overlay : vr::VRApplication_Scene;
	mSystem = vr::VR_Init( &Error, AppType );
	IsOkay( Error, "VR_Init" );
	
	//Start();
	//m_strDriver = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String );
	//m_strDisplay = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String );
}

Openvr::THmd::~THmd()
{
	Stop(true);
}



bool Openvr::THmd::ThreadIteration()
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
	return true;
}


void Openvr::TApp::GetPoses(ArrayBridge<Openvr::TDeviceState>&& States,bool Blocking)
{
	auto& Compositor = *vr::VRCompositor();
	const auto PoseCount = vr::k_unMaxTrackedDeviceCount;
	vr::TrackedDevicePose_t TrackedDevicePoses[PoseCount];

	//	game poses are in the future (I think)
	vr::TrackedDevicePose_t* GamePoseArray = nullptr;
	size_t GamePoseArraySize = 0;

	vr::EVRCompositorError Error;
	if (Blocking )
		Error = Compositor.WaitGetPoses(TrackedDevicePoses, PoseCount, GamePoseArray, GamePoseArraySize);
	else
		Error = Compositor.GetLastPoses(TrackedDevicePoses, PoseCount, GamePoseArray, GamePoseArraySize);
	Openvr::IsOkay(Error, Blocking ? "WaitGetPoses":"GetLastPoses");

	auto& System = GetSystem();

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


	for (auto i = 0; i < PoseCount; i++)
	{
		auto& Device = States.PushBack();
		Device.mDeviceIndex = i;
		Device.mPose = TrackedDevicePoses[i];

		//	is there a better unique identifier so we can cache this?
		auto Type = System.GetTrackedDeviceClass(Device.mDeviceIndex);
		Device.mClassName = magic_enum::enum_name<vr::ETrackedDeviceClass>(Type);

		//	tracking SYSTEM name always gives us light house
		//Device.mTrackedName = GetString(Device.mDeviceIndex, vr::Prop_TrackingSystemName_String);
		auto TrackingSystemName = GetString(Device.mDeviceIndex, vr::Prop_TrackingSystemName_String);
		auto RenderModelName = GetString(Device.mDeviceIndex, vr::Prop_RenderModelName_String);
		auto ManufacturerName = GetString(Device.mDeviceIndex, vr::Prop_ManufacturerName_String);
		Device.mTrackedName = RenderModelName;
		
		BufferArray<TDeviceState, 3> SubDevices;
		GetSubDevices(Device, Type, GetArrayBridge(SubDevices));
		States.PushBackArray(SubDevices);

	}
}

void Openvr::THmd::WaitForFrameStart()
{
	BufferArray<Openvr::TDeviceState, vr::k_unMaxTrackedDeviceCount+10> Devices;
	GetPoses(GetArrayBridge(Devices), true);
	
	mOnNewPoses(GetArrayBridge(Devices));
}

void Openvr::THmd::SubmitFrame(Opengl::TTexture& Left, Opengl::TTexture& Right)
{
	auto& Compositor = *vr::VRCompositor();
	
	{
		vr::VREvent_t event;
		while (mSystem->PollNextEvent(&event, sizeof(event)))
		{
			this->HandleEvent(event);
		}
	}
	WaitForFrameStart();

	SubmitEyeFrame(vr::Hmd_Eye::Eye_Left, Left.mTexture);
	SubmitEyeFrame(vr::Hmd_Eye::Eye_Right, Right.mTexture);

	//$ HACKHACK. From gpuview profiling, it looks like there is a bug where two renders and a present
		// happen right before and after the vsync causing all kinds of jittering issues. This glFinish()
		// appears to clear that up. Temporary fix while I try to get nvidia to investigate this problem.
		// 1/29/2014 mikesart
	glFinish();
	glFlush();
}

void Openvr::THmd::GetSubDevices(const Openvr::TDeviceState& ParentDevice, vr::ETrackedDeviceClass ParentDeviceType, ArrayBridge<Openvr::TDeviceState>&& SubDevices)
{
	//	make extra "devices" for the HMD for eyes
	if (ParentDeviceType != vr::TrackedDeviceClass_HMD)
		return;

	auto& Hmd = GetSystem();

	auto GetEyeDevice = [&](vr::Hmd_Eye Eye, const char* NameSuffix)
	{
		TDeviceState EyeDevice = ParentDevice;
		EyeDevice.mClassName += NameSuffix;
		EyeDevice.mTrackedName += NameSuffix;
		
		auto ProjectionMatrix = Hmd.GetProjectionMatrix(Eye, mNearClip, mFarClip);
		EyeDevice.mProjectionMatrix = Transpose44(ProjectionMatrix).GetArray();
		//Hmd.GetRecommendedRenderTargetSize(&EyeMatrix.mRenderTargetWidth, &EyeMatrix.mRenderTargetHeight);

		//	apply offset to local to world pose
		auto EyeToHead = Hmd.GetEyeToHeadTransform(Eye);
		auto HeadToWorld = ParentDevice.mPose.mDeviceToAbsoluteTracking;
		auto EyeToWorld = Openvr::MultiplyMatrix(EyeToHead, HeadToWorld);
		EyeDevice.mPose.mDeviceToAbsoluteTracking = EyeToWorld;
		EyeDevice.mPose.mDeviceToAbsoluteTracking = HeadToWorld;
		//	3x4 matrix eye pose
	//vr::HmdMatrix34_t
	//auto EyeToHeadMatrix = Hmd.GetEyeToHeadTransform( Eye );
	//EyeMatrix.mPose = EyeToHeadMatrix.invert();
		SubDevices.PushBack(EyeDevice);
	};

	GetEyeDevice( vr::Eye_Left, "_LeftEye");
	GetEyeDevice( vr::Eye_Right, "_RightEye");
}

void Openvr::THmd::SubmitEyeFrame(vr::Hmd_Eye Eye,Opengl::TAsset Texture)
{
	auto& Compositor = *vr::VRCompositor();

	vr::Texture_t EyeTexture;
	EyeTexture.handle = reinterpret_cast<void*>( static_cast<uintptr_t>(Texture.mName) );
	EyeTexture.eType = vr::TextureType_OpenGL;
	//EyeTexture.eColorSpace = vr::ColorSpace_Auto;
	EyeTexture.eColorSpace = vr::ColorSpace_Gamma;

	//std::Debug << "Submit texture " << Eye << " " << EyeTexture.handle << std::endl;
	auto Error = Compositor.Submit( Eye, &EyeTexture );
	//std::Debug << "error=" << Error << std::endl;
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

	mOverlay->mOnNewPoses = std::bind(&THmdWrapper::OnNewPoses, this, std::placeholders::_1);
}


void ApiOpenvr::TOverlayWrapper::SetTransform(Bind::TCallback& Params)
{
	BufferArray<float, 4 * 4> Transform;
	Params.GetArgumentArray(0, GetArrayBridge(Transform));

	auto m00 = Transform[0];
	auto m01 = Transform[1];
	auto m02 = Transform[2];
	auto m03 = Transform[3];
	auto m10 = Transform[4];
	auto m11 = Transform[5];
	auto m12 = Transform[6];
	auto m13 = Transform[7];
	auto m20 = Transform[8];
	auto m21 = Transform[9];
	auto m22 = Transform[10];
	auto m23 = Transform[11];
	auto m30 = Transform[12];
	auto m31 = Transform[13];
	auto m32 = Transform[14];
	auto m33 = Transform[15];
	//	need to homogenise the other rows too?
	m30 /= m33;
	m31 /= m33;
	m32 /= m33;

	//	transposed and reduced to xyz (no w)
	vr::HmdMatrix34_t Matrix43 =
	{
		m00, m10, m20, m30,
		m01, m11, m21, m31,
		m02, m12, m22, m32
	};

	mOverlay->SetTransform(Matrix43);
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

	mCompositor = vr::VRCompositor();
	if (!mCompositor)
		throw Soy::AssertException("Failed to allocate compositor");

	mOverlay = vr::VROverlay();
	if ( !mOverlay )
		throw Soy::AssertException("Failed to allocate overlay");

	//mOverlay->CreateOverlay
	//auto OError = mOverlay->CreateDhasboardOverlay(Name.c_str(), Name.c_str(), &mHandle, &mThumbnailHandle );
	auto OError = mOverlay->CreateOverlay(Name.c_str(), Name.c_str(), &mHandle );
	IsOkay(OError, "CreateOverlay");

	
	//	dashboard overlay has a default transform, but for a normal one we need to init
	vec3f Offset(0, 0, -2.0f);
	const float Width = 1.0f;
	vr::HmdMatrix34_t InitialTransform =
	{
		1.0f, 0.0f, 0.0f, Offset.x,
		0.0f, 1.0f, 0.0f, Offset.y,
		0.0f, 0.0f, 1.0f, Offset.z
	};
	SetTransform(InitialTransform);
	

	//	stereo left/right flag
	//	VROverlayFlags_SideBySide_Parallel

	OError = mOverlay->SetOverlayWidthInMeters(mHandle, Width);
	IsOkay(OError, "SetOverlayWidthInMeters");
	OError = mOverlay->SetOverlayInputMethod(mHandle, vr::VROverlayInputMethod_Mouse);
	IsOkay(OError, "SetOverlayInputMethod");

	OError = mOverlay->ShowOverlay(mHandle);
	IsOkay(OError, "ShowOverlay");

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


Openvr::TDeviceState Openvr::TOverlay::GetOverlayWindowPose()
{
	Openvr::TDeviceState Pose;
	Pose.mClassName = "OverlayWindow";
	Pose.mIsKeyFrame = false;
	Pose.mTrackedName = "OverlayWindow";

	float Width = 0;
	auto Error = mOverlay->GetOverlayWidthInMeters(mHandle,&Width);
	IsOkay(Error, "GetOverlayWidthInMeters");
	Pose.mLocalBounds.min.x = -Width / 2.0f;
	Pose.mLocalBounds.max.x = Width / 2.0f;
	Pose.mLocalBounds.min.y = -Width / 2.0f;
	Pose.mLocalBounds.max.y = Width / 2.0f;
	Pose.mLocalBounds.min.z = 0;
	Pose.mLocalBounds.max.z = 0.01f;


	Pose.mPose.bDeviceIsConnected = mOverlay->IsOverlayVisible(mHandle);
	Pose.mPose.bPoseIsValid = Pose.mPose.bDeviceIsConnected;

	Pose.mPose.vVelocity = vr::HmdVector3_t{ 0,0,0 };
	Pose.mPose.vAngularVelocity = vr::HmdVector3_t{ 0,0,0 };

	vr::ETrackingUniverseOrigin Origin = vr::TrackingUniverseSeated;
	Error = mOverlay->GetOverlayTransformAbsolute(mHandle, &Origin, &Pose.mPose.mDeviceToAbsoluteTracking);
	IsOkay(Error, "GetOverlayTransformAbsolute");

	return Pose;
}

void Openvr::TOverlay::UpdatePoses()
{
	if (!mOnNewPoses)
		return;

	//	get a world-space pose for the overlay window
	Array<TDeviceState> States;
	auto OverlayPose = GetOverlayWindowPose();
	States.PushBack(OverlayPose);

	GetPoses(GetArrayBridge(States), false);


	mOnNewPoses( GetArrayBridge(States));
}

void Openvr::TApp::HandleEvent(vr::VREvent_t& Event)
{
	auto EventName = GetSystem().GetEventTypeNameFromEnum( static_cast<vr::EVREventType>(Event.eventType));
	std::Debug << "Event " << EventName << std::endl;
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

	UpdatePoses();

	return true;
}

void Openvr::TOverlay::SetTransform(vr::HmdMatrix34_t& Transform)
{
	//	option to be relative to something
	/*
	OError = mOverlay->SetOverlayTransformTrackedDeviceRelative(mHandle, vr::k_unTrackedDeviceIndex_Hmd, &InitialTransform);
	IsOkay(OError, "SetOverlayTransformTrackedDeviceRelative");
	*/
	//	then, fix it in space by getting it's absolute, then setting it again (instead of relative)
	auto TransformOrigin = vr::TrackingUniverseStanding;
	auto Error = mOverlay->SetOverlayTransformAbsolute(mHandle, TransformOrigin, &Transform );
	IsOkay(Error, "SetOverlayTransformAbsolute");
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
