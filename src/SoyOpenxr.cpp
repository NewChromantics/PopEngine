#include "SoyOpenxr.h"
#include <SoyThread.h>
#include <openxr/openxr.h>
#include <magic_enum.hpp>
#include <SoyRuntimeLibrary.h>
#include <functional>

namespace Win32
{
	class TOpenglContext;
}

#if defined(TARGET_WINDOWS)
#define ENABLE_DIRECTX
#include "Win32OpenglWindow.h"
#endif

#if defined(ENABLE_DIRECTX)
#include <SoyDirectx.h>
#define XR_USE_GRAPHICS_API_D3D11
#endif

#if defined(ENABLE_OPENGL)
#define XR_USE_GRAPHICS_API_OPENGL
#endif

#if defined(TARGET_WINDOWS)
#define XR_USE_PLATFORM_WIN32
#endif


#if defined(XR_USE_PLATFORM_WIN32) && defined(XR_USE_GRAPHICS_API_OPENGL)
typedef struct XrGraphicsBindingOpenGLWin32KHR XrGraphicsBindingOpenGL;
#else
typedef int XrGraphicsBindingOpenGL;
#endif

#if defined(XR_USE_GRAPHICS_API_D3D11)
#else
typedef int XrGraphicsBindingD3D11KHR;
#endif

#include <openxr/openxr_platform.h>

namespace Openxr
{
	class TSession;
	
	void					LoadDll();
	Soy::TRuntimeLibrary&	GetDll();

	void	IsOkay(XrResult Result,const char* Context);
	void	IsOkay(XrResult Result,const std::string& Context);

	void	EnumExtensions(std::function<void(std::string&,uint32_t)> OnExtension);
	
	template<typename FUNCTION>
	std::function<FUNCTION>		GetFunction(XrInstance mInstance, const char* FunctionName);

	namespace Actions
	{
		enum TYPE
		{
			LeftSide,
			RightSide
		};
	}

	constexpr XrPosef PoseIdentity() 
	{
		return { {0, 0, 0, 1}, {0, 0, 0} };
	}
}



class Xr::TDevice
{
public:
	virtual ~TDevice() {}
};


class Openxr::TSession : public SoyThread, public Xr::TDevice
{
public:
	TSession(Win32::TOpenglContext& Context);
	~TSession();
	
private:
	virtual bool	ThreadIteration() override;
	bool			ProcessEvents();

	void			CreateInstance(const std::string& ApplicationName,uint32_t ApplicationVersion,const std::string& EngineName,uint32_t EngineVersion);
	void			InitializeSystem();
	void			InitializeSession();
	void			CreateActions();
	void			CreateSpaces();
	void			CreateSwapchains();
	
	void			EnumEnvironmentBlendModes(ArrayBridge<XrEnvironmentBlendMode>&& Modes);
	void			EnumViewConfigurations(XrViewConfigurationType ViewType, ArrayBridge<XrViewConfigurationView>&& Views);

	XrPath			GetXrPath(const char* PathString);
	
	void			CreateDx11Device();
	XrGraphicsBindingOpenGL		InitOpengl();
	XrGraphicsBindingD3D11KHR	InitDirectx();
	bool			HasExtension(const char* ExtensionName);

private:
	Win32::TOpenglContext*	mOpenglContext = nullptr;

	XrFormFactor	mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrViewConfigurationType mPrimaryViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	bool			mSessionRunning = false;	//	after begin
	//uint32_t		mStereoViewCount = 2; // PRIMARY_STEREO view configuration always has 2 views

	XrInstance		mInstance = XR_NULL_HANDLE;
	XrSystemId		mSystemId = XR_NULL_SYSTEM_ID;
	XrActionSet		mActionSet = XR_NULL_HANDLE;
	XrSession		mSession = XR_NULL_HANDLE;

	XrSpace			mSceneSpace = XR_NULL_HANDLE;
	//xr::SpaceHandle m_sceneSpace;
	XrReferenceSpaceType	mSceneSpaceType = XR_REFERENCE_SPACE_TYPE_MAX_ENUM;

	XrSessionState	mSessionState{XR_SESSION_STATE_UNKNOWN};
	
	XrEnvironmentBlendMode mEnvironmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

	//const std::unique_ptr<sample::IGraphicsPluginD3D11> m_graphicsPlugin;
	//xr::ExtensionDispatchTable m_extensions;
	/*
	struct {
		bool DepthExtensionSupported{false};
		bool UnboundedRefSpaceSupported{false};
		bool SpatialAnchorSupported{false};
	} m_optionalExtensions;
	
	
	struct Hologram {
		sample::Cube Cube;
		xr::SpatialAnchorHandle Anchor;
	};
	std::vector<Hologram> m_holograms;
	
	std::optional<uint32_t> m_mainCubeIndex;
	std::optional<uint32_t> m_spinningCubeIndex;
	XrTime m_spinningCubeStartTime;
	*/
	std::map<Actions::TYPE, XrPath>	mSubActionPaths;
	/*
	std::array<XrPath, 2> m_subactionPaths{};
	std::array<sample::Cube, 2> m_cubesInHand{};
	
	xr::ActionSetHandle m_actionSet;
	xr::ActionHandle m_placeAction;
	xr::ActionHandle m_exitAction;
	xr::ActionHandle m_poseAction;
	xr::ActionHandle m_vibrateAction;
	
	XrEnvironmentBlendMode m_environmentBlendMode{};
	xr::math::NearFar m_nearFar{};
	
	struct SwapchainD3D11 {
		xr::SwapchainHandle Handle;
		DXGI_FORMAT Format{DXGI_FORMAT_UNKNOWN};
		uint32_t Width{0};
		uint32_t Height{0};
		uint32_t ArraySize{0};
		std::vector<XrSwapchainImageD3D11KHR> Images;
	};
	
	struct RenderResources {
		XrViewState ViewState{XR_TYPE_VIEW_STATE};
		std::vector<XrView> Views;
		std::vector<XrViewConfigurationView> ConfigViews;
		SwapchainD3D11 ColorSwapchain;
		SwapchainD3D11 DepthSwapchain;
		std::vector<XrCompositionLayerProjectionView> ProjectionLayerViews;
		std::vector<XrCompositionLayerDepthInfoKHR> DepthInfoViews;
	};
	
	std::unique_ptr<RenderResources> m_renderResources{};
	*/
};



std::shared_ptr<Xr::TDevice> Openxr::CreateDevice(Win32::TOpenglContext& Context)
{
	std::shared_ptr<Xr::TDevice> Device(new Openxr::TSession(Context));
	return Device;
}

Soy::TRuntimeLibrary& Openxr::GetDll()
{
	static std::shared_ptr<Soy::TRuntimeLibrary> Dll;
	if (Dll)
		return *Dll;

#if defined(TARGET_WINDOWS)
	//	current bodge
	const char* Filename = "openxr_loader.dll";
	Dll.reset(new Soy::TRuntimeLibrary(Filename));
	return *Dll;
#endif
}


void Openxr::LoadDll()
{
	GetDll();
}

void Openxr::IsOkay(XrResult Result,const char* Context)
{
	if ( Result == XR_SUCCESS )
		return;

	std::stringstream Error;
	Error << "Openxr error " << magic_enum::enum_name(Result) << " in " << Context;
	throw Soy::AssertException(Error);
}



void Openxr::IsOkay(XrResult Result, const std::string& Context)
{
	IsOkay(Result, Context.c_str());
}







Openxr::TSession::TSession(Win32::TOpenglContext& Context) :
	SoyThread	( "Openxr::TSession" )
{
	mOpenglContext = &Context;

	//	lets add these later
	//	openvr has it, does anything else?
	auto ApplicationName = "Pop";
	auto ApplicationVersion = 1;
	auto EngineName = "PopEngine";
	auto EngineVersion = 1;

	//https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/3579c5fcc123524a545e6fd361e76b7f819aa8a3/src/tests/hello_xr/main.cpp
	CreateInstance( ApplicationName, ApplicationVersion, EngineName, EngineVersion );
	InitializeSystem();
	CreateDx11Device();
	InitializeSession();
	CreateSpaces();
	CreateSwapchains();

	/*
	std::shared_ptr<IPlatformPlugin> platformPlugin = CreatePlatformPlugin(options, data);
	std::shared_ptr<IGraphicsPlugin> graphicsPlugin = CreateGraphicsPlugin(options, platformPlugin);
	std::shared_ptr<IOpenXrProgram> program = CreateOpenXrProgram(options, platformPlugin, graphicsPlugin);
	program->CreateInstance();
	program->InitializeSystem();
	program->InitializeSession();
	program->CreateSwapchains();
	
	while (!quitKeyPressed) {
		bool exitRenderLoop = false;
		program->PollEvents(&exitRenderLoop, &requestRestart);
		if (exitRenderLoop) {
			break;
		}
		
		if (program->IsSessionRunning()) {
			program->PollActions();
			program->RenderFrame();
		} else {
			// Throttle loop since xrWaitFrame won't be called.
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
	}
	*/
	Start();
}

Openxr::TSession::~TSession()
{
	Stop(true);
}

bool Openxr::TSession::ThreadIteration()
{
	if (!ProcessEvents())
		return false;

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	return true;
}

void Openxr::TSession::EnumEnvironmentBlendModes(ArrayBridge<XrEnvironmentBlendMode>&& Modes)
{
	uint32_t Count = 0;
	auto Result = xrEnumerateEnvironmentBlendModes( mInstance, mSystemId, mPrimaryViewConfigType, 0, &Count, nullptr );
	IsOkay(Result,"xrEnumerateEnvironmentBlendModes (count)");

	Modes.SetSize(Count);
	Result = xrEnumerateEnvironmentBlendModes( mInstance, mSystemId, mPrimaryViewConfigType, Count, &Count, Modes.GetArray() );
	IsOkay(Result,"xrEnumerateEnvironmentBlendModes (enum)");
}
	

void Openxr::EnumExtensions(std::function<void(std::string&,uint32_t)> OnExtension)
{
	// Fetch the list of extensions supported by the runtime.
	uint32_t ExtensionCount = 0;
	auto Result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &ExtensionCount, nullptr);
	IsOkay(Result,"xrEnumerateInstanceExtensionProperties(count)");

	Array< XrExtensionProperties> ExtensionProperties;
	ExtensionProperties.SetSize(ExtensionCount);
	ExtensionProperties.SetAll({ XR_TYPE_EXTENSION_PROPERTIES });

	Result = xrEnumerateInstanceExtensionProperties(nullptr, ExtensionCount, &ExtensionCount, ExtensionProperties.GetArray());
	IsOkay(Result,"xrEnumerateInstanceExtensionProperties (enum)");

	//	report each
	for (auto i = 0; i < ExtensionProperties.GetSize(); i++)
	{
		auto& Extension = ExtensionProperties[i];
		std::string Name(Extension.extensionName);
		OnExtension(Name, Extension.extensionVersion);
	}
}

void Openxr::TSession::CreateInstance(const std::string& ApplicationName,uint32_t ApplicationVersion,const std::string& EngineName,uint32_t EngineVersion)
{
	//	https://github.com/microsoft/OpenXR-MixedReality/blob/master/samples/BasicXrApp/OpenXrProgram.cpp
	// Build out the extensions to enable. Some extensions are required and some are optional.
	//const std::vector<const char*> enabledExtensions = SelectExtensions();
	
	//	gr: just enable all the supported extensions
	Array<std::string> EnabledExtensionStrings;
	auto EnumExtension = [&](const std::string& Name, uint32_t Version)
	{
		std::Debug << "Openxr Extension: " << Name << " (Version " << Version << ")" << std::endl;
		EnabledExtensionStrings.PushBack(Name);
	};
	EnumExtensions(EnumExtension);
	
	//	func needs char* pointers
	Array<const char*> EnabledExtensions;
	for (auto i = 0; i < EnabledExtensionStrings.GetSize(); i++)
	{
		auto* ExtensionName = EnabledExtensionStrings[i].c_str();
		EnabledExtensions.PushBack(ExtensionName);
	}

	//	hololens2 sample required these
	/*
	 XR_KHR_D3D11_ENABLE_EXTENSION_NAME
	 XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME
	 XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME
	 XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME
	 XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME
*/
	
	//	gr: is the pointer the key or is it just a string?
	XrInstanceCreateInfo CreateInfo{XR_TYPE_INSTANCE_CREATE_INFO};
	CreateInfo.enabledExtensionCount = EnabledExtensions.GetSize();
	CreateInfo.enabledExtensionNames = EnabledExtensions.GetArray();
	CreateInfo.applicationInfo = { "", ApplicationVersion, "", EngineVersion, XR_CURRENT_API_VERSION };
	Soy::StringToBuffer( ApplicationName, CreateInfo.applicationInfo.applicationName );
	Soy::StringToBuffer( EngineName, CreateInfo.applicationInfo.engineName );

	auto Result = xrCreateInstance( &CreateInfo, &mInstance );
	IsOkay(Result,"xrCreateInstance");
}

void Openxr::TSession::CreateActions()
{
	// Create an action set.
	{
		XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
		Soy::StringToBuffer("place_hologram_action_set",actionSetInfo.actionSetName);
		Soy::StringToBuffer("Placement",actionSetInfo.localizedActionSetName);
		auto Result = xrCreateActionSet( mInstance, &actionSetInfo, &mActionSet );
		IsOkay( Result, "xrCreateActionSet" );
	}
	
	// Create actions.
	//	gr: read up on these before writing
	/*
	{
		//	gr: where do these strings come from?
		mSubActionPaths[Actions::LeftSide] = GetXrPath("/user/hand/left");
		mSubActionPaths[Actions::RightSide] = GetXrPath("/user/hand/right");
		
		// Create an input action to place a hologram.
		{
			XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
			actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
			strcpy_s(actionInfo.actionName, "place_hologram");
			strcpy_s(actionInfo.localizedActionName, "Place Hologram");
			actionInfo.countSubactionPaths = (uint32_t)m_subactionPaths.size();
			actionInfo.subactionPaths = m_subactionPaths.data();
			CHECK_XRCMD(xrCreateAction(m_actionSet.Get(), &actionInfo, m_placeAction.Put()));
		}
		
		// Create an input action getting the left and right hand poses.
		{
			XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
			actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
			strcpy_s(actionInfo.actionName, "hand_pose");
			strcpy_s(actionInfo.localizedActionName, "Hand Pose");
			actionInfo.countSubactionPaths = (uint32_t)m_subactionPaths.size();
			actionInfo.subactionPaths = m_subactionPaths.data();
			CHECK_XRCMD(xrCreateAction(m_actionSet.Get(), &actionInfo, m_poseAction.Put()));
		}
		
		// Create an output action for vibrating the left and right controller.
		{
			XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
			actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
			strcpy_s(actionInfo.actionName, "vibrate");
			strcpy_s(actionInfo.localizedActionName, "Vibrate");
			actionInfo.countSubactionPaths = (uint32_t)m_subactionPaths.size();
			actionInfo.subactionPaths = m_subactionPaths.data();
			CHECK_XRCMD(xrCreateAction(m_actionSet.Get(), &actionInfo, m_vibrateAction.Put()));
		}
		
		// Create an input action to exit session
		{
			XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
			actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
			strcpy_s(actionInfo.actionName, "exit_session");
			strcpy_s(actionInfo.localizedActionName, "Exit session");
			actionInfo.countSubactionPaths = (uint32_t)m_subactionPaths.size();
			actionInfo.subactionPaths = m_subactionPaths.data();
			CHECK_XRCMD(xrCreateAction(m_actionSet.Get(), &actionInfo, m_exitAction.Put()));
		}
	}
	*/
	
	/*
	// Setup suggest bindings for simple controller.
	{
		std::vector<XrActionSuggestedBinding> bindings;
		bindings.push_back({m_placeAction.Get(), GetXrPath("/user/hand/right/input/select/click")});
		bindings.push_back({m_placeAction.Get(), GetXrPath("/user/hand/left/input/select/click")});
		bindings.push_back({m_poseAction.Get(), GetXrPath("/user/hand/right/input/grip/pose")});
		bindings.push_back({m_poseAction.Get(), GetXrPath("/user/hand/left/input/grip/pose")});
		bindings.push_back({m_vibrateAction.Get(), GetXrPath("/user/hand/right/output/haptic")});
		bindings.push_back({m_vibrateAction.Get(), GetXrPath("/user/hand/left/output/haptic")});
		bindings.push_back({m_exitAction.Get(), GetXrPath("/user/hand/right/input/menu/click")});
		bindings.push_back({m_exitAction.Get(), GetXrPath("/user/hand/left/input/menu/click")});
		
		XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
		suggestedBindings.interactionProfile = GetXrPath("/interaction_profiles/khr/simple_controller");
		suggestedBindings.suggestedBindings = bindings.data();
		suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
		CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance.Get(), &suggestedBindings));
	}
	*/
}

void Openxr::TSession::InitializeSystem()
{
	//	from
	//	https://github.com/microsoft/OpenXR-MixedReality/blob/master/samples/BasicXrApp/OpenXrProgram.cpp
	XrFormFactor m_formFactor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
	XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
	systemInfo.formFactor = m_formFactor;
	
	//	gr: sample does a while loop and tries every 1 sec if XR_ERROR_FORM_FACTOR_UNAVAILABLE
	auto Result = xrGetSystem(mInstance, &systemInfo, &mSystemId);
	if ( Result == XR_ERROR_FORM_FACTOR_UNAVAILABLE )
		std::Debug << "Sample would sleep & try again here..." << std::endl;
	IsOkay(Result,"xrGetSystem");

	//	no enum for invalid
	BufferArray<XrEnvironmentBlendMode,4> EnvironmentBlendModes;
	EnumEnvironmentBlendModes( GetArrayBridge(EnvironmentBlendModes) );
	if ( EnvironmentBlendModes.IsEmpty() )
		throw Soy::AssertException("No environment blend modes");
	mEnvironmentBlendMode = EnvironmentBlendModes[0];
}

void Openxr::TSession::CreateDx11Device()
{
	/*
	//	create the D3D11 device for the adapter associated with the system.
	XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
	CHECK_XRCMD(m_extensions.xrGetD3D11GraphicsRequirementsKHR(m_instance.Get(), m_systemId, &graphicsRequirements));
	
	// Create a list of feature levels which are both supported by the OpenXR runtime and this application.
	std::vector<D3D_FEATURE_LEVEL> featureLevels = {D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0};
	featureLevels.erase(std::remove_if(featureLevels.begin(),
									   featureLevels.end(),
									   [&](D3D_FEATURE_LEVEL fl) { return fl < graphicsRequirements.minFeatureLevel; }),
						featureLevels.end());
	CHECK_MSG(featureLevels.size() != 0, "Unsupported minimum feature level!");
	
	ID3D11Device* device = m_graphicsPlugin->InitializeDevice(graphicsRequirements.adapterLuid, featureLevels);
	
	XrGraphicsBindingD3D11KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
	graphicsBinding.device = device;
	*/
}

XrGraphicsBindingD3D11KHR Openxr::TSession::InitDirectx()
{
	Soy_AssertTodo();
}


//	GetFunction<decltype(Symbol)>()
template<typename FUNCTYPE>
std::function<FUNCTYPE> Openxr::GetFunction(XrInstance mInstance, const char* FunctionName)
{
	/*
	//	https://github.com/microsoft/OpenXR-MixedReality/blob/2117b8b4f522e8322e32277f4480272cbaa34ee6/shared/XrUtility/XrExtensions.h#L36
#define GET_INSTANCE_PROC_ADDRESS(name) \
    (void)xrGetInstanceProcAddr(instance, #name, reinterpret_cast<PFN_xrVoidFunction*>(const_cast<PFN_##name*>(&name)));
#define DEFINE_PROC_MEMBER(name) const PFN_##name name{nullptr};
*/
	PFN_xrVoidFunction Symbol = nullptr;
	auto Result = xrGetInstanceProcAddr(mInstance, FunctionName, &Symbol);
	IsOkay(Result, std::string("xrGetInstanceProcAddr(") + FunctionName);
	if (!Symbol)
	{
		std::stringstream Error;
		Error << "Function " << FunctionName << " =null from xrGetInstanceProcAddr";
		throw Soy::AssertException(Error.str());
	}

	//	cast & assign
	std::function<FUNCTYPE> FunctionPtr;
	FUNCTYPE* ff = reinterpret_cast<FUNCTYPE*>(Symbol);
	FunctionPtr = ff;
	return FunctionPtr;
}


XrGraphicsBindingOpenGL Openxr::TSession::InitOpengl()
{
	if (!mOpenglContext)
		throw Soy::AssertException("InitOpengl missing opengl context");

#if defined(XR_USE_GRAPHICS_API_OPENGL)
	//	gr: this should check for the extension first!
	auto xrGetOpenGLGraphicsRequirementsKHR_Function = GetFunction<decltype(xrGetOpenGLGraphicsRequirementsKHR)>(mInstance,"xrGetOpenGLGraphicsRequirementsKHR");
	XrGraphicsRequirementsOpenGLKHR Requirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
	auto Result = xrGetOpenGLGraphicsRequirementsKHR_Function(mInstance, mSystemId, &Requirements);
	IsOkay(Result, "xrGetOpenGLGraphicsRequirementsKHR");
	std::Debug << "Openxr Opengl version min=" << Requirements.minApiVersionSupported << " max=" << Requirements.maxApiVersionSupported << std::endl;
#endif

#if defined(XR_USE_PLATFORM_WIN32) && defined(XR_USE_GRAPHICS_API_OPENGL)
	auto& OpenglWindow = *mOpenglContext;
	XrGraphicsBindingOpenGLWin32KHR Binding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
	Binding.next = nullptr;
	Binding.hDC = OpenglWindow.GetHdc();
	Binding.hGLRC = OpenglWindow.GetHglrc();
	return Binding;
#endif

	throw Soy::AssertException("Unhandled opengl setup");

	/*
	//	create the D3D11 device for the adapter associated with the system.
	XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
	CHECK_XRCMD(m_extensions.xrGetD3D11GraphicsRequirementsKHR(m_instance.Get(), m_systemId, &graphicsRequirements));

	// Create a list of feature levels which are both supported by the OpenXR runtime and this application.
	std::vector<D3D_FEATURE_LEVEL> featureLevels = {D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0};
	featureLevels.erase(std::remove_if(featureLevels.begin(),
									   featureLevels.end(),
									   [&](D3D_FEATURE_LEVEL fl) { return fl < graphicsRequirements.minFeatureLevel; }),
						featureLevels.end());
	CHECK_MSG(featureLevels.size() != 0, "Unsupported minimum feature level!");

	ID3D11Device* device = m_graphicsPlugin->InitializeDevice(graphicsRequirements.adapterLuid, featureLevels);

	XrGraphicsBindingD3D11KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
	graphicsBinding.device = device;

	#if defined(XR_USE_GRAPHICS_API_OPENGL)
	auto Binding =
	//	gr: if API_DX?
#if defined(ENABLE_DIRECTX)
	auto* Dx11Device = reinterpret_cast<ID3D11Device*>(GraphicsDevice);
	XrGraphicsBindingD3D11KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
	graphicsBinding.device = Dx11Device;
#endif

	*/
}

void Openxr::TSession::InitializeSession()
{
	XrGraphicsBindingD3D11KHR Binding_Directx;
	XrGraphicsBindingOpenGL Binding_Opengl;

	XrSessionCreateInfo createInfo{ XR_TYPE_SESSION_CREATE_INFO };
	createInfo.systemId = mSystemId;
	createInfo.next = nullptr;	//	graphics binding

	//	try different bindings
	if (!createInfo.next)
	{
		try
		{
			Binding_Opengl = InitOpengl();
			createInfo.next = &Binding_Opengl;
			std::Debug << "Using opengl binding" << std::endl;
		}
		catch (std::exception& e)
		{
			std::Debug << "Opengl binding failed " << e.what() << std::endl;
		}
	}

	if (!createInfo.next)
	{
		try
		{
			Binding_Directx = InitDirectx();
			createInfo.next = &Binding_Directx;
			std::Debug << "Using directx binding" << std::endl;
		}
		catch (std::exception& e)
		{
			std::Debug << "directx binding failed " << e.what() << std::endl;
		}
	}

	if (!createInfo.next)
		throw Soy::AssertException("Failed to setup graphics binding");

	
	auto Result = xrCreateSession( mInstance, &createInfo, &mSession );
	IsOkay(Result, "xrCreateSession");

	//	create action set
	XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &mActionSet;
	Result = xrAttachSessionActionSets(mSession, &attachInfo);
	IsOkay(Result,"xrAttachSessionActionSets");
}

void Openxr::TSession::CreateSpaces()
{
	// Create a scene space to bridge interactions and all holograms.
	{
		if ( HasExtension(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME) )
		{
			// Unbounded reference space provides the best scene space for world-scale experiences.
			mSceneSpaceType = XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT;
		}
		else
		{
			// If running on a platform that does not support world-scale experiences, fall back to local space.
			mSceneSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
		}
		
		XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
		spaceCreateInfo.referenceSpaceType = mSceneSpaceType;
		spaceCreateInfo.poseInReferenceSpace = PoseIdentity();
		auto Result = xrCreateReferenceSpace( mSession, &spaceCreateInfo, &mSceneSpace );
		IsOkay(Result,"xrCreateReferenceSpace");
	}
	/*
	// Create a space for each hand pointer pose.
	for (uint32_t side : {LeftSide, RightSide})
	{
		XrActionSpaceCreateInfo createInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
		createInfo.action = mPoseAction;
		createInfo.poseInActionSpace = xr::math::Pose::Identity();
		createInfo.subactionPath = mSubactionPaths[side];
		auto Result = xrCreateActionSpace( mSession, &createInfo, mCubesInHand[side].Space );
		IsOkay(Result,"xrCreateActionSpace");
	}
	*/
}

/*
std::tuple<DXGI_FORMAT, DXGI_FORMAT> SelectSwapchainPixelFormats() {
	CHECK(m_session.Get() != XR_NULL_HANDLE);
	
	// Query runtime preferred swapchain formats.
	uint32_t swapchainFormatCount;
	CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session.Get(), 0, &swapchainFormatCount, nullptr));
	
	std::vector<int64_t> swapchainFormats(swapchainFormatCount);
	CHECK_XRCMD(xrEnumerateSwapchainFormats(
											m_session.Get(), (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));
	
	// Choose the first runtime preferred format that this app supports.
	auto SelectPixelFormat = [](const std::vector<int64_t>& runtimePreferredFormats,
								const std::vector<DXGI_FORMAT>& applicationSupportedFormats) {
		auto found = std::find_first_of(std::begin(runtimePreferredFormats),
										std::end(runtimePreferredFormats),
										std::begin(applicationSupportedFormats),
										std::end(applicationSupportedFormats));
		if (found == std::end(runtimePreferredFormats)) {
			THROW("No runtime swapchain format is supported.");
		}
		return (DXGI_FORMAT)*found;
	};
	
	DXGI_FORMAT colorSwapchainFormat = SelectPixelFormat(swapchainFormats, m_graphicsPlugin->SupportedColorFormats());
	DXGI_FORMAT depthSwapchainFormat = SelectPixelFormat(swapchainFormats, m_graphicsPlugin->SupportedDepthFormats());
	
	return {colorSwapchainFormat, depthSwapchainFormat};
}
*/

void Openxr::TSession::EnumViewConfigurations(XrViewConfigurationType ViewType,ArrayBridge< XrViewConfigurationView>&& Views)
{
	//	get count
	uint32_t ViewCount = 0;
	auto Result = xrEnumerateViewConfigurationViews(mInstance, mSystemId, ViewType, 0, &ViewCount, nullptr);
	IsOkay(Result, "xrEnumerateViewConfigurationViews (Count)");

//	if (viewCount != mStereoViewCount)
	//	throw Soy::AssertException(std::string("xrEnumerateViewConfigurationViews expected 2 views for stereo, got ") + std::to_string(viewCount));

	//	alloc buffer
	Views.SetSize(ViewCount);
	Result = xrEnumerateViewConfigurationViews(mInstance, mSystemId, mPrimaryViewConfigType, ViewCount, &ViewCount, Views.GetArray());
	IsOkay(Result, "xrEnumerateViewConfigurationViews (Get)");
}


void Openxr::TSession::CreateSwapchains()
{
	//mRenderResources = std::make_unique<RenderResources>();
	
	// Read graphics properties for preferred swapchain length and logging.
	XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
	auto Result = xrGetSystemProperties( mInstance, mSystemId, &systemProperties );
	IsOkay(Result,"xrGetSystemProperties");
	
	Array< XrViewConfigurationView> Views;
	EnumViewConfigurations(this->mPrimaryViewConfigType, GetArrayBridge(Views));

	/*

	// Select color and depth swapchain pixel formats
	const auto [colorSwapchainFormat, depthSwapchainFormat] = SelectSwapchainPixelFormats();
	// Using texture array for better performance, but requiring left/right views have identical sizes.
	const XrViewConfigurationView& view = m_renderResources->ConfigViews[0];
	CHECK(m_renderResources->ConfigViews[0].recommendedImageRectWidth ==
		  m_renderResources->ConfigViews[1].recommendedImageRectWidth);
	CHECK(m_renderResources->ConfigViews[0].recommendedImageRectHeight ==
		  m_renderResources->ConfigViews[1].recommendedImageRectHeight);
	CHECK(m_renderResources->ConfigViews[0].recommendedSwapchainSampleCount ==
		  m_renderResources->ConfigViews[1].recommendedSwapchainSampleCount);
	
	// Use recommended rendering parameters for a balance between quality and performance
	const uint32_t imageRectWidth = view.recommendedImageRectWidth;
	const uint32_t imageRectHeight = view.recommendedImageRectHeight;
	const uint32_t swapchainSampleCount = view.recommendedSwapchainSampleCount;
	
	// Create swapchains with texture array for color and depth images.
	// The texture array has the size of viewCount, and they are rendered in a single pass using VPRT.
	const uint32_t textureArraySize = viewCount;
	m_renderResources->ColorSwapchain =
	CreateSwapchainD3D11(m_session.Get(),
						 colorSwapchainFormat,
						 imageRectWidth,
						 imageRectHeight,
						 textureArraySize,
						 swapchainSampleCount,
						 0,//createFlags,
						 XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT);
	
	m_renderResources->DepthSwapchain =
	CreateSwapchainD3D11(m_session.Get(),
						 depthSwapchainFormat,
						 imageRectWidth,
						 imageRectHeight,
						 textureArraySize,
						 swapchainSampleCount,
						 0,//createFlags,
						 XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	
	// Preallocate view buffers for xrLocateViews later inside frame loop.
	m_renderResources->Views.resize(viewCount, {XR_TYPE_VIEW});
	*/
}


/*

struct SwapchainD3D11;
SwapchainD3D11 CreateSwapchainD3D11(XrSession session,
									DXGI_FORMAT format,
									uint32_t width,
									uint32_t height,
									uint32_t arraySize,
									uint32_t sampleCount,
									XrSwapchainCreateFlags createFlags,
									XrSwapchainUsageFlags usageFlags) {
	SwapchainD3D11 swapchain;
	swapchain.Format = format;
	swapchain.Width = width;
	swapchain.Height = height;
	swapchain.ArraySize = arraySize;
	
	XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
	swapchainCreateInfo.arraySize = arraySize;
	swapchainCreateInfo.format = format;
	swapchainCreateInfo.width = width;
	swapchainCreateInfo.height = height;
	swapchainCreateInfo.mipCount = 1;
	swapchainCreateInfo.faceCount = 1;
	swapchainCreateInfo.sampleCount = sampleCount;
	swapchainCreateInfo.createFlags = createFlags;
	swapchainCreateInfo.usageFlags = usageFlags;
	
	CHECK_XRCMD(xrCreateSwapchain(session, &swapchainCreateInfo, swapchain.Handle.Put()));
	
	uint32_t chainLength;
	CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.Handle.Get(), 0, &chainLength, nullptr));
	
	swapchain.Images.resize(chainLength, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
	CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.Handle.Get(),
										   (uint32_t)swapchain.Images.size(),
										   &chainLength,
										   reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchain.Images.data())));
	
	return swapchain;
}
*/

bool Openxr::TSession::ProcessEvents()
{
	auto pollEvent = [&](XrEventDataBuffer& eventData)
	{
		eventData.type = XR_TYPE_EVENT_DATA_BUFFER;
		eventData.next = nullptr;
		auto Result = xrPollEvent(mInstance, &eventData);
		return Result == XR_SUCCESS;
	};

	auto OnSessionStateChanged = [&](const XrEventDataSessionStateChanged& StateEvent)
	{
		std::Debug << "Session State changed from " << magic_enum::enum_name(mSessionState) << " to " << magic_enum::enum_name(StateEvent.state) << std::endl;
		mSessionState = StateEvent.state;

		switch (mSessionState)
		{
			//	ready to begin session
		case XR_SESSION_STATE_READY:
		{
			XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
			sessionBeginInfo.primaryViewConfigurationType = mPrimaryViewConfigType;
			auto Result = xrBeginSession(mSession, &sessionBeginInfo);
			IsOkay(Result, "xrBeginSession");
			mSessionRunning = true;
		}
		break;

		case XR_SESSION_STATE_STOPPING:
		{
			mSessionRunning = false;
			auto Result = xrEndSession(mSession);
			IsOkay(Result, "xrEndSession");
			break;
		}

		case XR_SESSION_STATE_EXITING:
			// Do not attempt to restart because user closed this session.
			//*requestRestart = false;
			return false;

		case XR_SESSION_STATE_LOSS_PENDING:
			//*requestRestart = true;
			return false;

		default:
			throw Soy::AssertException( std::string("Unhandled new session state ") + std::string(magic_enum::enum_name(mSessionState)) );
		}
	};
	
	while ( true )
	{
		//	no more events
		XrEventDataBuffer eventData{};
		if ( !pollEvent(eventData) )
			return true;

		switch (eventData.type)
		{
			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			{
				bool RequestRestart = false;
				return false;
			}

			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				const auto stateEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(&eventData);
				if ( stateEvent.session != mSession )
					throw Soy::AssertException("Event session different from our session");
				OnSessionStateChanged(stateEvent);
				break;
			}
				
			case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
			case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
			default:
				std::Debug << "Ignoring event " << magic_enum::enum_name(eventData.type) << std::endl;
				break;
		}
	}
	return true;
}
/*
struct Hologram;
Hologram CreateHologram(const XrPosef& poseInScene, XrTime placementTime) const {
	Hologram hologram{};
	if (m_optionalExtensions.SpatialAnchorSupported) {
		// Anchors provide the best stability when moving beyond 5 meters, so if the extension is enabled,
		// create an anchor at given location and place the hologram at the resulting anchor space.
		XrSpatialAnchorCreateInfoMSFT createInfo{XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT};
		createInfo.space = m_sceneSpace.Get();
		createInfo.pose = poseInScene;
		createInfo.time = placementTime;
		
		XrResult result = m_extensions.xrCreateSpatialAnchorMSFT(
																 m_session.Get(), &createInfo, hologram.Anchor.Put(m_extensions.xrDestroySpatialAnchorMSFT));
		if (XR_SUCCEEDED(result)) {
			XrSpatialAnchorSpaceCreateInfoMSFT createSpaceInfo{XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT};
			createSpaceInfo.anchor = hologram.Anchor.Get();
			createSpaceInfo.poseInAnchorSpace = xr::math::Pose::Identity();
			CHECK_XRCMD(m_extensions.xrCreateSpatialAnchorSpaceMSFT(m_session.Get(), &createSpaceInfo, hologram.Cube.Space.Put()));
		} else if (result == XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT) {
			DEBUG_PRINT("Anchor cannot be created, likely due to lost positional tracking.");
		} else {
			CHECK_XRRESULT(result, "xrCreateSpatialAnchorMSFT");
		}
	} else {
		// If the anchor extension is not available, place it in the scene space.
		// This works fine as long as user doesn't move far away from scene space origin.
		XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
		createInfo.referenceSpaceType = m_sceneSpaceType;
		createInfo.poseInReferenceSpace = poseInScene;
		CHECK_XRCMD(xrCreateReferenceSpace(m_session.Get(), &createInfo, hologram.Cube.Space.Put()));
	}
	return hologram;
}

void PollActions() {
	// Get updated action states.
	std::vector<XrActiveActionSet> activeActionSets = {{m_actionSet.Get(), XR_NULL_PATH}};
	XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
	syncInfo.countActiveActionSets = (uint32_t)activeActionSets.size();
	syncInfo.activeActionSets = activeActionSets.data();
	CHECK_XRCMD(xrSyncActions(m_session.Get(), &syncInfo));
	
	// Check the state of the actions for left and right hands separately.
	for (uint32_t side : {LeftSide, RightSide}) {
		const XrPath subactionPath = m_subactionPaths[side];
		
		// Apply a tiny vibration to the corresponding hand to indicate that action is detected.
		auto ApplyVibration = [this, subactionPath] {
			XrHapticActionInfo actionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
			actionInfo.action = m_vibrateAction.Get();
			actionInfo.subactionPath = subactionPath;
			
			XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
			vibration.amplitude = 0.5f;
			vibration.duration = XR_MIN_HAPTIC_DURATION;
			vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
			CHECK_XRCMD(xrApplyHapticFeedback(m_session.Get(), &actionInfo, (XrHapticBaseHeader*)&vibration));
		};
		
		XrActionStateBoolean placeActionValue{XR_TYPE_ACTION_STATE_BOOLEAN};
		{
			XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
			getInfo.action = m_placeAction.Get();
			getInfo.subactionPath = subactionPath;
			CHECK_XRCMD(xrGetActionStateBoolean(m_session.Get(), &getInfo, &placeActionValue));
		}
		
		// When select button is pressed, place the cube at the location of corresponding hand.
		if (placeActionValue.isActive && placeActionValue.changedSinceLastSync && placeActionValue.currentState) {
			// Use the poses at the time when action happened to do the placement
			const XrTime placementTime = placeActionValue.lastChangeTime;
			
			// Locate the hand in the scene.
			XrSpaceLocation handLocation{XR_TYPE_SPACE_LOCATION};
			CHECK_XRCMD(xrLocateSpace(m_cubesInHand[side].Space.Get(), m_sceneSpace.Get(), placementTime, &handLocation));
			
			// Ensure we have tracking before placing a cube in the scene, so that it stays reliably at a physical location.
			if (!xr::math::Pose::IsPoseValid(handLocation)) {
				DEBUG_PRINT("Cube cannot be placed when positional tracking is lost.");
			} else {
				// Place a new cube at the given location and time, and remember output placement space and anchor.
				m_holograms.push_back(CreateHologram(handLocation.pose, placementTime));
			}
			
			ApplyVibration();
		}
		
		// This sample, when menu button is released, requests to quit the session, and therefore quit the application.
		{
			XrActionStateBoolean exitActionValue{XR_TYPE_ACTION_STATE_BOOLEAN};
			XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
			getInfo.action = m_exitAction.Get();
			getInfo.subactionPath = subactionPath;
			CHECK_XRCMD(xrGetActionStateBoolean(m_session.Get(), &getInfo, &exitActionValue));
			
			if (exitActionValue.isActive && exitActionValue.changedSinceLastSync && !exitActionValue.currentState) {
				CHECK_XRCMD(xrRequestExitSession(m_session.Get()));
				ApplyVibration();
			}
		}
	}
}

void RenderFrame() {
	CHECK(m_session.Get() != XR_NULL_HANDLE);
	
	XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
	XrFrameState frameState{XR_TYPE_FRAME_STATE};
	CHECK_XRCMD(xrWaitFrame(m_session.Get(), &frameWaitInfo, &frameState));
	
	XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
	CHECK_XRCMD(xrBeginFrame(m_session.Get(), &frameBeginInfo));
	
	// EndFrame can submit mutiple layers
	std::vector<XrCompositionLayerBaseHeader*> layers;
	
	// The projection layer consists of projection layer views.
	XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
	
	// Inform the runtime to consider alpha channel during composition
	// The primary display on Hololens has additive environment blend mode. It will ignore alpha channel.
	// But mixed reality capture has alpha blend mode display and use alpha channel to blend content to environment.
	layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	
	// Only render when session is visible. otherwise submit zero layers
	if (frameState.shouldRender) {
		// First update the viewState and views using latest predicted display time.
		{
			XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
			viewLocateInfo.viewConfigurationType = m_primaryViewConfigType;
			viewLocateInfo.displayTime = frameState.predictedDisplayTime;
			viewLocateInfo.space = m_sceneSpace.Get();
			
			// The output view count of xrLocateViews is always same as xrEnumerateViewConfigurationViews
			// Therefore Views can be preallocated and avoid two call idiom here.
			uint32_t viewCapacityInput = (uint32_t)m_renderResources->Views.size();
			uint32_t viewCountOutput;
			CHECK_XRCMD(xrLocateViews(m_session.Get(),
									  &viewLocateInfo,
									  &m_renderResources->ViewState,
									  viewCapacityInput,
									  &viewCountOutput,
									  m_renderResources->Views.data()));
			
			CHECK(viewCountOutput == viewCapacityInput);
			CHECK(viewCountOutput == m_renderResources->ConfigViews.size());
			CHECK(viewCountOutput == m_renderResources->ColorSwapchain.ArraySize);
			CHECK(viewCountOutput == m_renderResources->DepthSwapchain.ArraySize);
		}
		
		// Then render projection layer into each view.
		if (RenderLayer(frameState.predictedDisplayTime, layer)) {
			layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
		}
	}
	
	// Submit the composition layers for the predicted display time.
	XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
	frameEndInfo.displayTime = frameState.predictedDisplayTime;
	frameEndInfo.environmentBlendMode = m_environmentBlendMode;
	frameEndInfo.layerCount = (uint32_t)layers.size();
	frameEndInfo.layers = layers.data();
	CHECK_XRCMD(xrEndFrame(m_session.Get(), &frameEndInfo));
}

uint32_t AquireAndWaitForSwapchainImage(XrSwapchain handle) {
	uint32_t swapchainImageIndex;
	XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
	CHECK_XRCMD(xrAcquireSwapchainImage(handle, &acquireInfo, &swapchainImageIndex));
	
	XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
	waitInfo.timeout = XR_INFINITE_DURATION;
	CHECK_XRCMD(xrWaitSwapchainImage(handle, &waitInfo));
	
	return swapchainImageIndex;
}

void UpdateSpinningCube(XrTime predictedDisplayTime) {
	if (!m_mainCubeIndex) {
		// Initialize a big cube 1 meter in front of user.
		Hologram hologram = CreateHologram(xr::math::Pose::Translation({0, 0, -1}), predictedDisplayTime);
		hologram.Cube.Scale = {0.25f, 0.25f, 0.25f};
		m_holograms.push_back(std::move(hologram));
		m_mainCubeIndex = (uint32_t)m_holograms.size() - 1;
	}
	
	if (!m_spinningCubeIndex) {
		// Initialize a small cube and remember the time when animation is started.
		Hologram hologram = CreateHologram(xr::math::Pose::Translation({0, 0, -1}), predictedDisplayTime);
		hologram.Cube.Scale = {0.1f, 0.1f, 0.1f};
		m_holograms.push_back(std::move(hologram));
		m_spinningCubeIndex = (uint32_t)m_holograms.size() - 1;
		
		m_spinningCubeStartTime = predictedDisplayTime;
	}
	
	// Pause spinning cube animation when app lost 3D focus
	if (IsSessionFocused()) {
		auto convertToSeconds = [](XrDuration nanoSeconds) {
			using namespace std::chrono;
			return duration_cast<duration<float>>(duration<XrDuration, std::nano>(nanoSeconds)).count();
		};
		
		const XrDuration duration = predictedDisplayTime - m_spinningCubeStartTime;
		const float seconds = convertToSeconds(duration);
		const float angle = DirectX::XM_PIDIV2 * seconds; // Rotate 90 degrees per second
		const float radius = 0.5f;                        // Rotation radius in meters
		
		// Let spinning cube rotate around the main cube at y axis.
		XrPosef pose;
		pose.position = {radius * std::sin(angle), 0, radius * std::cos(angle)};
		pose.orientation = xr::math::Quaternion::RotationAxisAngle({0, 1, 0}, angle);
		m_holograms[m_spinningCubeIndex.value()].Cube.PoseInSpace = pose;
	}
}

bool RenderLayer(XrTime predictedDisplayTime, XrCompositionLayerProjection& layer) {
	const uint32_t viewCount = (uint32_t)m_renderResources->ConfigViews.size();
	
	if (!xr::math::Pose::IsPoseValid(m_renderResources->ViewState)) {
		DEBUG_PRINT("xrLocateViews returned an invalid pose.");
		return false; // Skip rendering layers if view location is invalid
	}
	
	std::vector<const sample::Cube*> visibleCubes;
	
	auto UpdateVisibleCube = [&](sample::Cube& cube) {
		if (cube.Space.Get() != XR_NULL_HANDLE) {
			XrSpaceLocation cubeSpaceInScene{XR_TYPE_SPACE_LOCATION};
			CHECK_XRCMD(xrLocateSpace(cube.Space.Get(), m_sceneSpace.Get(), predictedDisplayTime, &cubeSpaceInScene));
			
			// Update cubes location with latest space relation
			if (xr::math::Pose::IsPoseValid(cubeSpaceInScene)) {
				if (cube.PoseInSpace.has_value()) {
					cube.PoseInScene = xr::math::Pose::Multiply(cube.PoseInSpace.value(), cubeSpaceInScene.pose);
				} else {
					cube.PoseInScene = cubeSpaceInScene.pose;
				}
				visibleCubes.push_back(&cube);
			}
		}
	};
	
	UpdateSpinningCube(predictedDisplayTime);
	
	UpdateVisibleCube(m_cubesInHand[LeftSide]);
	UpdateVisibleCube(m_cubesInHand[RightSide]);
	
	for (auto& hologram : m_holograms) {
		UpdateVisibleCube(hologram.Cube);
	}
	
	m_renderResources->ProjectionLayerViews.resize(viewCount);
	if (m_optionalExtensions.DepthExtensionSupported) {
		m_renderResources->DepthInfoViews.resize(viewCount);
	}
	
	// Swapchain is acquired, rendered to, and released together for all views as texture array
	const SwapchainD3D11& colorSwapchain = m_renderResources->ColorSwapchain;
	const SwapchainD3D11& depthSwapchain = m_renderResources->DepthSwapchain;
	
	// Use the full range of recommended image size to achieve optimum resolution
	const XrRect2Di imageRect = {{0, 0}, {(int32_t)colorSwapchain.Width, (int32_t)colorSwapchain.Height}};
	CHECK(colorSwapchain.Width == depthSwapchain.Width);
	CHECK(colorSwapchain.Height == depthSwapchain.Height);
	
	const uint32_t colorSwapchainImageIndex = AquireAndWaitForSwapchainImage(colorSwapchain.Handle.Get());
	const uint32_t depthSwapchainImageIndex = AquireAndWaitForSwapchainImage(depthSwapchain.Handle.Get());
	
	// Prepare rendering parameters of each view for swapchain texture arrays
	std::vector<xr::math::ViewProjection> viewProjections(viewCount);
	for (uint32_t i = 0; i < viewCount; i++) {
		viewProjections[i] = {m_renderResources->Views[i].pose, m_renderResources->Views[i].fov, m_nearFar};
		
		m_renderResources->ProjectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
		m_renderResources->ProjectionLayerViews[i].pose = m_renderResources->Views[i].pose;
		m_renderResources->ProjectionLayerViews[i].fov = m_renderResources->Views[i].fov;
		m_renderResources->ProjectionLayerViews[i].subImage.swapchain = colorSwapchain.Handle.Get();
		m_renderResources->ProjectionLayerViews[i].subImage.imageRect = imageRect;
		m_renderResources->ProjectionLayerViews[i].subImage.imageArrayIndex = i;
		
		if (m_optionalExtensions.DepthExtensionSupported) {
			m_renderResources->DepthInfoViews[i] = {XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR};
			m_renderResources->DepthInfoViews[i].minDepth = 0;
			m_renderResources->DepthInfoViews[i].maxDepth = 1;
			m_renderResources->DepthInfoViews[i].nearZ = m_nearFar.Near;
			m_renderResources->DepthInfoViews[i].farZ = m_nearFar.Far;
			m_renderResources->DepthInfoViews[i].subImage.swapchain = depthSwapchain.Handle.Get();
			m_renderResources->DepthInfoViews[i].subImage.imageRect = imageRect;
			m_renderResources->DepthInfoViews[i].subImage.imageArrayIndex = i;
			
			// Chain depth info struct to the corresponding projection layer views's next
			m_renderResources->ProjectionLayerViews[i].next = &m_renderResources->DepthInfoViews[i];
		}
	}
	
	// For Hololens additive display, best to clear render target with transparent black color (0,0,0,0)
	constexpr DirectX::XMVECTORF32 opaqueColor = {0.184313729f, 0.309803933f, 0.309803933f, 1.000000000f};
	constexpr DirectX::XMVECTORF32 transparent = {0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f};
	const DirectX::XMVECTORF32 renderTargetClearColor =
	(m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) ? opaqueColor : transparent;
	
	m_graphicsPlugin->RenderView(imageRect,
								 renderTargetClearColor,
								 viewProjections,
								 colorSwapchain.Format,
								 colorSwapchain.Images[colorSwapchainImageIndex].texture,
								 depthSwapchain.Format,
								 depthSwapchain.Images[depthSwapchainImageIndex].texture,
								 visibleCubes);
	
	XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
	CHECK_XRCMD(xrReleaseSwapchainImage(colorSwapchain.Handle.Get(), &releaseInfo));
	CHECK_XRCMD(xrReleaseSwapchainImage(depthSwapchain.Handle.Get(), &releaseInfo));
	
	layer.space = m_sceneSpace.Get();
	layer.viewCount = (uint32_t)m_renderResources->ProjectionLayerViews.size();
	layer.views = m_renderResources->ProjectionLayerViews.data();
	return true;
}

void PrepareSessionRestart() {
	m_mainCubeIndex = m_spinningCubeIndex = {};
	m_holograms.clear();
	m_renderResources.reset();
	m_session.Reset();
	m_systemId = XR_NULL_SYSTEM_ID;
}

constexpr bool IsSessionFocused() const {
	return m_sessionState == XR_SESSION_STATE_FOCUSED;
}

XrPath GetXrPath(const char* string) const {
	return xr::StringToPath(m_instance.Get(), string);
}

*/
XrPath Openxr::TSession::GetXrPath(const char* PathString)
{
	XrPath Path = XR_NULL_PATH;
	auto Result = xrStringToPath( mInstance, PathString, &Path );
	IsOkay(Result, std::string("xrStringToPath ") + PathString );
	return Path;
}

bool Openxr::TSession::HasExtension(const char* MatchExtension)
{
	std::string MatchExtensionStr(MatchExtension);
	//	gr: this should check the ones that we explicitly enabled
	bool Matched = false;
	auto OnExtension = [&](const std::string& Name,uint32_t Version)
	{
		if (MatchExtensionStr != Name)
			return;
		Matched = true;
	};
	EnumExtensions(OnExtension);
	return Matched;
}

