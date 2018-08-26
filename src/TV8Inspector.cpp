#include "TV8Inspector.h"
#include <SoyJson.h>




TV8Inspector::TV8Inspector(v8::Isolate& Isolate) :
	mIsolate	( Isolate ),
	mUuid		( "f00df00d-f00d-f00d-f00d-f00df00df00d" )
{
	auto ChromeDevToolsPort = 9229;

	auto OnWebRequest = [this](std::string& Url,Http::TResponseProtocol& Response)
	{
		this->OnDiscoveryRequest( Url, Response );
	};
	mDiscoveryServer.reset( new THttpServer(ChromeDevToolsPort,OnWebRequest) );
	
	
	auto OnTextMessage = [this](const std::string& Message)
	{
		this->OnMessage( Message );
	};
	auto OnBinaryMessage = [this](const Array<uint8_t>& Message)
	{
		this->OnMessage( Message );
	};
	auto InspectorPort = 0;
	mWebsocketServer.reset( new TWebsocketServer(InspectorPort,OnTextMessage,OnBinaryMessage) );
/*
	9229
	9229
	agent->Start(isolate, platform, argv[1]);
	agent->PauseOnNextJavascriptStatement("Break on start");
 */
}

void TV8Inspector::OnMessage(const std::string& Message)
{
	std::Debug << "Chrome tools message: " << Message << std::endl;
}

void TV8Inspector::OnMessage(const Array<uint8_t>& Message)
{
	std::Debug << "Chrome tools binary message: " << Message.GetDataSize() << "bytes" << std::endl;

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
	
	
	std::Debug << "Unhandled chrome tools request: " << Url << std::endl;
}
