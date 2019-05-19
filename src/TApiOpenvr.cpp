#include "TApiOpenvr.h"
#include "Libs/OpenVr/headers/openvr.h"
#include "TApiCommon.h"
#include "SoyOpengl.h"

namespace ApiOpenvr
{
	const char Namespace[] = "Pop.Openvr";

	DEFINE_BIND_TYPENAME(Hmd);

	DEFINE_BIND_FUNCTIONNAME(SubmitEyeTexture);
	DEFINE_BIND_FUNCTIONNAME(GetEyeMatrix);
	DEFINE_BIND_FUNCTIONNAME(BeginFrame);
}

namespace Openvr
{
	const std::string	EyeName_Left = "Left";
	const std::string	EyeName_Right = "Right";

	void	IsOkay(vr::EVRInitError Error,const std::string& Context);
	void	IsOkay(vr::EVRCompositorError Error,const std::string& Context);
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



class Openvr::THmd
{
public:
	THmd();
	~THmd();
	
	void		SubmitEyeFrame(const std::string& EyeName,TImageWrapper& Image);
	void		EnumEyes(std::function<void(const TEyeMatrix& Eye)>& Enum);
	TEyeMatrix	GetEyeMatrix(const std::string& EyeName);
	void		WaitForFrameStart();

	vr::IVRSystem*	mHmd = nullptr;
	float			mNearClip = 0.1f;
	float			mFarClip = 40.0f;
};





void ApiOpenvr::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<THmdWrapper>( Namespace );
}


void ApiOpenvr::THmdWrapper::Construct(Bind::TCallback& Params)
{
	//auto Filename = Params.GetArgumentFilename(0);
	mHmd.reset( new Openvr::THmd() );

}


void ApiOpenvr::THmdWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<SubmitEyeTexture_FunctionName>( SubmitEyeTexture );
	Template.BindFunction<GetEyeMatrix_FunctionName>( GetEyeMatrix );
	Template.BindFunction<BeginFrame_FunctionName>( BeginFrame );
}


void ApiOpenvr::THmdWrapper::SubmitEyeTexture(Bind::TCallback& Params)
{
	auto& This = Params.This<ApiOpenvr::THmdWrapper>();
	auto EyeName = Params.GetArgumentString(0);
	auto& EyeTexture = Params.GetArgumentPointer<TImageWrapper>(1);
	This.mHmd->SubmitEyeFrame(EyeName, EyeTexture);
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

void ApiOpenvr::THmdWrapper::BeginFrame(Bind::TCallback& Params)
{
	auto& This = Params.This<ApiOpenvr::THmdWrapper>();
	auto& Hmd = *This.mHmd;
	Hmd.WaitForFrameStart();
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


Openvr::THmd::THmd()
{
	vr::EVRInitError Error = vr::VRInitError_None;
	mHmd = vr::VR_Init( &Error, vr::VRApplication_Scene );
	IsOkay( Error, "VR_Init" );
	
	
	//m_strDriver = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String );
	//m_strDisplay = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String );
}

Openvr::THmd::~THmd()
{
	if ( mHmd )
	{
		vr::VR_Shutdown();
		mHmd = nullptr;
	}
}

void Openvr::THmd::EnumEyes(std::function<void(const TEyeMatrix& Eye)>& Enum)
{
	auto& Hmd = *mHmd;
	
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
	auto EyeToHeadMatrix = Hmd.GetEyeToHeadTransform( Eye );
	//EyeMatrix.mPose = EyeToHeadMatrix.invert();

	Hmd.GetRecommendedRenderTargetSize( &EyeMatrix.mRenderTargetWidth, &EyeMatrix.mRenderTargetHeight );

	return EyeMatrix;
}


void Openvr::THmd::WaitForFrameStart()
{
	auto& Compositor = *vr::VRCompositor();
	vr::TrackedDevicePose_t TrackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
	vr::TrackedDevicePose_t* GamePoseArray = nullptr;
	size_t GamePoseArraySize = 0;
	auto Error = Compositor.WaitGetPoses(TrackedDevicePoses, vr::k_unMaxTrackedDeviceCount, GamePoseArray, GamePoseArraySize);
	Openvr::IsOkay(Error, "WaitGetPoses");
}

void Openvr::THmd::SubmitEyeFrame(const std::string& EyeName,TImageWrapper& Image)
{
	vr::EVREye Eye;
	
	if ( EyeName == Openvr::EyeName_Left )
		Eye = vr::Eye_Left;
	else if ( EyeName == Openvr::EyeName_Right )
		Eye = vr::Eye_Right;
	else
	{
		std::stringstream Error;
		Error << "Unhandled eye name " << EyeName;
		throw Soy::AssertException(Error);
	}
	
	auto& Compositor = *vr::VRCompositor();

	vr::Texture_t EyeTexture;
	auto& Texture = Image.GetTexture();
	EyeTexture.handle = reinterpret_cast<void*>( static_cast<uintptr_t>(Texture.mTexture.mName) );
	EyeTexture.eType = vr::TextureType_OpenGL;
	EyeTexture.eColorSpace = vr::ColorSpace_Gamma;

	auto Error = Compositor.Submit( Eye, &EyeTexture );
	if ( Error == vr::VRCompositorError_TextureUsesUnsupportedFormat )
	{
		std::stringstream ErrorString;
		ErrorString << "Eye Texture Submit (" << Texture.mMeta << ")";
		Openvr::IsOkay(Error, ErrorString.str());
	}
	if ( Error == vr::VRCompositorError_DoNotHaveFocus )
	{
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
