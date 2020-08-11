#include "SoyOpenxr.h"
#include <SoyThread.h>
#include <openxr/openxr.h>

namespace Openxr
{

	class TSession;
}



class Openxr::TSession : public SoyThread
{
public:
	TSession();
	~TSession();
	
	virtual bool	ThreadIteration() override;
};

Openxr::TSession::TSession() :
	SoyThread	( "Openxr::TSession" )
{
	//https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/3579c5fcc123524a545e6fd361e76b7f819aa8a3/src/tests/hello_xr/main.cpp
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
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	return true;
}

//	https://github.com/microsoft/OpenXR-MixedReality/blob/master/samples/BasicXrApp/OpenXrProgram.cpp
