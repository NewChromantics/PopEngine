#include "TApiHttp.h"
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
	DEFINE_BIND_FUNCTIONNAME(Disconnect);

	DEFINE_BIND_TYPENAME(HttpClient);
	DEFINE_BIND_FUNCTIONNAME(WaitForBody);
}

void ApiHttp::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<THttpServerWrapper>(Namespace,"Server");
	Context.BindObjectType<THttpClientWrapper>(Namespace,"Client");
}



void THttpServerWrapper::Construct(Bind::TCallback& Params)
{
	auto& This = Params.This<THttpServerWrapper>();

	auto ListenPort = Params.GetArgumentInt(0);

	if (Params.IsArgumentFunction(1))
	{
		auto HandleVirtualFileFunc = Params.GetArgumentFunction(1);
		mHandleVirtualFile = Bind::TPersistent(Params.mLocalContext, HandleVirtualFileFunc, "HTTP server HandleVirtualFileFunction");
	}

	auto OnRequest = [this](std::string& Url,Http::TResponseProtocol& Response)
	{
		this->OnRequest(Url,Response);
	};
	This.mSocket.reset( new THttpServer( ListenPort, OnRequest ) );
}


void THttpServerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiHttp::BindFunction::GetAddress>( &ApiSocket::TSocketWrapper::GetAddress );
	Template.BindFunction<ApiHttp::BindFunction::Send>(&ApiSocket::TSocketWrapper::Send );
	Template.BindFunction<ApiHttp::BindFunction::GetPeers>(&ApiSocket::TSocketWrapper::GetPeers );
	Template.BindFunction<ApiHttp::BindFunction::Disconnect>( &ApiSocket::TSocketWrapper::Disconnect );
}


void THttpServerWrapper::HandleMissingFile(std::string& Url, Http::TResponseProtocol& Response,bool CallOverload)
{
	//	init response
	Response.SetContent(Url);
	Response.mResponseCode = Http::Response_FileNotFound;
	Response.mResponseString = "File Not Found";

	//	there's a handler func, call it, and handle the result
	if (!mHandleVirtualFile )
		return;
	if (!CallOverload)
		return;

	std::string RedirectedFilename;

	//	call overload, wait for it to finish and fill in the response
	auto CallOverloadResponse = [&](Bind::TLocalContext& Context)
	{
		std::string _Content("Content");

		//	make an object with the meta
		auto ResponseObject = Context.mGlobalContext.CreateObjectInstance(Context);
		ResponseObject.SetString("Url", Url);
		ResponseObject.SetString("Mime", Response.mContentMimeType);
		ResponseObject.SetInt("StatusCode", Response.mResponseCode);
		ResponseObject.SetNull(_Content);

		//	call func
		Bind::TCallback Call(Context);
		auto Func = this->mHandleVirtualFile.GetFunction(Context);
		Call.SetArgumentObject(0, ResponseObject);
		Func.Call(Call);

		if (!Call.IsReturnUndefined())
		{
			auto ReturnFilename = Call.GetReturnString();
			RedirectedFilename = Context.mGlobalContext.GetResolvedFilename(ReturnFilename);
			return;
		}

		//	read out modified data
		auto Mime = ResponseObject.GetString("Mime");
		//Response.mContentMimeType = ResponseObject.GetString("Mime");
		Response.mResponseCode = ResponseObject.GetInt("StatusCode");

		if (ResponseObject.IsMemberArray(_Content))
		{
			Array<uint8_t> Content;
			ResponseObject.GetArray(_Content, GetArrayBridge(Content));
			Response.SetContent(GetArrayBridge(Content), Mime);
		}
		else
		{
			auto Content = ResponseObject.GetString(_Content);
			Response.SetContent(Content, Mime);
		}
	};
	auto& Context = mHandleVirtualFile.GetContext();
	try
	{
		Context.Execute(CallOverloadResponse);
		if (!RedirectedFilename.empty())
		{
			HandleFile(RedirectedFilename, Response);
		}
	}
	catch (std::exception& Error)
	{
		Response.mResponseString = Error.what();
		Response.mResponseCode = Http::Response_Error;
	}
}


void THttpServerWrapper::OnRequest(std::string& Url, Http::TResponseProtocol& Response)
{
	//	gr: have a specific case where we want to serve files from a different dir, but it contained a file
	//		(bootup.js) that was ALSO in our context path, so sent the wrong one
	//		so if user has provided an overload... ALWAYS call it.
	if ( mHandleVirtualFile )
	{
		HandleMissingFile(Url, Response,true);
		return;
	}
	
	//	todo: make the response a function that we can defer to other threads
	//	then we can make callbacks for certain urls in javascript for dynamic replies
	auto Filename = GetContext().GetResolvedFilename(Url);
	
	if (!Platform::FileExists(Filename))
	{
		HandleMissingFile(Url, Response,true);
		return;
	}

	HandleFile(Filename, Response);

}

void THttpServerWrapper::HandleFile(std::string& Filename, Http::TResponseProtocol& Response)
{
	if (!Platform::FileExists(Filename))
	{
		HandleMissingFile(Filename, Response,false);
		return;
	}

	//	todo: get mime type and do binary vs text properly
	std::string Contents;
	Soy::FileToString(Filename, Contents);

	//	get mimetype from extension
	std::string Extension;
	auto GetLastPart = [&](const std::string& Part, char SplitChar)
	{
		Extension = Part;
		return true;
	};
	Soy::StringSplitByMatches(GetLastPart, Filename, ".");
	std::string MimeType = "application/octet-stream";
	try
	{
		auto Format = SoyMediaFormat::FromExtension(Extension);
		MimeType = SoyMediaFormat::ToMime(Format);
	}
	catch (std::exception& e)
	{
		std::Debug << "Error getting mime from extension " << Extension << "; " << e.what() << std::endl;
	}
	Response.SetContent(Contents, MimeType);
	Response.mResponseCode = Http::Response_OK;
	Response.mResponseString = Http::GetDefaultResponseString(Response.mResponseCode);
}


THttpServer::THttpServer(uint16_t ListenPort,std::function<void(std::string&,Http::TResponseProtocol&)> OnRequest) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"HttpServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep )
{
	mSocket.reset( new SoySocket() );
	//mSocket->CreateTcp(true);
	mSocket->ListenTcp(ListenPort);
	
	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		AddClient( ClientRef, OnRequest );
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef,const std::string& Reason)
	{
		//	need to work out if OUR socket has closed
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




THttpClient::THttpClient(const std::string& Url,std::function<void(Http::TResponseProtocol&)> OnResponse,std::function<void(const std::string&)> OnError) :
	mOnResponse	( OnResponse ),
	mOnError	( OnError )
{
	//	start a thread
	mOnError("Todo");
}

void THttpClientWrapper::Construct(Bind::TCallback& Params)
{
	auto Url = Params.GetArgumentString(0);

	auto OnResponse = [this](Http::TResponseProtocol& Response)
	{
		this->mBodyPromises.Push(Response);
	};
	
	auto OnError = [this](const std::string& Error)
	{
		std::Debug << "Need to get rejections into promise queues; " << Error << std::endl;
		//this->mBodyPromises.Reject(Error);
	};

	mSocket.reset( new THttpClient( Url, OnResponse, OnError ) );
}



void THttpClientWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiHttp::BindFunction::WaitForBody>( &THttpClientWrapper::WaitForBody );
}

void THttpClientWrapper::WaitForBody(Bind::TCallback& Params)
{
	auto Promise = mBodyPromises.AddPromise( Params.mLocalContext );
	Params.Return(Promise);
}

