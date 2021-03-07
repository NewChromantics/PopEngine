#pragma once
#include "TBind.h"

//#include "TPopServerThread.h"
#include "SoyRef.h"
#include "SoyStream.h"
#include "SoyWebSocket.h"
#include "SoyHttp.h"
#include "SoySocketStream.h"
#include "TApiSocket.h"



class SoySocket;

namespace ApiHttp
{
	void	Bind(Bind::TContext& Context);
	DECLARE_BIND_TYPENAME(HttpServer);
	DECLARE_BIND_TYPENAME(HttpClient);
}




class THttpServerPeer : public TSocketReadThread_Impl<Http::TRequestProtocol>, TSocketWriteThread
{
public:
	THttpServerPeer(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef,std::function<void(std::string&,Http::TResponseProtocol&)> OnRequest) :
		TSocketReadThread_Impl	( Socket, ConnectionRef ),
		TSocketWriteThread		( Socket, ConnectionRef ),
		mConnectionRef			( ConnectionRef ),
		mOnRequest				( OnRequest )
	{
		TSocketReadThread_Impl::Start();
		TSocketWriteThread::Start();
	}
	
	virtual void		OnDataRecieved(std::shared_ptr<Http::TRequestProtocol>& Data) override;
	
public:
	std::function<void(std::string&,Http::TResponseProtocol&)>	mOnRequest;
	SoyRef				mConnectionRef;
};


class THttpServer : public SoyWorkerThread
{
public:
	THttpServer(uint16_t ListenPort,std::function<void(std::string&,Http::TResponseProtocol&)> OnRequest);
	
	std::string					GetAddress() const;
	
protected:
	virtual bool				Iteration() override;
	
	void						AddClient(SoyRef ClientRef,std::function<void(std::string&,Http::TResponseProtocol&)> OnRequest);
	void						RemoveClient(SoyRef ClientRef);
	std::shared_ptr<THttpServerPeer>	GetClient(SoyRef ClientRef);
	
public:
	std::shared_ptr<SoySocket>		mSocket;
	
protected:
	std::recursive_mutex			mClientsLock;
	Array<std::shared_ptr<THttpServerPeer>>	mClients;
	
	std::function<void(std::string&,Http::TResponseProtocol&)>	mOnRequest;
};



class THttpServerWrapper: public Bind::TObjectWrapper<ApiHttp::BindType::HttpServer,THttpServer>, public ApiSocket::TSocketWrapper
{
public:
	THttpServerWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context )
	{
	}
	
	static void						CreateTemplate(Bind::TTemplate& Template);
	virtual void 					Construct(Bind::TCallback& Arguments) override;

	virtual std::shared_ptr<SoySocket>	GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

	void							OnRequest(std::string& Url,Http::TResponseProtocol& Response);
	
protected:
	void							HandleMissingFile(std::string& Url, Http::TResponseProtocol& Response,bool CallOverload);
	void							HandleFile(std::string& Filename, Http::TResponseProtocol& Response);

public:
	Bind::TPersistent				mHandleVirtualFile;
	std::shared_ptr<THttpServer>&	mSocket = mObject;
};



class THttpConnection;

class THttpClient //: public SoyWorkerThread
{
public:
	THttpClient(const std::string& Url,std::function<void(std::shared_ptr<Http::TResponseProtocol>&)> OnResponse,std::function<void(const std::string&)> OnError);
	
	std::shared_ptr<THttpConnection>	mFetchThread;
};


class THttpClientWrapper: public Bind::TObjectWrapper<ApiHttp::BindType::HttpClient,THttpClient>
{
public:
	THttpClientWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context )
	{
	}
	
	static void						CreateTemplate(Bind::TTemplate& Template);
	virtual void 					Construct(Bind::TCallback& Arguments) override;
	
	void							WaitForBody(Bind::TCallback& Arguments);

public:
	Bind::TPromiseQueueObjects<std::shared_ptr<Http::TResponseProtocol>>	mBodyPromises;
	std::shared_ptr<THttpClient>&	mSocket = mObject;
};
