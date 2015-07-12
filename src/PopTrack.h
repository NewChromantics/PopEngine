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
	
private:
	std::shared_ptr<TFilter>	GetFilter(const std::string& Name);
	
public:
	Soy::Platform::TConsoleApp	mConsoleApp;
	
	Array<std::shared_ptr<TFilter>>	mFilters;
};



