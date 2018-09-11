#include "TV8Inspector.h"
#include <SoyJson.h>
#include <SoyFilesystem.h>

using namespace v8_inspector;

class TStringViewContainer
{
public:
	TStringViewContainer(const std::string& Text) :
		mString	( Text )
	{
	}
	
	StringView	GetStringView()
	{
		auto* CStr8 = reinterpret_cast<const uint8_t*>(mString.c_str());
		return StringView( CStr8, mString.length() );
	}
	
	std::string	mString;
	StringView	mView;
};

std::string GetString(const StringView& String)
{
	std::stringstream StringOut;
	auto* Chars = String.characters8();
	auto CharStep = String.is8Bit() ? 1 : 2;
	for ( int i=0;	i<String.length();	i++ )
	{
		char c = Chars[ i * CharStep ];
		StringOut << c;
	}
	return StringOut.str();
}

std::string GetString(StringBuffer& String)
{
	return GetString( String.string() );
}


void ChannelImpl::sendResponse(int callId,std::unique_ptr<StringBuffer> message)
{
	/*
	 Chrome tools message: {"id":1,"method":"Network.enable","params":{"maxPostDataSize":65536}}
	 Chrome tools message: {"id":2,"method":"Page.enable"}
	 Chrome tools message: {"id":3,"method":"Page.getResourceTree"}
	 Chrome tools message: {"id":4,"method":"Profiler.enable"}
	 Chrome tools message: {"id":5,"method":"Runtime.enable"}
	 Chrome tools message: {"id":6,"method":"Debugger.enable"}
	 Chrome tools message: {"id":7,"method":"Debugger.setPauseOnExceptions","params":{"state":"none"}}
	 Chrome tools message: {"id":8,"method":"Debugger.setAsyncCallStackDepth","params":{"maxDepth":32}}
	 */
	auto MessageStdString = GetString(*message);
	std::Debug << "Channel response: " << MessageStdString << std::endl;
	
	mOnResponse( MessageStdString );
	/*
	//	from inspector-test.cc and taskrunner...
	//	send this to the context as a job
	auto MessageString = message->string();
	auto MessageStringData = GetRemoteArray( MessageString.characters16(), MessageString.length() );
	Array<uint16_t> Message16;
	Message16.Copy(MessageStringData);
	
	auto Job = [=](v8::Local<v8::Context> Context)
	{
		auto* Isolate = Context->GetIsolate();
		auto MessageStringv8 = v8::String::NewFromTwoByte( Isolate, Message16.GetArray(), v8::NewStringType::kNormal, Message16.GetSize() );
		//.ToLocalChecked();
		
		auto This = Context->Global();
		//result = channel_->function_.Get(data->isolate())->Call(context, This, 1, &MessageStringv8 );
	};
	mContainer.QueueScoped(Job);
	 */
/*
	v8::MicrotasksScope microtasks_scope(data->isolate(),
										 v8::MicrotasksScope::kRunMicrotasks);
	v8::HandleScope handle_scope(data->isolate());
	v8::Local<v8::Context> context =
	data->GetContext(channel_->context_group_id_);
	v8::Context::Scope context_scope(context);
	v8::Local<v8::Value> message = ToV8String(data->isolate(), message_);
	v8::MaybeLocal<v8::Value> result;
	result = channel_->function_.Get(data->isolate())
	->Call(context, context->Global(), 1, &message);
	allId,
	std::unique_ptr<v8_inspector::StringBuffer> message) override {
		task_runner_->Append(
							 new SendMessageTask(this, ToVector(message->string())));
*/
}

void ChannelImpl::sendNotification(std::unique_ptr<StringBuffer> message)
{
	sendResponse( -1, std::move(message) );
}

void ChannelImpl::flushProtocolNotifications()
{
	std::Debug << "flushProtocolNotifications" << std::endl;
	/*
	 f (disposed_)
	 return;
	 for (size_t i = 0; i < agents_.size(); i++)
	 agents_[i]->FlushPendingProtocolNotifications();
	 if (!notification_queue_.size())
	 return;
	 v8_session_state_json_.Set(ToCoreString(v8_session_->stateJSON()));
	 for (size_t i = 0; i < notification_queue_.size(); ++i) {
	 client_->SendProtocolNotification(session_id_,
	 notification_queue_[i]->Serialize(),
	 session_state_.TakeUpdates());
	 }
	 notification_queue_.clear();
	 */
}


TV8Inspector::TV8Inspector(TV8Container& Container) :
	mContainer	( Container ),
	mUuid		( "f00df00d-f00d-f00d-f00d-f00df00df00d" )
{
	auto& Isolate = mContainer.GetIsolate();
	auto Context = mContainer.mContext->GetLocal(Isolate);

	//	https://medium.com/@hyperandroid/v8-inspector-from-an-embedder-standpoint-7f9c0472e2b7
	mInspector = V8Inspector::create( &Isolate, this );
	
	auto DoSendResponse = [this](const std::string& Message)
	{
		SendResponse(Message);
	};
	
	auto ContextGroupId = 1;

	// make sure you register Context objects in the V8Inspector.
	// ctx_name will be shown in CDT/console. Call this for each context
	// your app creates.
	TStringViewContainer ContextName("PopEngineContextName");
	V8ContextInfo ContextInfo( Context, ContextGroupId, ContextName.GetStringView() );
	mInspector->contextCreated(ContextInfo);

	
	// create a v8 channel.
	// ChannelImpl : public v8_inspector::V8Inspector::Channel
	mChannel.reset( new ChannelImpl(DoSendResponse) );
	
	TStringViewContainer State("{}");

	// Create a debugging session by connecting the V8Inspector
	// instance to the channel
	mSession = mInspector->connect( ContextGroupId, mChannel.get(), State.GetStringView() );
	
	
	auto BreakOnStart = false;
	if ( BreakOnStart )
	{
		mSession->schedulePauseOnNextStatement(TStringViewContainer("Break on start").GetStringView(),TStringViewContainer("Just do it").GetStringView());
	}

							   
	/*
	auto ChromeDevToolsPort = 9229;

	auto OnWebRequest = [this](std::string& Url,Http::TResponseProtocol& Response)
	{
		this->OnDiscoveryRequest( Url, Response );
	};
	mDiscoveryServer.reset( new THttpServer(ChromeDevToolsPort,OnWebRequest) );
	*/
	
	
	auto OnTextMessage = [this](SoyRef Connection,const std::string& Message)
	{
		this->OnMessage( Connection, Message );
	};
	auto OnBinaryMessage = [this](SoyRef Connection,const Array<uint8_t>& Message)
	{
		this->OnMessage( Connection, Message );
	};
	
	auto InspectorPorts = {8008,0};
	for ( auto InspectorPort : InspectorPorts )
	{
		try
		{
			mWebsocketServer.reset( new TWebsocketServer(InspectorPort,OnTextMessage,OnBinaryMessage) );
			break;
		}
		catch(std::exception& e)
		{
			std::Debug << e.what() << std::endl;
		}
	}
	if ( !mWebsocketServer )
		throw Soy::AssertException("Failed to open websocket server");

	std::stringstream OpenUrl;
	OpenUrl << "http://" << GetChromeDebuggerUrl();
	auto str = OpenUrl.str();
	Platform::ShellOpenUrl(str);
	std::Debug << "Open chrome inspector: " << GetChromeDebuggerUrl() << std::endl;
						   /*
	std::stringstream OpenUrl;
	OpenUrl << "open -a \"Google Chrome\" 'http://" << GetChromeDebuggerUrl() << "'";
	system( OpenUrl.str().c_str() );
	Platform::ShellExecute(OpenUrl.str());
	*/

}

std::string TV8Inspector::GetChromeDebuggerUrl()
{
	//	lets skip the auto stuff for now!
	//	chrome-devtools://devtools/bundled/inspector.html?experiments=true&v8only=true&ws=127.0.0.1:14549
	auto WebsocketAddress = GetWebsocketAddress();
	std::stringstream ChromeUrl;
	ChromeUrl << "chrome-devtools://devtools/bundled/inspector.html?experiments=true&v8only=true&ws=" << WebsocketAddress;
	return ChromeUrl.str();
}

std::string TV8Inspector::GetWebsocketAddress()
{
	std::string WebsocketAddress;
	auto EnumSocketAddress = [&](const std::string& InterfaceName,SoySockAddr& Address)
	{
		if ( !WebsocketAddress.empty() )
			return;
		std::stringstream WebsocketAddressStr;
		WebsocketAddressStr << Address;
		WebsocketAddress = WebsocketAddressStr.str();
	};
	auto& Socket = mWebsocketServer->GetSocket();
	Socket.GetSocketAddresses(EnumSocketAddress);
	
	return WebsocketAddress;
}


void TV8Inspector::OnMessage(SoyRef Connection,const std::string& Message)
{
	std::Debug << "Chrome tools message: " << Message << std::endl;
	mClient = Connection;

	//	send message to session
	//Array<uint8_t> MessageBuffer;
	//Soy::StringToArray( Message, GetArrayBridge(MessageBuffer) );
	//std::string TempMessage = Message;
	
	auto DispatchMessage = [=](v8::Local<v8::Context> Context)
	{
		Array<uint8_t> MessageBuffer;
		std::Debug << "Message=" << Message << std::endl;
		Soy::StringToArray( Message, GetArrayBridge(MessageBuffer) );
		v8_inspector::StringView MessageString( MessageBuffer.GetArray(), MessageBuffer.GetSize() );
		this->mSession->dispatchProtocolMessage(MessageString);
	};
	mContainer.QueueScoped(DispatchMessage);
}

void TV8Inspector::OnMessage(SoyRef Connection,const Array<uint8_t>& Message)
{
	std::Debug << "Chrome tools binary message: " << Message.GetDataSize() << "bytes" << std::endl;
	mClient = Connection;

}

void TV8Inspector::SendResponse(const std::string& Message)
{
	mWebsocketServer->Send( mClient, Message );
}


void TV8Inspector::OnDiscoveryRequest(const std::string& Url,Http::TResponseProtocol& Response)
{
	//	gr: I've not found the protocol documentation yet...
	//	https://nodejs.org/en/docs/guides/debugging-getting-started/
	/*
	 {
	 "description": "node.js instance",
	 "devtoolsFrontendUrl": "chrome-devtools://devtools/bundled/inspector.html?experiments=true&v8only=true&ws=127.0.0.1:9229/0f2c936f-b1cd-4ac9-aab3-f63b0f33d55e",
	 "faviconUrl": "https://nodejs.org/static/favicon.ico",
	 "id": "0f2c936f-b1cd-4ac9-aab3-f63b0f33d55e",
	 "title": "node",
	 "type": "node",
	 "url": "file://",
	 "webSocketDebuggerUrl": "ws://127.0.0.1:9229/0f2c936f-b1cd-4ac9-aab3-f63b0f33d55e"
	 }
	 */
	//	https://github.com/hsharsha/v8inspector/blob/master/inspector_socket_server.cc#L377
	if ( Url == "json" )
	{
		std::string WebsocketAddress;
		auto EnumSocketAddress = [&](const std::string& InterfaceName,SoySockAddr& Address)
		{
			if ( !WebsocketAddress.empty() )
				return;
			std::stringstream WebsocketAddressStr;
			WebsocketAddressStr << Address;
			WebsocketAddress = WebsocketAddressStr.str();
		};
		auto& Socket = mWebsocketServer->GetSocket();
		Socket.GetSocketAddresses(EnumSocketAddress);

		std::stringstream WebsocketUrl;
		WebsocketUrl << "ws://" << WebsocketAddress << "/" << mUuid;
		TJsonWriter Json;
		Json.Open();
		Json.Push("description", "description");
		Json.Push("id", mUuid );
		Json.Push("type", "type");
		Json.Push("title", "title");
		Json.Push("webSocketDebuggerUrl", WebsocketUrl.str() );
		Json.Close();
		Response.SetContent( Json.GetString(), SoyMediaFormat::Json );
		return;
	}
	
	
	if ( Url == "json/version" )
	{
		TJsonWriter Json;
		Json.Open();
		Json.Push("Browser", "v8inspector");
		Json.Push("Protocol-Version", "1.1" );
		Json.Close();
		Response.SetContent( Json.GetString(), SoyMediaFormat::Json );
		return;
	}
	
	std::Debug << "Unhandled chrome tools request: " << Url << std::endl;
}

void TV8Inspector::runMessageLoopOnPause(int contextGroupId)
{
	//	https://github.com/hsharsha/v8inspector/blob/master/inspector_agent.cc#L147
	//	https://medium.com/@hyperandroid/v8-inspector-from-an-embedder-standpoint-7f9c0472e2b7
	/*
	 While runMessageLoopOnPause is being called, you must
	 synchronously consume all front end (Dev Tools) debugging
	 messages. If not, you will not get all context information
	 of the code you are debugging. Once V8 knows it has no more
	 inspector messages pending, it will call quitMessageLoopOnPause
	 automatically.
	 */
	std::Debug << __func__ << std::endl;

	//	run all the contexts in this group
	auto ThreadIsRunning = []{	return true;	};
	mContainer.ProcessJobs(ThreadIsRunning);
}

void TV8Inspector::quitMessageLoopOnPause()
{
	std::Debug << __func__ << std::endl;
}

v8::Local<v8::Context> TV8Inspector::ensureDefaultContextInGroup(int contextGroupId)
{
	auto& Isolate = mContainer.GetIsolate();
	auto LocalContext = mContainer.mContext->GetLocal(Isolate);
	return LocalContext;
}

