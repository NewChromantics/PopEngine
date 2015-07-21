#pragma once
#include <SoyApp.h>
#include <TJob.h>
#include <TChannel.h>

#include "TFilter.h"


class TPopTrack : public TJobHandler, public TPopJobHandler, public TChannelManager
{
public:
	TPopTrack();
	
	virtual bool	AddChannel(std::shared_ptr<TChannel> Channel) override;

	void			OnExit(TJobAndChannel& JobAndChannel);
	void			OnLoadFrame(TJobAndChannel& JobAndChannel);
	void			OnAddStage(TJobAndChannel& JobAndChannel);
	void			OnSetFilterUniform(TJobAndChannel& JobAndChannel);
	void			OnRunFilter(TJobAndChannel& JobAndChannel);
	
private:
	std::shared_ptr<TPlayerFilter>	GetFilter(const std::string& Name);
	
public:
	Soy::Platform::TConsoleApp	mConsoleApp;
	
	Array<std::shared_ptr<TPlayerFilter>>	mFilters;
};



