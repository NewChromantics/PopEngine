#pragma once
#include <ofxSoylent.h>
#include <SoyApp.h>
#include <TJob.h>
#include <TChannel.h>
#include "SoyOpenglContext.h"

class TTextureWindow;




class TPopOpengl : public TJobHandler, public TPopJobHandler, public TChannelManager
{
public:
	TPopOpengl();
	
	virtual bool	AddChannel(std::shared_ptr<TChannel> Channel) override;

	void			OnExit(TJobAndChannel& JobAndChannel);
	void			OnMakeWindow(TJobAndChannel& JobAndChannel);
	void			OnMakeRenderTarget(TJobAndChannel& JobAndChannel);
	void			OnClearRenderTarget(TJobAndChannel& JobAndChannel);
	
private:
	Opengl::TContext*	GetContext(const std::string& Name);
	std::shared_ptr<Opengl::TRenderTargetFbo>	GetRenderTarget(const std::string& Name);
	
public:
	Soy::Platform::TConsoleApp	mConsoleApp;
	
	Array<std::shared_ptr<TTextureWindow>>	mWindows;
	Array<std::shared_ptr<Opengl::TRenderTargetFbo>>	mRenderTargets;	//
};



