#include "TApiHttp.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoySocket.h"
#include "SoyFilesystem.h"
#include "SoyMediaFormat.h"

namespace ApiHttp
{
	const char Namespace[] = "Pop.Http";

	DEFINE_BIND_TYPENAME(HttpServer);
	DEFINE_BIND_FUNCTIONNAME(GetAddress);
	DEFINE_BIND_FUNCTIONNAME(Send);
	DEFINE_BIND_FUNCTIONNAME(GetPeers);
}

void ApiHttp::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<THttpServerWrapper>(Namespace,"Server");
}



void THttpServerWrapper::Construct(Bind::TCallback& Params)
{
	auto& This = Params.This<THttpServerWrapper>();

	auto ListenPort = Params.GetArgumentInt(0);
	auto OnRequest = [this](std::string& Url,Http::TResponseProtocol& Response)
	{
		this->OnRequest(Url,Response);
	};
	This.mSocket.reset( new THttpServer( ListenPort, OnRequest ) );
}


void THttpServerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiHttp::GetAddress_FunctionName>( GetAddress );
	Template.BindFunction<ApiHttp::Send_FunctionName>( Send );
	Template.BindFunction<ApiHttp::GetPeers_FunctionName>( GetPeers );
}


void THttpServerWrapper::OnRequest(std::string& Url,Http::TResponseProtocol& Response)
{
	//	todo: make the response a function that we can defer to other threads
	//	then we can make callbacks for certain urls in javascript for dynamic replies
	auto Filename = mContext.GetResolvedFilename(Url);
	
	//	todo: get mime type and do binary vs text properly
	std::string Contents;
	Soy::FileToString( Filename, Contents );
	
	//	get mimetype from extension
	std::string Extension;
	auto GetLastPart = [&](const std::string& Part,char SplitChar)
	{
		Extension = Part;
		return true;
	};
	Soy::StringSplitByMatches(GetLastPart, Url, "." );
	auto Format = SoyMediaFormat::FromExtension(Extension);
	auto MimeType = SoyMediaFormat::ToMime(Format);
	Response.SetContent( Contents, Format );
}


THttpServer::THttpServer(uint16_t ListenPort,std::function<void(std::string&,Http::TResponseProtocol&)> OnRequest) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"HttpServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep )
{
	mSocket.reset( new SoySocket() );
	mSocket->CreateTcp(true);
	mSocket->ListenTcp(ListenPort);
	
	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		AddClient( ClientRef, OnRequest );
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef)
	{
		RemoveClient(ClientRef);
	};
	
	Start();
	
}
	

bool THttpServer::Iteration()
{
	if ( !mSocket )
		return false;
	
	if ( !mSocket->IsCreated() )
		return true;
	
	//	non blocking so lets just do everything in a loop
	auto NewClient = mSocket->WaitForClient();
	if ( NewClient.IsValid() )
		std::Debug << "new client connected: " << NewClient << std::endl;
	/*
	Array<char> RecvBuffer;
	auto RecvFromConnection = [&](SoyRef ClientRef,SoySocketConnection ClientCon)
	{
		RecvBuffer.Clear();
		ClientCon.Recieve( GetArrayBridge(RecvBuffer) );
		if ( !RecvBuffer.IsEmpty() )
		{
			auto Client = GetClient(ClientRef);
			Client->OnRecvData( GetArrayBridge(RecvBuffer) );
		}
	};
	mSocket->EnumConnections( RecvFromConnection );
	*/
	return true;
}


std::shared_ptr<THttpServerPeer> THttpServer::GetClient(SoyRef ClientRef)
{
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	for ( int c=0;	c<mClients.GetSize();	c++ )
	{
		auto& pClient = mClients[c];
		if ( pClient->mConnectionRef == ClientRef )
			return pClient;
	}

	throw Soy::AssertException("Client not found");
}

void THttpServer::AddClient(SoyRef ClientRef,std::function<void(std::string&,Http::TResponseProtocol&)> OnRequest)
{
	std::shared_ptr<THttpServerPeer> Client( new THttpServerPeer( mSocket, ClientRef, OnRequest ) );
	std::lock_guard<std::recursive_mutex> Lock(mClientsLock);
	mClients.PushBack(Client);
}

void THttpServer::RemoveClient(SoyRef ClientRef)
{
	
}


void THttpServerPeer::OnDataRecieved(std::shared_ptr<Http::TRequestProtocol>& pData)
{
	auto& Data = *pData;
	
	//	send file back
	//	todo: let JS register url callbacks
	if ( Data.mUrl.empty() )
		Data.mUrl = "index.html";
	
	auto pResponse = std::make_shared<Http::TResponseProtocol>();
	auto& Response = *pResponse;
	try
	{
		mOnRequest( Data.mUrl, Response );
	}
	catch(std::exception& e)
	{
		std::stringstream ResponseMessage;
		ResponseMessage << "Exception fetching " << Data.mUrl << ": " << e.what();
		Response.SetContent( ResponseMessage.str() );
		Response.mResponseCode = Http::Response_Error;
		Response.mResponseString = "Exception";
	}

	Push( pResponse );
}


