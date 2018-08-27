#pragma once

#include "v8-inspector.h"
#include "TApiWebsocket.h"
#include "TApiHttp.h"




using namespace v8_inspector;


class ChannelImpl : public V8Inspector::Channel
{
public:
	ChannelImpl(std::function<void(const std::string&)> OnResponse) :
		mOnResponse	( OnResponse )
	{
	}
	
	virtual void sendResponse(int callId,std::unique_ptr<StringBuffer> message) override;
	virtual void sendNotification(std::unique_ptr<StringBuffer> message) override;
	virtual void flushProtocolNotifications() override;
	
	std::function<void(const std::string&)>	mOnResponse;
};


class TV8Inspector : public V8InspectorClient
{
public:
	TV8Inspector(TV8Container& Container);
	
	std::string		GetChromeDebuggerUrl();
	std::string		GetWebsocketAddress();

protected:
	void	OnMessage(SoyRef Connection,const std::string& Message);
	void	OnMessage(SoyRef Connection,const Array<uint8_t>& Message);
	
	void	SendResponse(const std::string& Message);
	
	void 	OnDiscoveryRequest(const std::string& Url,Http::TResponseProtocol& Request);
	
	virtual void runMessageLoopOnPause(int contextGroupId) override;
	virtual void quitMessageLoopOnPause() override;

	
private:
	TV8Container&	mContainer;
	std::shared_ptr<THttpServer>		mDiscoveryServer;	//	chrome connects to this to find inspectors
	std::shared_ptr<TWebsocketServer>	mWebsocketServer;
	std::string		mUuid;
	
	std::unique_ptr<V8Inspector>			mInspector;
	std::shared_ptr<V8Inspector::Channel>	mChannel;
	std::unique_ptr<V8InspectorSession>		mSession;
	
	SoyRef			mClient;		//	need a better way to relay this around? or... maybe see if the id's are unique per client
};


