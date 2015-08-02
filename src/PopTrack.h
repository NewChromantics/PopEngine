#pragma once
#include <SoyApp.h>
#include <TJob.h>
#include <TChannel.h>
#include <TJobEventSubscriber.h>

#include "TPlayerFilter.h"
#include "SoyMovieDecoder.h"



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
	
	void			OnStartDecode(TJobAndChannel& JobAndChannel);
	void			OnList(TJobAndChannel& JobAndChannel);
	void			OnGetFrame(TJobAndChannel& JobAndChannel);
	void			SubscribeNewFrame(TJobAndChannel& JobAndChannel);
	bool			OnNewFrameCallback(TEventSubscriptionManager& SubscriptionManager,TJobChannelMeta Client,TVideoDevice& Device);

	
private:
	std::shared_ptr<TPlayerFilter>	GetFilter(const std::string& Name);

	std::shared_ptr<TMovieDecoderContainer>	mMovies;
	SoyVideoCapture		mVideoCapture;
	TSubscriberManager	mSubcriberManager;
	
public:
	Soy::Platform::TConsoleApp	mConsoleApp;
	
	Array<std::shared_ptr<TPlayerFilter>>	mFilters;
};



