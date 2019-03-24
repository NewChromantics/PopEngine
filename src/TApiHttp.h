#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"

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
	DECLARE_BIND_TYPENAME(Http);
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



class THttpServerWrapper: public Bind::TObjectWrapper<ApiHttp::Http_TypeName,THttpServer>, public TSocketWrapper
{
public:
	THttpServerWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
	{
	}
	
	static void						CreateTemplate(Bind::TTemplate& Template);
	virtual void 					Construct(Bind::TCallback& Arguments) override;

	virtual std::shared_ptr<SoySocket>	GetSocket() override	{	return mSocket ? mSocket->mSocket : nullptr;	}

	void							OnRequest(std::string& Url,Http::TResponseProtocol& Response);
	
public:
	std::shared_ptr<THttpServer>&	mSocket = mObject;
};
