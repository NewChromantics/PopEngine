#include "TApiOpenvr.h"
#include "Libs/OpenVr/headers/openvr.h"
#include "TApiCommon.h"
#include "SoyOpengl.h"

namespace ApiOpenvr
{
	const char Namespace[] = "Pop.Openvr";

	DEFINE_BIND_TYPENAME(Hmd);
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
	
	void	SubmitEyeFrame(const std::string& EyeName,TImageWrapper& Image);
	void	EnumEyes(std::function<void(const TEyeMatrix& Eye)>& Enum);

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
//	Template.BindFunction<ApiDll::BindFunction_FunctionName>( BindFunction );
//	Template.BindFunction<ApiDll::CallFunction_FunctionName>( CallFunction );
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

void Openvr::THmd::EnumEyes(std::function<void(const TEyeMatrix& Eye)>& Enum)
{
	auto& Hmd = *mHmd;
	
	auto EnumEye = [&](vr::Hmd_Eye Eye,const std::string& EyeName)
	{
		TEyeMatrix EyeMatrix;
		EyeMatrix.mName = EyeName;
		
		//	demo for opengl transposes this, maybe
		EyeMatrix.mProjection = Hmd.GetProjectionMatrix( Eye, mNearClip, mFarClip );
		
		//	3x4 matrix eye pose
		//vr::HmdMatrix34_t
		auto EyeToHeadMatrix = Hmd.GetEyeToHeadTransform( Eye );
		//EyeMatrix.mPose = EyeToHeadMatrix.invert();
		
		Hmd.GetRecommendedRenderTargetSize( &EyeMatrix.mRenderTargetWidth, &EyeMatrix.mRenderTargetHeight );

		
		Enum( EyeMatrix );
	};
	
	EnumEye( vr::Eye_Left, Openvr::EyeName_Left );
	EnumEye( vr::Eye_Right, Openvr::EyeName_Right );
}


Openvr::THmd::~THmd()
{
	if ( mHmd )
	{
		vr::VR_Shutdown();
		mHmd = nullptr;
	}
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

	vr::Texture_t EyeTexture;
	auto& Texture = Image.GetTexture();
	EyeTexture.handle = reinterpret_cast<void*>( static_cast<uintptr_t>(Texture.mTexture.mName) );
	EyeTexture.eType = vr::TextureType_OpenGL;
	EyeTexture.eColorSpace = vr::ColorSpace_Gamma;
	
	auto Error = vr::VRCompositor()->Submit( Eye, &EyeTexture );
	Openvr::IsOkay( Error, "Eye Texture Submit");
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
