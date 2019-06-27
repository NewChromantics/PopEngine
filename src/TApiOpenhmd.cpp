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
	const std::string	DeviceName_Prefix = "Openhmd_";
	
	class TContext;
	TContext& 			GetContext();
	
	void 				ThrowError(ohmd_context* Context,const std::string& ErrorContext);
	void 				ThrowError(TContext& Context,const std::string& ErrorContext);
}


class Openhmd::TContext
{
public:
	TContext();
	~TContext();
	
	int				GetDeviceIndex(std::string DeviceName);
	
public:
	ohmd_context*	mContext = nullptr;
};


//	maybe rename to camera
class TEyeMatrix
{
public:
	std::string			mName;
	float4x4			mProjection;
	uint32_t			mRenderTargetWidth = 0;		//	"recommended"
	uint32_t			mRenderTargetHeight = 0;	//	"recommended"
};

/*

class Openhmd::THmdFrame
{
public:
	TEyeMatrix		mEye;
	Opengl::TAsset	mEyeTexture;
};
*/

class Openhmd::THmd
{
public:
	THmd(TContext& Context,const std::string& DeviceName);
	~THmd();

	TEyeMatrix		GetEyeMatrix(const std::string& EyeName);
	
public:
	TContext&		mContext;
	ohmd_device*	mDevice = nullptr;
};



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

int Openhmd::TContext::GetDeviceIndex(std::string DeviceName)
{
	if ( !Soy::StringTrimLeft( DeviceName, DeviceName_Prefix, true ) )
	{
		std::stringstream Error;
		Error << "Device name not prefixed with " << DeviceName_Prefix;
		throw Soy::AssertException( Error );
	}
	
	auto Index = DeviceName[0] - '0';
	return Index;
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
	Context.BindObjectType<THmdWrapper>( Namespace );
}


void ApiOpenhmd::EnumDevices(Bind::TCallback &Params)
{
	auto& TheContext = Openhmd::GetContext();
	auto* Context = TheContext.mContext;
	
	auto DeviceCount = ohmd_ctx_probe( Context );
	if ( DeviceCount < 0 )
		Openhmd::ThrowError(Context,"ohmd_ctx_probe");
	
	Array<Bind::TObject> Hmds;
	for ( auto d=0;	d<DeviceCount;	d++ )
	{
		auto Vendor = ohmd_list_gets( Context, d, OHMD_VENDOR );
		auto Product = ohmd_list_gets( Context, d, OHMD_PRODUCT );
		auto Path = ohmd_list_gets( Context, d, OHMD_PATH );
		
		auto Hmd = Params.mContext.CreateObjectInstance(Params.mLocalContext);
		
		//	todo: need a unique identifier for serial!
		//	gr: see if this order is persistent
		std::stringstream Serial;
		Serial << Openhmd::DeviceName_Prefix << d;
		
		Hmd.SetString("Vendor", Vendor );
		Hmd.SetString("Serial", Serial.str() );
		Hmd.SetString("Product", Product );
		Hmd.SetString("Path", Path );
		Hmds.PushBack(Hmd);
	}
	
	Params.Return( GetArrayBridge(Hmds ) );
}

void ApiOpenhmd::THmdWrapper::Construct(Bind::TCallback& Params)
{
	auto DeviceName = Params.GetArgumentFilename(0);
	
	mHmd.reset( new Openhmd::THmd(DeviceName) );

	/*
	auto OnRender = [this](Openhmd::THmdFrame& Left,Openhmd::THmdFrame& Right)
	{
		this->OnRender( Left, Right );
	};
	mHmd->mOnRender = OnRender;
	*/
}


void ApiOpenhmd::THmdWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<GetEyeMatrix_FunctionName>( &THmdWrapper::GetEyeMatrix );
	//Template.BindFunction<BeginFrame_FunctionName>( &THmdWrapper::BeginFrame );
}

void ApiOpenhmd::THmdWrapper::GetEyeMatrix(Bind::TCallback& Params)
{
	auto EyeName = Params.GetArgumentString(0);

	auto EyeMatrix = mHmd->GetEyeMatrix(EyeName);

	auto Obj = Params.mContext.CreateObjectInstance(Params.mLocalContext);
	//Obj.SetArray("
	Params.Return(Obj);
}


Openhmd::THmd::THmd(TContext& Context,const std::string& DeviceName) :
	mContext	( Context )
{
	auto* settings = ohmd_device_settings_create(Context.mContext);
	
	// If OHMD_IDS_AUTOMATIC_UPDATE is set to 0, ohmd_ctx_update() must be called at least 10 times per second.
	// It is enabled by default.
	int auto_update = 1;
	ohmd_device_settings_seti( settings, OHMD_IDS_AUTOMATIC_UPDATE, &auto_update );
	
	auto DeviceIndex = Context.GetDeviceIndex( DeviceName );
	mDevice = ohmd_list_open_device_s( Context.mContext, DeviceIndex, settings );
	if ( !mDevice )
		ThrowError(Context,"ohmd_list_open_device_s");
	
	ohmd_device_settings_destroy(settings);
	
	UpdateMeta();
}

Openhmd::THmd::~THmd()
{
	
}

void Openhmd::THmd::UpdateMeta()
{
	/*
	int mResolutionWidth;
	int mResolutionHeight;
	float mScreenPhysicalWidthMetres;
	float mScreenPhysicalHeightMetres
	float mIpd;
	float distortion_coeffs[6];
	
	auto GetInt = [&](ohmd_int_value Key,int& Value)
	{
		auto Result = ohmd_device_geti( mDevice, Key, &Value );
		if ( Result < 0 )
			ThrowError( mContext.mContext, "ohmd_device_geti");
	};
	
	auto GetFloat = [&](ohmd_int_value Key,float& Value)
	{
		auto Result = ohmd_device_getf( mDevice, Key, &Value );
		if ( Result < 0 )
			ThrowError( mContext.mContext, "ohmd_device_getf");
	};
	
	auto GetFloats = [&](ohmd_int_value Key,float[16]& Value)
	{
		auto Result = ohmd_device_getf( mDevice, Key, &Value[0] );
		if ( Result < 0 )
			ThrowError( mContext.mContext, "ohmd_device_getf");
	};
	
	GetInt( OHMD_SCREEN_HORIZONTAL_RESOLUTION, mResolutionWidth );
	GetInt( OHMD_SCREEN_VERTICAL_RESOLUTION, mResolutionHeight );

	GetFloat( OHMD_EYE_IPD, mIpd );
	float viewport_scale[2];
	float distortion_coeffs[4];
	float aberr_scale[3];
	float sep;
	float left_lens_center[2];
	float right_lens_center[2];
	//viewport is half the screen
	GetFloat( OHMD_SCREEN_HORIZONTAL_SIZE, mScreenPhysicalWidthMetres );
	mScreenPhysicalWidthMetres /= 2.0f;
	GetFloat( OHMD_SCREEN_VERTICAL_SIZE, mScreenPhysicalHeightMetres );
	
	//distortion coefficients
	GetFloats( OHMD_UNIVERSAL_DISTORTION_K, distortion_coeffs );
	ohmd_device_getf(hmd, OHMD_UNIVERSAL_ABERRATION_K, &(aberr_scale[0]));
	//calculate lens centers (assuming the eye separation is the distance between the lens centers)
	ohmd_device_getf(hmd, OHMD_LENS_HORIZONTAL_SEPARATION, &sep);
	ohmd_device_getf(hmd, OHMD_LENS_VERTICAL_POSITION, &(left_lens_center[1]));
	ohmd_device_getf(hmd, OHMD_LENS_VERTICAL_POSITION, &(right_lens_center[1]));
	left_lens_center[0] = viewport_scale[0] - sep/2.0f;
	right_lens_center[0] = sep/2.0f;
	//assume calibration was for lens view to which ever edge of screen is further away from lens center
	float warp_scale = (left_lens_center[0] > right_lens_center[0]) ? left_lens_center[0] : right_lens_center[0];
	float warp_adj = 1.0f;
	
	const char* vertex;
	ohmd_gets(OHMD_GLSL_DISTORTION_VERT_SRC, &vertex);
	const char* fragment;
	ohmd_gets(OHMD_GLSL_DISTORTION_FRAG_SRC, &fragment);
	
	float mat[16];
	ohmd_device_getf(hmd, OHMD_LEFT_EYE_GL_PROJECTION_MATRIX, mat);
	printf("Projection L: ");
	print_matrix(mat);
	printf("\n");
	ohmd_device_getf(hmd, OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX, mat);
	printf("Projection R: ");
	print_matrix(mat);
	printf("\n");
	ohmd_device_getf(hmd, OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX, mat);
	printf("View: ");
	print_matrix(mat);
	 */
}

TEyeMatrix Openhmd::THmd::GetEyeMatrix(const std::string& EyeName)
{
	
}

