#include "TApiOpenhmd.h"
#include "Libs/OpenHmd/openhmd.h"
#include "TApiCommon.h"
#include "SoyOpengl.h"


namespace ApiOpenhmd
{
	const char Namespace[] = "Pop.Openhmd";

	DEFINE_BIND_TYPENAME(Hmd);

	DEFINE_BIND_FUNCTIONNAME(EnumDevices);
	
	DEFINE_BIND_FUNCTIONNAME(GetEyeMatrix);
	DEFINE_BIND_FUNCTIONNAME(BeginFrame);

	void		EnumDevices(Bind::TCallback& Params);
}

namespace Openhmd
{
	const std::string	EyeName_Left = "Left";
	const std::string	EyeName_Right = "Right";

	class TContext;
	TContext& 			GetContext();
}


class Openhmd::TContext
{
public:
	TContext();
	~TContext();
	
public:
	ohmd_context*	mContext = nullptr;
};

/*
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


class Openhmd::THmdFrame
{
public:
	TEyeMatrix		mEye;
	Opengl::TAsset	mEyeTexture;
};

class Openhmd::THmd : public SoyThread
{
public:
	THmd(bool OverlayApp);
	~THmd();

	virtual void	Thread() override;
	
	void			SubmitEyeFrame(vr::Hmd_Eye Eye,Opengl::TAsset Texture);
	void			EnumEyes(std::function<void(const TEyeMatrix& Eye)>& Enum);
	TEyeMatrix		GetEyeMatrix(const std::string& EyeName);
	void			WaitForFrameStart();

public:
	std::function<void(THmdFrame&,THmdFrame&)>	mOnRender;
	vr::IVRSystem*	mHmd = nullptr;
	float			mNearClip = 0.1f;
	float			mFarClip = 40.0f;
};
*/


Openhmd::TContext::TContext()
{
	mContext = ohmd_ctx_create();
	if ( !mContext )
		throw Soy_AssertException("Failed to create context");
}

Openhmd::TContext::~TContext()
{
	ohmd_ctx_destroy( mContext );
}
	

std::shared_ptr<Openhmd::TContext> gContext;

Openhmd::TContext& Openhmd::GetContext()
{
	if ( gContext )
		return *gContext;
	
	if ( !gContext )
		gContext.reset( new TContext() );

	return *gContext;
}



void ApiOpenhmd::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<EnumDevices_FunctionName>( EnumDevices, Namespace );
	//Context.BindObjectType<THmdWrapper>( Namespace );
}

void ApiOpenhmd::THmdWrapper::Construct(Bind::TCallback& Params)
{
	/*
	auto Device = Params.GetArgumentFilename(0);
	
	mHmd.reset( new Openhmd::THmd(IsOverlay) );

	auto OnRender = [this](Openhmd::THmdFrame& Left,Openhmd::THmdFrame& Right)
	{
		this->OnRender( Left, Right );
	};
	mHmd->mOnRender = OnRender;
	 */
}

void ApiOpenhmd::EnumDevices(Bind::TCallback &Params)
{
	auto& TheContext = Openhmd::GetContext();
	auto* Context = TheContext.mContext;
	
	auto DeviceCount = ohmd_ctx_probe( Context );
	
	Array<Bind::TObject> Hmds;
	for ( auto d=0;	d<DeviceCount;	d++ )
	{
		auto Vendor = ohmd_list_gets( Context, d, OHMD_VENDOR );
		auto Product = ohmd_list_gets( Context, d, OHMD_PRODUCT );
		auto Path = ohmd_list_gets( Context, d, OHMD_PATH );
		
		auto Hmd = Params.mContext.CreateObjectInstance(Params.mLocalContext);
		//	todo: need a unique identifier for serial!
		Hmd.SetString("Vendor", Vendor );
		Hmd.SetString("Serial", Product );
		Hmd.SetString("Product", Product );
		Hmd.SetString("Path", Path );
		Hmds.PushBack(Hmd);
	}
	
	Params.Return( GetArrayBridge(Hmds ) );
}

/*
void ApiOpenhmd::THmdWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<GetEyeMatrix_FunctionName>( &THmdWrapper::GetEyeMatrix );
	Template.BindFunction<BeginFrame_FunctionName>( &THmdWrapper::BeginFrame );
}

void ApiOpenhmd::THmdWrapper::OnRender(Openhmd::THmdFrame& Left,Openhmd::THmdFrame& Right)
{
	//	call javascript
}

void ApiOpenhmd::THmdWrapper::GetEyeMatrix(Bind::TCallback& Params)
{
	auto& This = Params.This<ApiOpenhmd::THmdWrapper>();
	auto EyeName = Params.GetArgumentString(0);

	auto EyeMatrix = This.mHmd->GetEyeMatrix(EyeName);

	auto Obj = Params.mContext.CreateObjectInstance(Params.mLocalContext);
	//Obj.SetArray("
	Params.Return(Obj);
}

void ApiOpenhmd::THmdWrapper::BeginFrame(Bind::TCallback& Params)
{
	auto& This = Params.This<ApiOpenhmd::THmdWrapper>();
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

void Openhmd::IsOkay(vr::EVRInitError Error,const std::string& Context)
{
	if ( Error == vr::VRInitError_None )
		return;

	std::stringstream Exception;
	Exception << Context << " Openhmd error: " << Error;
	throw Soy::AssertException(Exception);
}

void Openhmd::IsOkay(vr::EVRCompositorError Error,const std::string& Context)
{
	if ( Error == vr::VRCompositorError_None )
		return;
	
	std::stringstream Exception;
	Exception << Context << " Openhmd error: " << Error;
	throw Soy::AssertException(Exception);
}


Openhmd::THmd::THmd(bool OverlayApp) :
	SoyThread	( "Openhmd::THmd" )
{
	vr::EVRInitError Error = vr::VRInitError_None;
	auto AppType = OverlayApp ? vr::VRApplication_Overlay : vr::VRApplication_Scene;
	mHmd = vr::VR_Init( &Error, AppType );
	IsOkay( Error, "VR_Init" );
	
	Start();
	//m_strDriver = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String );
	//m_strDisplay = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String );
}

Openhmd::THmd::~THmd()
{
	Stop(true);
	
	if ( mHmd )
	{
		vr::VR_Shutdown();
		mHmd = nullptr;
	}
}


void Openhmd::THmd::Thread()
{
	//	loop
	while ( IsThreadRunning() )
	{
		//	block until time for a frame
		//	update frames
		WaitForFrameStart();
		
		try
		{
			THmdFrame Left;
			THmdFrame Right;

			mOnRender( Left, Right );
			
			SubmitEyeFrame( vr::Eye_Left, Left.mEyeTexture );
			SubmitEyeFrame( vr::Eye_Right, Right.mEyeTexture );
		}
		catch(std::exception& e)
		{
			//	gr: need to force a submission here
			std::Debug << "HMD render exception " << e.what() << std::endl;
		}

	}
}


void Openhmd::THmd::EnumEyes(std::function<void(const TEyeMatrix& Eye)>& Enum)
{
	auto& Hmd = *mHmd;
	
	auto Left = GetEyeMatrix(EyeName_Left);
	Enum(Left);
	auto Right = GetEyeMatrix(EyeName_Right);
	Enum(Right);
}

vr::Hmd_Eye GetHmdEye(const std::string& EyeName)
{
	if ( EyeName == Openhmd::EyeName_Left )
		return vr::Hmd_Eye::Eye_Left;
	if ( EyeName == Openhmd::EyeName_Right )
		return vr::Hmd_Eye::Eye_Right;

	std::stringstream Error;
	Error << "Unknown eye name " << EyeName;
	throw Soy::AssertException(Error);
}

TEyeMatrix Openhmd::THmd::GetEyeMatrix(const std::string& EyeName)
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


void Openhmd::THmd::WaitForFrameStart()
{
	auto& Compositor = *vr::VRCompositor();
	vr::TrackedDevicePose_t TrackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
	vr::TrackedDevicePose_t* GamePoseArray = nullptr;
	size_t GamePoseArraySize = 0;
	auto Error = Compositor.WaitGetPoses(TrackedDevicePoses, vr::k_unMaxTrackedDeviceCount, GamePoseArray, GamePoseArraySize);
	Openhmd::IsOkay(Error, "WaitGetPoses");
}

void Openhmd::THmd::SubmitEyeFrame(vr::Hmd_Eye Eye,Opengl::TAsset Texture)
{
	auto& Compositor = *vr::VRCompositor();

	vr::Texture_t EyeTexture;
	EyeTexture.handle = reinterpret_cast<void*>( static_cast<uintptr_t>(Texture.mName) );
	EyeTexture.eType = vr::TextureType_OpenGL;
	EyeTexture.eColorSpace = vr::ColorSpace_Gamma;

	auto Error = Compositor.Submit( Eye, &EyeTexture );
	if ( Error == vr::VRCompositorError_TextureUsesUnsupportedFormat )
	{
		std::stringstream ErrorString;
		//	gr: this meta was useful!
		//ErrorString << "Eye Texture Submit (" << Texture.mMeta << ")";
		ErrorString << "Eye Texture Submit (" << Texture.mName << ")";
		Openhmd::IsOkay(Error, ErrorString.str());
	}
	if ( Error == vr::VRCompositorError_DoNotHaveFocus )
	{
		WaitForFrameStart();
	
		//	submit again
		Error = Compositor.Submit( Eye, &EyeTexture );
		Openhmd::IsOkay(Error, "Eye Texture Submit B");
	}
	Openhmd::IsOkay( Error, "Eye Texture Submit A");

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
