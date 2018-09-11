#include "v8Minimal.h"
#include "TV8Container.h"
#include "TApiWebsocket.h"
#include <SoyFilesystem.h>

#include "TV8Inspector.h"
#include "include/v8-inspector.h"

std::shared_ptr<v8::Platform>	mPlatform;
using namespace v8;
using namespace v8_inspector;

std::shared_ptr<V8Inspector>		mInspector;


class InspectorClientImpl : public v8_inspector::V8InspectorClient {
public:
	InspectorClientImpl(Local<v8::Context> Context,std::function<void()> RunMessageLoop,std::function<void()> StopMessageLoop) :
		mContext			( Context ),
		mRunMessageLoop		( RunMessageLoop ),
		mStopMessageLoop	( StopMessageLoop )
	{
	}
	virtual ~InspectorClientImpl()
	{
	}
	
private:
	
	virtual v8::Local<v8::Context> ensureDefaultContextInGroup(int context_group_id) override
	{
		return mContext;
	}
	/*
	virtual double currentTimeMS() override
	{
		return 1.0;
	}*/
	virtual void runMessageLoopOnPause(int context_group_id) override
	{
		mRunMessageLoop();
	}
	virtual void quitMessageLoopOnPause() override
	{
		mStopMessageLoop();
	}
/*
	std::unique_ptr<v8_inspector::V8Inspector> inspector_;
	std::unique_ptr<v8_inspector::V8InspectorSession> session_;
	std::unique_ptr<v8_inspector::V8Inspector::Channel> channel_;
*/
	//v8::Isolate* isolate_;
	//v8::Global<v8::Context> context_;
	Local<v8::Context>		mContext;
	std::function<void()>	mRunMessageLoop;
	std::function<void()>	mStopMessageLoop;

	//DISALLOW_COPY_AND_ASSIGN(InspectorClientImpl);
};




void RunHelloWorld(Local<Context> context)
{
	auto* isolate = context->GetIsolate();
	
	// Create a string containing the JavaScript source code.
	v8::Local<v8::String> source =
	v8::String::NewFromUtf8(isolate, "function Log(str) { console.log(str); }\nLog('Hello!'); throw 'xxx'",
							v8::NewStringType::kNormal)
	.ToLocalChecked();
		
	// Compile the source code.
	/*
	static std::string SourceFilename("SourceFile.js");
	auto OriginStr = v8::GetString(*isolate, SourceFilename );
	auto OriginRow = v8::Integer::New( isolate, 0 );
	auto OriginCol = v8::Integer::New( isolate, 0 );
	auto Cors = v8::Boolean::New( isolate, true );
	static int ScriptIdCounter = 99;
	auto ScriptId = v8::Integer::New( isolate, ScriptIdCounter++ );
	auto OriginUrl = v8::GetString(*isolate, std::string("file://")+SourceFilename );
	
	static std::shared_ptr<v8::ScriptOrigin> HelloWorldOrigin;
	HelloWorldOrigin.reset( new v8::ScriptOrigin( OriginStr, OriginRow, OriginCol, Cors, ScriptId, OriginUrl ) );
	 auto* pOrigin = HelloWorldOrigin.get();
*/
	v8::ScriptOrigin origin(
							v8::String::NewFromUtf8(isolate, "http://localhost/abcd.js"),
							v8::Integer::New(isolate, 0),
							v8::Integer::New(isolate, 0)
							);
	auto* pOrigin = &origin;
	//v8::ScriptOrigin* pOrigin = nullptr;
	static v8::Local<v8::Script> script =
	v8::Script::Compile(context, source, pOrigin ).ToLocalChecked();
		
	// Run the script to get the result.
	auto result = script->Run(context);
	/*
	//v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
	
	// Convert the result to an UTF8 string and print it.
	v8::String::Utf8Value utf8(isolate, result);
	printf("%s\n", *utf8);
	 */
}

void v8min_main_helloworld(v8::Isolate* isolate,const std::string& mRootDirectory)
{
	
	// Create a new context.
	/*
	v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New(isolate);
	
	v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global_template);
	Local<ObjectTemplate> gl = ObjectTemplate::New(isolate);
	global_template->Set(String::NewFromUtf8(isolate, "gl"), gl);
	
*/
	v8::Local<v8::Context> context = v8::Context::New(isolate);

	
	
	// Enter the context for compiling and running the hello world script.
	v8::Context::Scope context_scope(context);
	
	

	std::shared_ptr<V8InspectorClient>	mInspectorClient;
	std::shared_ptr<V8Inspector::Channel>	mChannel;
	std::shared_ptr<V8InspectorSession>		mSession;
	std::shared_ptr<TWebsocketServer>	mWebsocketServer;
	SoyRef WebsocketClient;
	
	bool RunOnlyMessagesInLoop = false;
	
	auto RunMessageLoop = [&]()
	{
		std::Debug << "RunMessageLoop" << std::endl;
		RunOnlyMessagesInLoop = true;
	};
	auto QuitMessageLoop = [&]()
	{
		std::Debug << "QuitMessageLoop" << std::endl;
		RunOnlyMessagesInLoop = false;
	};

	auto DoSendResponse = [&](const std::string& Message)
	{
		std::Debug << "Send response " << Message << std::endl;
		mWebsocketServer->Send( WebsocketClient, Message );
	};
	
	{
		auto ContextGroupId = 1;
		mInspectorClient.reset( new InspectorClientImpl(context,RunMessageLoop,QuitMessageLoop) );
		mInspector = V8Inspector::create( isolate, mInspectorClient.get() );
		TStringViewContainer ContextName("TheContextName");
		V8ContextInfo ContextInfo( context, ContextGroupId, ContextName.GetStringView() );
		mInspector->contextCreated(ContextInfo);

		// create a v8 channel.
		// ChannelImpl : public v8_inspector::V8Inspector::Channel
		mChannel.reset( new ChannelImpl(DoSendResponse) );
		
		TStringViewContainer State("{}");
		
		// Create a debugging session by connecting the V8Inspector
		// instance to the channel
		mSession = mInspector->connect( ContextGroupId, mChannel.get(), State.GetStringView() );
	}
	
	
	
	Array<std::string> Messages;
	std::mutex MessagesLock;
	auto OnTextMessage = [&](SoyRef Connection,const std::string& Message)
	{
		WebsocketClient = Connection;
		MessagesLock.lock();
		Messages.PushBack(Message);
		MessagesLock.unlock();
	};
	auto OnBinaryMessage = [&](SoyRef Connection,const Array<uint8_t>& Message)
	{
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
	
	
	auto GetWebsocketAddress = [&]
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
	};
	
	auto GetChromeDebuggerUrl = [&]
	{
		//	lets skip the auto stuff for now!
		//	chrome-devtools://devtools/bundled/inspector.html?experiments=true&v8only=true&ws=127.0.0.1:14549
		auto WebsocketAddress = GetWebsocketAddress();
		std::stringstream ChromeUrl;
		ChromeUrl << "chrome-devtools://devtools/bundled/inspector.html?experiments=true&v8only=true&ws=" << WebsocketAddress;
		return ChromeUrl.str();
	};
	
	std::stringstream OpenUrl;
	OpenUrl << "http://" << GetChromeDebuggerUrl();
	auto str = OpenUrl.str();
	::Platform::ShellOpenUrl(str);
	std::Debug << "Open chrome inspector: " << GetChromeDebuggerUrl() << std::endl;
	
	auto* Session = mSession.get();
	auto SendMessageToSession = [&](const std::string& Message)
	{
		Array<uint8_t> MessageBuffer;
		std::Debug << "Message=" << Message << std::endl;
		Soy::StringToArray( Message, GetArrayBridge(MessageBuffer) );
		v8_inspector::StringView MessageString( MessageBuffer.GetArray(), MessageBuffer.GetSize() );
		Session->dispatchProtocolMessage(MessageString);
	};
	
	RunHelloWorld(context);

	
	while ( true )
	{
		if ( false && !RunOnlyMessagesInLoop )
		{
			while (v8::platform::PumpMessageLoop(mPlatform.get(), isolate))
			{
	
			}
		}
		
		//	process inspector messages
		{
			std::lock_guard<std::mutex> Lock( MessagesLock );
			while ( Messages.GetSize() > 0 )
			{
				auto Message = Messages.PopAt(0);
				SendMessageToSession(Message);
			}
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	
}




static std::string GetString(const StringView& String)
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

static std::string GetString(StringBuffer& String)
{
	return GetString( String.string() );
}


void MessageHandler(v8::Local<v8::Message> message, v8::Local<v8::Value> exception) {
	
	v8::Isolate *isolate_ = v8::Isolate::GetCurrent();
	v8::Local<v8::Context> context = isolate_->GetEnteredContext();
	if (context.IsEmpty())
		return;
	v8_inspector::V8Inspector *inspector = mInspector.get();
	if ( inspector == nullptr )
		return;
	
	v8::Local<v8::StackTrace> stack = message->GetStackTrace();
	
	if ( !stack.IsEmpty() )
	{
		int StackSize = stack->GetFrameCount();
		std::Debug << "exception: stack size x" << StackSize << std::endl;
		for ( int i=0;	i<StackSize;	i++ )
		{
			auto Frame = stack->GetFrame(i);
			auto ScriptName = v8::GetString(Frame->GetScriptName());
			auto FuncName = v8::GetString(Frame->GetFunctionName());
			std::Debug << "stack #" << i << " " << ScriptName << ", " << FuncName << "()" << std::endl;
		}
	}
	
	auto ScriptOrigin = message->GetScriptOrigin();
	
	/*
	int script_id = static_cast<int>(message->GetScriptOrigin().ScriptID()->Value());
	if (!stack.IsEmpty() && stack->GetFrameCount() > 0) {
		int top_script_id = stack->GetFrame(0)->GetScriptId();
		if (top_script_id == script_id) script_id = 0;
	}
	int line_number = message->GetLineNumber(context).FromMaybe(0);
	int column_number = 0;
	if (message->GetStartColumn(context).IsJust())
		column_number = message->GetStartColumn(context).FromJust() + 1;
	
	v8_inspector::StringView detailed_message;
	v8::internal::Vector<uint16_t> message_text_string = ToVector(message->Get());
	v8_inspector::StringView message_text(message_text_string.start(),
										  (size_t) message_text_string.length());
	v8::internal::Vector<uint16_t> url_string;
	if (message->GetScriptOrigin().ResourceName()->IsString()) {
		url_string = ToVector(message->GetScriptOrigin().ResourceName().As<v8::String>());
	}
	v8_inspector::StringView url(url_string.start(), (size_t) url_string.length());
	
	inspector->exceptionThrown(context, message_text, exception, detailed_message,
							   url, (unsigned int) line_number,
							   (unsigned int) column_number,
							   inspector->createStackTrace(stack), script_id);
*/
}

void v8min_main(const std::string& mRootDirectory)
{
	std::string IcuPath = mRootDirectory + "../v8Runtime/icudtl.dat";
	std::string NativesBlobPath = mRootDirectory + "../v8Runtime/natives_blob.bin";
	std::string SnapshotBlobPath = mRootDirectory + "../v8Runtime/snapshot_blob.bin";
	
	if ( !V8::InitializeICUDefaultLocation( nullptr, IcuPath.c_str() ) )
		throw Soy::AssertException("Failed to load ICU");
	V8::InitializeExternalStartupData( NativesBlobPath.c_str(), SnapshotBlobPath.c_str() );
	
	//std::unique_ptr<v8::Platform> platform = v8::platform::CreateDefaultPlatform();
	mPlatform.reset( v8::platform::CreateDefaultPlatform() );
	V8::InitializePlatform( mPlatform.get() );
	V8::Initialize();
	
	// Create a new Isolate and make it the current one.
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	v8::Isolate* isolate = v8::Isolate::New(create_params);
	isolate->AddMessageListener(MessageHandler);
	
	{
		v8::Locker locker(isolate);
		isolate->Enter();
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);

		v8min_main_helloworld( isolate, mRootDirectory );
	}
	
	// Dispose the isolate and tear down V8.
	isolate->Dispose();
	v8::V8::Dispose();
	v8::V8::ShutdownPlatform();
	delete create_params.array_buffer_allocator;
}



