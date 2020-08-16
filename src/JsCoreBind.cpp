//#include "JsCoreBind.h"
#include "TBind.h"
#include "SoyAssert.h"
#include "SoyFilesystem.h"

#include "TApiCommon.h"
#include "TApiSocket.h"
#include "TApiPanopoly.h"
#include "TApiEngine.h"
#include "TApiWebsocket.h"
#include "TApiHttp.h"
#include "TApiZip.h"
#if !defined(TARGET_LINUX) && !defined(TARGET_ANDROID)
#include "TApiGui.h"
#endif

#if defined(ENABLE_OPENGL)
#include "TApiOpengl.h"
#endif

#if defined(ENABLE_DIRECTX)
#include "TApiDirectx11.h"
#endif

//	gr: maybe make this an actual define
#if defined(TARGET_OSX) || defined(TARGET_IOS) || defined(TARGET_LINUX) || defined(TARGET_WINDOWS) 
#if !defined(TARGET_WINDOWS_UWP)
#define ENABLE_APIMEDIA
#endif
#endif

#if defined(TARGET_WINDOWS)
#define ENABLE_APIXR
#endif

//	gr: todo; rename/rewrite this with new names
#if defined(ENABLE_APIVISION)
#include "TApiVision.h"
#endif

#if defined(ENABLE_APIMEDIA)
#include "TApiMedia.h"
#endif

#if defined(ENABLE_APIOPENCV)
#include "TApiOpencv.h"
#endif

#if defined(ENABLE_APIXR)
#include "TApiXr.h"
#endif

#if defined(TARGET_OSX)
#include "TApiAudio.h"
//#include "TApiOpencl.h"
//#include "TApiDlib.h"
#include "TApiEzsift.h"
#include "TApiInput.h"
#include "TApiBluetooth.h"
#include "TApiLeapMotion.h"
#include "TApiOpenvr.h"
#endif

#if defined(TARGET_OSX)||defined(TARGET_WINDOWS)
#include "TApiDll.h"
#include "TApiSerial.h"
#endif



JSObjectRef	JSObjectMakeTypedArrayWithBytesWithCopy(JSContextRef Context, JSTypedArrayType ArrayType,const uint8_t* ExternalBuffer, size_t ExternalBufferSize, JSValueRef* Exception);
JSValueRef JSObjectToValue(JSObjectRef Object);
bool JSContextGroupRunVirtualMachineTasks(JSContextGroupRef ContextGroup, std::function<void(std::chrono::milliseconds)> &Sleep);
void JSGlobalContextSetQueueJobFunc(JSContextGroupRef ContextGroup, JSGlobalContextRef Context, std::function<void(std::function<void(JSContextRef)>)> QueueJobFunc);

#if defined(JSAPI_JSCORE)
//	wrapper as v8 needs to setup the runtime files
JSContextGroupRef JSContextGroupCreateWithRuntime(const std::string& RuntimeDirectory)
{
	return JSContextGroupCreate();
}
#endif

#if defined(JSAPI_JSCORE)
void JSGlobalContextSetQueueJobFunc(JSContextGroupRef ContextGroup, JSGlobalContextRef Context, std::function<void(std::function<void(JSContextRef)>)> QueueJobFunc)
{
	
}
#endif

#if defined(JSAPI_JSCORE)
//	wrapper as v8 needs a context
size_t JSStringGetUTF8CString(JSContextRef Context,JSStringRef string, char* buffer, size_t bufferSize)
{
	return JSStringGetUTF8CString(string, buffer, bufferSize);
}
#endif

#if defined(JSAPI_JSCORE)
//	wrapper as v8 needs a context
JSStringRef JSStringCreateWithUTF8CString(JSContextRef Context, const char* string)
{
	return JSStringCreateWithUTF8CString(string);
}
#endif

#if defined(JSAPI_JSCORE)
//	wrapper as v8 needs a context
JSStringRef JSStringCreateWithUTF8CString(JSContextRef Context, const std::string& string)
{
	return JSStringCreateWithUTF8CString(string.c_str());
}
#endif

#if defined(JSAPI_JSCORE)
JSClassRef JSClassCreate(JSContextRef Context, JSClassDefinition& Definition)
{
	return JSClassCreate(&Definition);
}
#endif

#if defined(JSAPI_JSCORE)
JSValueRef JSObjectToValue(JSObjectRef Object)
{
	return Object;
}
#endif

#if defined(JSAPI_JSCORE)
void JSObjectSetProperty(JSContextRef Context,JSObjectRef This,const std::string& Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception)
{
	//	some systems have caching or special property types for strings,
	//	but not in js core, so manage it ourselves
	auto NameJs = JsCore::GetString( Context, Name );
	JSObjectSetProperty( Context, This, NameJs, Value, Attribs, Exception );
	JSStringRelease( NameJs );
}
#endif

#if defined(JSAPI_JSCORE)
JSValueRef JSObjectGetProperty(JSContextRef Context,JSObjectRef This,const std::string& Name,JSValueRef* Exception)
{
	//	some systems have caching or special property types for strings,
	//	but not in js core, so manage it ourselves
	auto NameJs = JsCore::GetString( Context, Name );
	auto Value = JSObjectGetProperty( Context, This, NameJs, Exception );
	JSStringRelease( NameJs );
	return Value;
}
#endif

#if defined(JSAPI_JSCORE)
//	creating a value from JSON in Chakra is much faster without going into a JSstring, so we have a wrapper
JSValueRef JSValueMakeFromJSONString(JSContextRef Context,const std::string& Json)
{
	auto JsonString = JSStringCreateWithUTF8CString( Context, Json );
	return JSValueMakeFromJSONString( Context, JsonString );
}
#endif

#if defined(JSAPI_JSCORE)
std::string JSJSONStringFromValue(JSContextRef Context,JSValueRef Value)
{
	unsigned Indent = 1;
	JSValueRef Exception = nullptr;
	auto StringValue = JSValueCreateJSONString( Context, Value, Indent, &Exception );
	JsCore::ThrowException( Context, Exception, "JSValueCreateJSONString" );
	auto String = Bind::GetString( Context, StringValue );
	return String;
}
#endif

#if defined(JSAPI_JSCORE)
std::string JSJSONStringFromValue_NoThrow(JSContextRef Context,JSValueRef Value)
{
	unsigned Indent = 1;
	JSValueRef Exception = nullptr;
	auto StringValue = JSValueCreateJSONString( Context, Value, Indent, &Exception );
	if ( Exception )
		return "JSJSONStringFromValue_NoThrow exception";
	//JsCore::ThrowException( Context, Exception, "JSValueCreateJSONString" );
	auto String = Bind::GetString( Context, StringValue );
	return String;
}
#endif



namespace JsCore
{
	std::map<JSGlobalContextRef,TContext*> ContextCache;
	
	void		AddContextCache(TContext& Context,JSGlobalContextRef Ref);
	void		RemoveContextCache(TContext& Context);
	
	template<typename TYPE>
	TYPE*	GetPointer(JSContextRef Context,JSValueRef Handle);
}

prmem::Heap& JsCore::GetGlobalObjectHeap()
{
	static prmem::Heap gGlobalObjectHeap(true, true, "JsCore::GlobalObjectHeap",0,true);
	return gGlobalObjectHeap;
}



template<typename TYPE>
JSTypedArrayType GetTypedArrayType()
{
	static_assert( sizeof(TYPE) == -1, "GetTypedArrayType not implemented for type" );
	return kJSTypedArrayTypeUint8Array;	//	linux/gcc has a warning with no return
}

template<> JSTypedArrayType GetTypedArrayType<uint8_t>()	{	return kJSTypedArrayTypeUint8Array;	}
template<> JSTypedArrayType GetTypedArrayType<uint16_t>()	{	return kJSTypedArrayTypeUint16Array;	}
template<> JSTypedArrayType GetTypedArrayType<uint32_t>()	{	return kJSTypedArrayTypeUint32Array;	}
template<> JSTypedArrayType GetTypedArrayType<int8_t>()		{	return kJSTypedArrayTypeInt8Array;	}
template<> JSTypedArrayType GetTypedArrayType<int16_t>()	{	return kJSTypedArrayTypeInt16Array;	}
template<> JSTypedArrayType GetTypedArrayType<int32_t>()	{	return kJSTypedArrayTypeInt32Array;	}
template<> JSTypedArrayType GetTypedArrayType<float>()		{	return kJSTypedArrayTypeFloat32Array;	}


std::ostream& operator<<(std::ostream &out,const JSTypedArrayType& in)
{
#define CASE_VALUE_STRING(Value)	case Value:	out << static_cast<const char*>(#Value);	break
	switch ( in )
	{
			CASE_VALUE_STRING(kJSTypedArrayTypeNone);
			CASE_VALUE_STRING(kJSTypedArrayTypeArrayBuffer);
			CASE_VALUE_STRING(kJSTypedArrayTypeUint8Array);
			CASE_VALUE_STRING(kJSTypedArrayTypeUint8ClampedArray);
			CASE_VALUE_STRING(kJSTypedArrayTypeUint16Array);
			CASE_VALUE_STRING(kJSTypedArrayTypeUint32Array);
			CASE_VALUE_STRING(kJSTypedArrayTypeInt8Array);
			CASE_VALUE_STRING(kJSTypedArrayTypeInt16Array);
			CASE_VALUE_STRING(kJSTypedArrayTypeInt32Array);
			CASE_VALUE_STRING(kJSTypedArrayTypeFloat32Array);
			CASE_VALUE_STRING(kJSTypedArrayTypeFloat64Array);
		default:
			out << "kJSTypedArrayType<unhandled=" << static_cast<int>(in) << ">";
			break;
	}
#undef CASE_VALUE_STRING
	
	return out;
}



std::ostream& operator<<(std::ostream &out,const JSType& in)
{
#define CASE_VALUE_STRING(Value)	case Value:	out << static_cast<const char*>(#Value);	break
	switch ( in )
	{
			CASE_VALUE_STRING(kJSTypeUndefined);
			CASE_VALUE_STRING(kJSTypeNull);
			CASE_VALUE_STRING(kJSTypeBoolean);
			CASE_VALUE_STRING(kJSTypeNumber);
			CASE_VALUE_STRING(kJSTypeString);
			CASE_VALUE_STRING(kJSTypeObject);
		default:
			out << "JSType<unhandled=" << static_cast<int>(in) << ">";
			break;
	}
#undef CASE_VALUE_STRING
	
	return out;
}

JsCore::TContext& JsCore::GetContext(JSContextRef ContextRef)
{
	auto Key = JSContextGetGlobalContext( ContextRef );
	//auto Value = &Context;

	//	gr: currently, the contextref from a callback doesn't match
	//		the GlobalRef, so I think they're not the same, but I cant
	//		see globalcontext->context conversion
	//auto Entry = ContextCache.begin();
	auto Entry = ContextCache.find(Key);
	if ( Entry == ContextCache.end() )
		throw Soy::AssertException("Couldn't find context");
	return *Entry->second;
}

void JsCore::AddContextCache(TContext& Context,JSGlobalContextRef Ref)
{
	auto Key = Ref;
	auto Value = &Context;
	ContextCache[Key] = Value;
}

void JsCore::RemoveContextCache(TContext& Context)
{
	auto Value = &Context;
	for ( auto it=ContextCache.begin();	it!=ContextCache.end();	it++ )
	{
		if ( it->second != Value )
			continue;
		
		ContextCache.erase( it );
		return;
	}
	throw Soy::AssertException("Couldn't find context");
}


JSObjectRef JsCore::GetObject(JSContextRef Context,JSValueRef Value)
{
	auto ValueType = JSValueGetType( Context, Value );
	if ( ValueType != kJSTypeObject )
		throw Soy::AssertException("Value is not an object");
	
	if ( !JSValueIsObject( Context, Value ) )
		throw Soy::AssertException("Value is not object");

	JSValueRef Exception = nullptr;
	auto Object = JSValueToObject( Context, Value, &Exception );
	ThrowException( Context, Exception );
	return Object;
}

Bind::TObject JsCore::ParseObjectString(JSContextRef Context, const std::string& JsonString)
{
	auto Value = JSValueMakeFromJSONString(Context, JsonString);
	auto ObjectValue = GetObject(Context, Value);
	Bind::TObject Object(Context, ObjectValue);
	return Object;
}

std::string JsCore::StringifyObject(JsCore::TLocalContext& Context,Bind::TObject& Object)
{
	auto ContextRef = Context.mLocalContext;
#if defined(JSAPI_JSCORE)
	return JSJSONStringFromValue(ContextRef, Object.mThis);
#else
	auto& Json = Context.mGlobalContext.GetGlobalObject(Context, "JSON");
	auto& Stringify = Json.GetFunction("stringify");
	Bind::TCallback Params(Context);
	Params.SetArgumentObject(0, Object);
	Stringify.Call(Params);
	auto String = Params.GetReturnString();
	return String;
#endif
}




JsCore::TFunction::TFunction(JSContextRef Context,JSValueRef Value) :
	TFunction	( Context, GetObject( Context, Value ) )
{
}

JsCore::TFunction::TFunction(JSContextRef Context,JSObjectRef Value)
{
	mThis = Value;
	
	if ( !JSObjectIsFunction(Context, mThis) )
		throw Soy::AssertException("Object should be function");
}


JsCore::TFunction::~TFunction()
{
}


void JsCore::TFunction::Call(JsCore::TCallback& Params) const
{
	auto Context = Params.mLocalContext.mLocalContext;
	
	//	make sure function handle is okay
	auto FunctionHandle = mThis;
	if ( !JSValueIsObject( Context, FunctionHandle ) )
		throw Soy::AssertException("Function's handle is no longer an object");
	
	//	docs say null is okay
	//		https://developer.apple.com/documentation/javascriptcore/1451407-jsobjectcallasfunction?language=objc
	//		The object to use as "this," or NULL to use the global object as "this."
#if defined(TARGET_OSX)
	if ( Params.mThis == nullptr )
		Params.mThis = JSContextGetGlobalObject( Context );
#endif
	auto This = Params.mThis ? GetObject( Context, Params.mThis ) : nullptr;
	
	//	call
	JSValueRef Exception = nullptr;
	auto Result = JSObjectCallAsFunction( Context, FunctionHandle, This, Params.mArguments.GetSize(), Params.mArguments.GetArray(), &Exception );

	ThrowException( Context, Exception );
	
	Params.mReturn = Result;
}


void JsCore::GetString(JSContextRef Context,JSStringRef Handle,ArrayBridge<char>&& Buffer)
{
	//	gr: length doesn't include terminator, but JSStringGetUTF8CString writes one
	Buffer.SetSize(JSStringGetLength(Handle));
	Buffer.PushBack('\0');
	
	//	we need a context for v8 from version 7 up.
	size_t bytesWritten = JSStringGetUTF8CString( Context, Handle, Buffer.GetArray(), Buffer.GetSize() );
	Buffer.SetSize(bytesWritten);
	if ( Buffer.GetBack() == '\0' )
		Buffer.SetSize( bytesWritten-1 );
}

std::string	JsCore::GetString(JSContextRef Context,JSStringRef Handle)
{
	Array<char> Buffer;
	GetString( Context, Handle, GetArrayBridge(Buffer) );

	if ( Buffer.IsEmpty() )
		return std::string();

	std::string utf_string = std::string( Buffer.GetArray(), Buffer.GetSize() );
	return utf_string;
}

std::string	JsCore::GetString(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	JSValueRef Exception = nullptr;
	auto StringJs = JSValueToStringCopy( Context, Handle, &Exception );
	ThrowException( Context, Exception );
	auto Str = GetString( Context, StringJs );
	JSStringRelease( StringJs );
	return Str;
}



float JsCore::GetFloat(JSContextRef Context,JSValueRef Handle)
{
	//	gr: this should do a type check I think
	JSValueRef Exception = nullptr;
	auto DoubleJs = JSValueToNumber( Context, Handle, &Exception );
	auto Float = static_cast<float>( DoubleJs );
	return Float;
}

bool JsCore::GetBool(JSContextRef Context,JSValueRef Handle)
{
	auto Bool = JSValueToBoolean( Context, Handle );
	return Bool;
}


JSStringRef JsCore::GetString(JSContextRef Context,const std::string& String)
{
	//	JSCore doesn't need a context, but v8 does
	auto Handle = JSStringCreateWithUTF8CString( Context, String );
	return Handle;
}

JSValueRef JsCore::GetValue(JSContextRef Context,const std::string& String)
{
	auto StringHandle = JSStringCreateWithUTF8CString( Context, String );
	auto ValueHandle = JSValueMakeString( Context, StringHandle );
	JSStringRelease(StringHandle);
	return ValueHandle;
}

JSValueRef JsCore::GetValue(JSContextRef Context,JSObjectRef Value)
{
	return JSObjectToValue( Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,bool Value)
{
	return JSValueMakeBoolean( Context, Value );
}

//	on windows size_t and u64 are the same, so redundant
#if !defined(TARGET_WINDOWS)&& !defined(TARGET_LINUX) && !defined(TARGET_ANDROID)
JSValueRef JsCore::GetValue(JSContextRef Context, size_t Value)
{
	auto Value64 = static_cast<uint64_t>(Value);
	return GetValue( Context, Value64 );
}
#endif

JSValueRef JsCore::GetValue(JSContextRef Context,int64_t Value)
{
	//	javascript doesn't support 64bit (kinda), so throw if number goes over 32bit
	if ( Value > std::numeric_limits<int32_t>::max() )
	{
		std::stringstream Error;
		Error << "Javascript doesn't support 64bit integers, so this value(" << Value <<") is out of range (max 32bit=" << std::numeric_limits<uint32_t>::max() << ")";
		throw Soy::AssertException( Error.str() );
	}
	if ( Value < std::numeric_limits<int32_t>::min() )
	{
		std::stringstream Error;
		Error << "Javascript doesn't support 64bit integers, so this value(" << Value <<") is out of range (max 32bit=" << std::numeric_limits<uint32_t>::max() << ")";
		throw Soy::AssertException( Error.str() );
	}

	auto Value32 = static_cast<uint32_t>( Value );
	return GetValue( Context, Value32 );
}

JSValueRef JsCore::GetValue(JSContextRef Context,uint64_t Value)
{
	//	javascript doesn't support 64bit (kinda), so throw if number goes over 32bit
	if ( Value > std::numeric_limits<uint32_t>::max() )
	{
		std::stringstream Error;
		Error << "Javascript doesn't support 64bit integers, so this value(" << Value <<") is out of range (max 32bit=" << std::numeric_limits<uint32_t>::max() << ")";
		throw Soy::AssertException( Error.str() );
	}
	if ( Value < std::numeric_limits<uint32_t>::min() )
	{
		std::stringstream Error;
		Error << "Javascript doesn't support 64bit integers, so this value(" << Value <<") is out of range (max 32bit=" << std::numeric_limits<uint32_t>::max() << ")";
		throw Soy::AssertException( Error.str() );
	}
	
	auto Value32 = static_cast<uint32_t>( Value );
	return GetValue( Context, Value32 );
}

JSValueRef JsCore::GetValue(JSContextRef Context,uint8_t Value)
{
	return JSValueMakeNumber( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,uint16_t Value)
{
	return JSValueMakeNumber( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,uint32_t Value)
{
	return JSValueMakeNumber( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,int8_t Value)
{
	return JSValueMakeNumber( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,int16_t Value)
{
	return JSValueMakeNumber( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,int32_t Value)
{
	return JSValueMakeNumber( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,float Value)
{
	return JSValueMakeNumber( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TObject& Object)
{
	return JSObjectToValue( Object.mThis );
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TFunction& Object)
{
	return JSObjectToValue( Object.mThis );
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TArray& Object)
{
	return JSObjectToValue( Object.mThis );
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TPersistent& Object)
{
	if ( !Object )
		throw Soy::AssertException("return null, or undefined here?");

#if defined(JSAPI_V8)
	auto Local = Object.mObject->GetLocal( Context.GetIsolate() );
	auto LocalValue = Local.As<v8::Value>();
	return JSValueRef( LocalValue );
#else
	return JSObjectToValue( Object.mObject );
#endif
	
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TPromise& Object)
{
	return GetValue( Context, Object.mPromise );
}

//	gr: windows needs this as Bind::TInstance
Bind::TInstance::TInstance(const std::string& RootDirectory,const std::string& ScriptFilename,std::function<void(int32_t)> OnShutdown) :
	mContextGroupThread	( std::string("JSCore thread ") + ScriptFilename ),
	mOnShutdown			( OnShutdown )
{
	mRootDirectory = RootDirectory;
	
	auto CreateVirtualMachine = [this,ScriptFilename]()
	{
		#if defined(TARGET_OSX)
		{
			auto ThisRunloop = CFRunLoopGetCurrent();
			auto MainRunloop = CFRunLoopGetMain();

			if ( ThisRunloop == MainRunloop )
				throw Soy::AssertException("Need to create JS VM on a different thread to main");
		}
		#endif

		//	for v8
		std::string RuntimePath = Platform::GetAppResourcesDirectory();
	#if defined(TARGET_OSX)
		RuntimePath += "/v8Runtime/";
	#endif

#if defined(TARGET_WINDOWS)
		if ( &JSContextGroupCreate == nullptr )
			throw Soy::AssertException("If this function pointer is null, we may have manually loaded symbols, but they've been stripped. Turn OFF whole program optimisation!");
#endif
		mContextGroup = JSContextGroupCreateWithRuntime( RuntimePath );
		if ( !mContextGroup )
			throw Soy::AssertException("JSContextGroupCreate failed");
	
		
		//	bind first
		try
		{
			//	create a context
			auto Context = CreateContext(mRootDirectory);
			
			ApiPop::Bind(*Context);
			ApiSocket::Bind(*Context);
			ApiPanopoly::Bind(*Context);
			ApiWebsocket::Bind( *Context );
			ApiHttp::Bind( *Context );

#if !defined(TARGET_LINUX) && !defined(TARGET_ANDROID)
			ApiEngine::Bind(*Context);
#endif
#if defined(ENABLE_OPENGL)
			ApiOpengl::Bind(*Context);
#endif
#if defined(ENABLE_DIRECTX)
			ApiDirectx11::Bind(*Context);
#endif

#if defined(ENABLE_APIXR)
			ApiXr::Bind(*Context);
#endif

#if defined(ENABLE_APIMEDIA)
			ApiMedia::Bind( *Context );
#endif

#if defined(ENABLE_APIOPENCV)
			ApiOpencv::Bind(*Context);
#endif
			
#if defined(TARGET_OSX)||defined(TARGET_WINDOWS)
			ApiDll::Bind( *Context );
			ApiSerial::Bind( *Context );
#endif
#if !defined(TARGET_LINUX) && !defined(TARGET_ANDROID)
			ApiGui::Bind( *Context );
			ApiZip::Bind(*Context);
#endif
#if defined(ENABLE_APIVISION)
			ApiCoreMl::Bind(*Context);
#endif
#if defined(TARGET_OSX)
			ApiAudio::Bind(*Context);
			//ApiOpencl::Bind( *
            //ApiDlib::Bind( *Context );
			ApiEzsift::Bind( *Context );
			ApiInput::Bind( *Context );
			ApiBluetooth::Bind( *Context );
			ApiLeapMotion::Bind( *Context );
			ApiOpenvr::Bind(*Context);
#endif

			std::string BootupSource;
			Soy::FileToString( mRootDirectory + ScriptFilename, BootupSource );
			Context->LoadScript( BootupSource, ScriptFilename );
		}
		catch(std::exception& e)
		{
			//	clean up
			std::Debug << "CreateVirtualMachine failed: "  << e.what() << std::endl;
			//Context.reset();
			throw;
		}
	};

#if defined(TARGET_WINDOWS)
#if defined(JSAPI_JSCORE)
	JsCore::LoadDll();
#endif
	CreateVirtualMachine();
#else
	//	gr: these exceptions are getting swallowed!
	mContextGroupThread.PushJob( CreateVirtualMachine );
	mContextGroupThread.Start();
#endif
}

JsCore::TInstance::~TInstance()
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	mContextGroupThread.WaitToFinish();

	//	this does some of the final releasing in JS land
	try
	{
		JSContextGroupRelease(mContextGroup);
	}
	catch(std::exception& e)
	{
		std::Debug << "Caught exception in JSContextGroupRelease(): " << e.what() << std::endl;
	}

	//	try and shutdown all javascript objects first
	for ( auto c = 0; c < mContexts.GetSize(); c++ )
	{
		try
		{
			auto& Context = *mContexts[c];
			Context.ReleaseContext();
		}
		catch(std::exception& e)
		{
			std::Debug << "Caught exception in Context.ReleaseContext(): " << e.what() << std::endl;
		}
	}

	//	now cleanup jobs & context
	while ( mContexts.GetSize() > 0 )
	{
		auto pContext = mContexts[0];
		mContexts.RemoveBlock(0, 1);
		try
		{
			pContext->Cleanup();
		}
		catch(std::exception& e)
		{
			std::Debug << "Caught exception in Context->Cleanup(): " << e.what() << std::endl;
		}
		pContext.reset();
	}

	
}

#if defined(JSAPI_JSCORE)
bool JSContextGroupRunVirtualMachineTasks(JSContextGroupRef ContextGroup, std::function<void(std::chrono::milliseconds)> &Sleep)
{
	return true;
}
#endif



bool JsCore::TInstance::OnJobQueueIteration(std::function<void (std::chrono::milliseconds)> &Sleep)
{
	if ( !mContextGroup )
		return true;
	
	return JSContextGroupRunVirtualMachineTasks(mContextGroup,Sleep);
}

std::shared_ptr<JsCore::TContext> JsCore::TInstance::CreateContext(const std::string& Name)
{
	JSClassRef Global = nullptr;
	
	auto Context = JSGlobalContextCreateInGroup( mContextGroup, Global );
	
	//	no name in v8. and in chakra GetString needs to be inside a local context
#if defined(JSAPI_JSCORE)
	JSGlobalContextSetName( Context, JsCore::GetString( Context, Name ) );
#endif

	std::shared_ptr<JsCore::TContext> pContext( new TContext( *this, Context, mRootDirectory ) );
	mContexts.PushBack( pContext );

	auto QueueJobFunc = [pContext](std::function<void(JSContextRef)> Job)
	{
		auto CallJob = [=](Bind::TLocalContext& LocalContext)
		{
			Job(LocalContext.mLocalContext);
		};
		pContext->Queue(CallJob);
	};
	auto WakeJobQueueFunc = [pContext]()
	{
		pContext->mJobQueue.Wake();
	};
	JSGlobalContextSetQueueJobFunc(mContextGroup, Context, QueueJobFunc);
	
	//	set pointer
	auto SetContext = [&](Bind::TLocalContext& LocalContext)
	{
	#if defined(JSAPI_V8)
		LocalContext.mLocalContext.SetContext( *pContext );
	#endif
	};
	pContext->Execute( SetContext );
	
	return pContext;
}


void JsCore::TInstance::DestroyContext(JsCore::TContext& Context)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	std::shared_ptr<JsCore::TContext> pContext;

	//	pop context
	{
		auto Index = -1;
		for ( auto i = 0; i < mContexts.GetSize(); i++ )
		{
			auto* MatchContext = mContexts[i].get();
			if ( MatchContext != &Context )
				continue;
			Index = i;
		}
		if ( Index < 0 )
			throw Soy::AssertException("Instance doesn't recognise context to destroy");
		pContext = mContexts[Index];
		mContexts.RemoveBlock(Index, 1);
	}

	//	as this is often called from the context, we need to deffer a cleanup
	auto ShutdownContext = [this,pContext]()
	{
		//	incase there's a dangling reference, manually cleanup
		pContext->Cleanup();
		//	gr: context will be captured const, so will only release after this job is done
		//pContext.reset();
	};

	mContextGroupThread.PushJob(ShutdownContext);
}

void JsCore::TInstance::Shutdown(int32_t ExitCode)
{
	std::Debug << __PRETTY_FUNCTION__ << "(" << ExitCode << ")" << std::endl;
	//	gr: does this need to defer?
	//	do callback
	if ( mOnShutdown )
		mOnShutdown(ExitCode);
}

void JsCore::ThrowException(JSContextRef Context, JSValueRef ExceptionHandle, const char* ThrowContext)
{
	auto ExceptionType = JSValueGetType(Context, ExceptionHandle);
	//	not an exception
	if (ExceptionType == kJSTypeUndefined || ExceptionType == kJSTypeNull)
		return;

	ThrowException(Context, ExceptionHandle, std::string(ThrowContext));
}

JsCore::TExceptionMeta JsCore::GetExceptionMeta(JSContextRef Context, JSValueRef ExceptionHandle)
{
	TExceptionMeta Meta;


	auto GetString_NoThrow = [](JSContextRef Context, JSValueRef Handle)
	{
		JSValueRef Exception = nullptr;
		auto HandleString = JSValueToStringCopy(Context, Handle, &Exception);
		if (Exception)
		{
			auto HandleType = JSValueGetType(Context, Handle);
			std::stringstream Error;
			Error << "Exception->String threw exception. Exception is type " << HandleType;
			return Error.str();
		}
		auto Str = JsCore::GetString(Context, HandleString);
		JSStringRelease(HandleString);
		return Str;
};

#if defined(JSAPI_JSCORE)
	JSObjectRef ExceptionObject = GetObject(Context,ExceptionHandle);
	auto& TheContext = GetContext(Context);
	auto LineValue = JSObjectGetProperty(Context, ExceptionObject, "line", nullptr);
	auto FilenameValue = JSObjectGetProperty(Context, ExceptionObject, "sourceURL", nullptr);
	Meta.mLine = GetInt<int>(Context, LineValue);
	Meta.mFilename = TheContext.GetResolvedFilename(GetString(Context, FilenameValue));
	
	//	gr: object json has meta, but it doesn't show string's exception, so need to convert the error object to a string too
	Meta.mMessage = GetString_NoThrow(Context, ExceptionHandle);
	auto ErrorJson = JSJSONStringFromValue_NoThrow(Context, ExceptionHandle);
#else
	
	Meta.mMessage = GetString_NoThrow(Context, ExceptionHandle);
#endif
	return Meta;
}

void JsCore::ThrowException(JSContextRef Context, JSValueRef ExceptionHandle, const std::string& ThrowContext)
{
	auto ExceptionType = JSValueGetType( Context, ExceptionHandle );
	//	not an exception
	if ( ExceptionType == kJSTypeUndefined || ExceptionType == kJSTypeNull )
		return;
	JSObjectRef ExceptionObject = GetObject(Context,ExceptionHandle);

	
	std::stringstream Error;
	auto ExceptionMeta = GetExceptionMeta(Context, ExceptionHandle);
	Error << ExceptionMeta.mFilename << ":" << ExceptionMeta.mLine << "; " << ExceptionMeta.mMessage;

	//	try and open xcode at the erroring line (a bit experimental)
	/*	gr: I have a case where this crashes osx, so... disabled
#if defined(JSAPI_JSCORE) && defined(TARGET_OSX)
	{
		try
		{
			Platform::ShellExecute(std::string("xed --launch ")+ExceptionMeta.mFilename);
			Platform::ShellExecute(std::string("xed --launch --line ")+std::to_string(ExceptionMeta.mLine));
		}
		catch(std::exception& e)
		{
			std::Debug << "Error launching xcode jump-to-file; " << e.what() << std::endl;
		}
	}
#endif
	*/
	throw Soy::AssertException(Error.str());
}




JsCore::TContext::TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory) :
	mInstance		( Instance ),
	mContext		( Context ),
	mRootDirectory	( RootDirectory ),
	mJobQueue		( *this, [&Instance](std::function<void(std::chrono::milliseconds)>& Sleep)	{	return Instance.OnJobQueueIteration(Sleep);	} )
{
	AddContextCache( *this, mContext );
	mJobQueue.Start();
}

JsCore::TContext::~TContext()
{
	try
	{
		Cleanup();
	}
	catch ( std::exception& e )
	{
		std::Debug << "Exception in TContext destructor: " << e.what() << std::endl;
	}
}


void JsCore::TContext::ReleaseContext()
{
	//Array<TTemplate>	mObjectTemplates;
	mMakePromiseFunction = Bind::TPersistent();

	//	should instance be doing this?
	if ( mContext )
	{
		JSGlobalContextRelease(mContext);
		mContext = nullptr;
	}
}

void JsCore::TContext::Cleanup()
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;

	//	should instance be doing this?
	ReleaseContext();

	mJobQueue.Stop();
	mJobQueue.QueueDeleteAll();
	mJobQueue.WaitToFinish();

	//	for safety, do this after jobs have all gone
	RemoveContextCache( *this );
}


void JsCore::TContext::Shutdown(int32_t ExitCode)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;

	mJobQueue.Stop();

	//	tell instance to clean up
	//mInstance.DestroyContext(*this);
	mInstance.Shutdown(ExitCode);
}

void JsCore::TContext::GarbageCollect(JSContextRef LocalContext)
{
	//	seems like this would be a good idea...
#if defined(JSAPI_V8)
#else
	std::lock_guard<std::recursive_mutex> Lock(mExecuteLock);
#endif
	
	JSGarbageCollect( LocalContext );
}


void JsCore::TContext::LoadScript(const std::string& Source,const std::string& Filename)
{
	//	gr: javascript core on OSX failed with this chi 𝑥 character.
	//		web/v8 is okay.
	//		javascriptcore ios is okay
	{
		auto IsAscii = [](char Char)
		{
			//	Source.substr(45107,20).c_str()[0]
			//	signed, so negative is the valid test!
			if ( Char > 127 )
				return false;
			if ( Char < 0 )
				return false;
			return true;
		};

		Array<int> NonAsciiPositions;
		for ( auto i=0;	i<Source.length();	i++ )
		{
			auto Char = Source[i];
			if ( IsAscii(Char) )
				continue;
			NonAsciiPositions.PushBack(i);
		}
		
		if ( !NonAsciiPositions.IsEmpty() )
		{
			auto SubString = [&](int Start,int End)
			{
				auto SourceLength = Source.length();
				Start = std::max(0,Start);
				End = std::min<int>(SourceLength,End);
				auto Length = End-Start;
				auto String = Source.substr( Start, End-Start );
				std::replace( String.begin(), String.end(), '\r', '\n');
				Start = std::max<int>(Start,String.find_last_of('\n')+1);
				String = Source.substr( Start, End-Start );
				End = std::max<int>(End,String.find_first_of('\n')+1);
				String = Source.substr( Start, End-Start );
				return String;
			};

			std::stringstream Error;
			for ( auto i=0;	i<NonAsciiPositions.GetSize();	i++ )
			{
				//	get the line this character is on
				auto CharPos = NonAsciiPositions[i];
				auto Start = std::max<int>(0,Source.rfind('\n',CharPos));
				auto End = Source.find('\n',CharPos);
				if ( End == Source.npos )
					End = Source.length();
				auto Line = Source.substr( Start, End-Start );
				std::replace( Line.begin(), Line.end(), '\r', ' ');
				
				//	insert >< markers
				Line.insert( CharPos-Start+1, " <<< ");
				Line.insert( CharPos-Start, " >>> ");
				Error << "Non-ascii char in source @" << CharPos << "; " << Line << std::endl;
			}
			Error << "Will fail to compile on JavascriptCore OSX";
			throw Soy::AssertException(Error);
		}
	}
	
	auto Exec = [=](Bind::TLocalContext& Context)
	{
		auto ThisHandle = JSObjectRef(nullptr);
		auto SourceJs = JSStringCreateWithUTF8CString( Context.mLocalContext, Source );
		auto FilenameJs = JSStringCreateWithUTF8CString( Context.mLocalContext, Filename );
		auto LineNumber = 0;
		JSValueRef Exception = nullptr;
		//	gr: to capture this result, probably need to store it persistently
		auto ResultHandle = JSEvaluateScript( Context.mLocalContext, SourceJs, ThisHandle, FilenameJs, LineNumber, &Exception );
		ThrowException( Context.mLocalContext, Exception, Filename );
	};
	//	this exec meant the load was taking so long, JS funcs were happening on the queue thread
	//	and that seemed to cause some crashes
	//	gr: on windows, this... has some problem where the main thread seems to get stuck? maybe to do with creating windows on non-main threads?
	//		GetMessage blocks and we never get wm_paints, even though JS vm is running in the background
	//	gr: this was queueing on OSX. (the main thread stuff on windows has been fixed),
	//		but queueing meant we didn't process include()'s in JS synchronously, which we needed to
	Execute( Exec );
}

template<typename CLOCKTYPE=std::chrono::high_resolution_clock>
class TJob_DefferedUntil : public PopWorker::TJob_Function
{
public:
	TJob_DefferedUntil(std::function<void()>& Functor,typename CLOCKTYPE::time_point FutureTime) :
		TJob_Function	( Functor )
	{
		mFutureTime = FutureTime;
	}
	
	virtual size_t		GetRunDelay() override
	{
		auto Now = CLOCKTYPE::now();
		auto Delay = mFutureTime - Now;
		auto DelayMs = std::chrono::duration_cast<std::chrono::milliseconds>(Delay).count();
		if ( DelayMs <= 0 )
			return 0;
		return DelayMs;
	}
	
	typename CLOCKTYPE::time_point	mFutureTime;
};


void JsCore::TContext::Queue(std::function<void(JsCore::TLocalContext&)> Functor,size_t DeferMs)
{
	if ( !mJobQueue.IsWorking() )
		throw Soy::AssertException("Rejecting job as context is shutting down");

	//	catch negative params
	//	assuming nobody will ever defer for more than 24 hours
	auto TwentyFourHoursMs = 1000*60*60*24;
	if ( DeferMs > TwentyFourHoursMs )
	{
		std::stringstream Error;
		Error << "Queued JsCore job for " << DeferMs << " milliseconds. Capped at 24 hours (" << TwentyFourHoursMs << "). Possible negative value? (" << static_cast<ssize_t>(DeferMs) << ")";
		throw Soy::AssertException( Error.str() );
	}
	
	//	copy the function whilst still in callers thread
	std::shared_ptr<std::function<void(Bind::TLocalContext&)>> LocalCopy( new std::function<void(Bind::TLocalContext&)>(Functor) );
	std::function<void()> FunctorWrapper = [=]()mutable
	{
		//	need to catch this?
		auto& Local = *LocalCopy;
		std::function<void(Bind::TLocalContext&)> WrapperWrapper = [&](Bind::TLocalContext& Context)
		{
			Local( Context );
			LocalCopy.reset();
		};
		Execute_Reference( WrapperWrapper );
		
	};
	
	if ( DeferMs > 0 )
	{
		using CLOCKTYPE = std::chrono::system_clock;
		//auto Now = std::chrono::high_resolution_clock::now();
		auto Now = CLOCKTYPE::now();
		auto FutureTime = Now + std::chrono::milliseconds(DeferMs);
		/*	gr: this is deffering everything
		//	gr: would be nice to make this part of the SoyJobQueue so we skip jobs until a time is reached
		Platform::ExecuteDelayed( FutureTime, PushJob );
		*/
		std::shared_ptr<PopWorker::TJob> Job( new TJob_DefferedUntil<CLOCKTYPE>( FunctorWrapper, FutureTime ) );
		mJobQueue.PushJob( Job );
	}
	else
	{
		mJobQueue.PushJob( FunctorWrapper );
	}
}

#if defined(JSAPI_JSCORE)
void JSObjectTypedArrayDirty(JSContextRef Context,JSObjectRef Object)
{
	
}
#endif

#if defined(JSAPI_JSCORE)
void JSObjectSetPrivate(JSContextRef Context,JSObjectRef Object,void* Data)
{
	JSObjectSetPrivate( Object, Data );
}
#endif

#if defined(JSAPI_JSCORE)
void* JSObjectGetPrivate(JSContextRef Context,JSObjectRef Object)
{
	return JSObjectGetPrivate( Object );
}
#endif

#if defined(JSAPI_JSCORE)
void JSLockAndRun(JSGlobalContextRef GlobalContext,std::function<void(JSContextRef&)> Functor)
{
	//	gr: this may be the source of problems, this should be a properly locally scoped context...
	JSContextRef ContextRef = GlobalContext;
	Functor( ContextRef );
}
#endif

void JsCore::TContext::Execute(std::function<void(JsCore::TLocalContext&)> Functor)
{
	//	calling this func will have caused a copy, and now we have one.
	Execute_Reference( Functor );
}

void JsCore::TContext::Execute_Reference(std::function<void(JsCore::TLocalContext&)>& Functor)
{
	//	gr: lock so only one JS operation happens at a time
	//		doing this to test stability (this also emulates v8 a lot more)
	//	gr: v8 has an isolate locker, so an extra one here causes deadlock
	//		todo: move it into JSLockAndRun for JSCore side?
#if defined(JSAPI_V8)
#else
	std::lock_guard<std::recursive_mutex> Lock(mExecuteLock);
#endif
	
	//	javascript core is threadsafe, so we can just call
	//	but maybe we need to set a javascript exception, if this is
	//	being called from js to relay stuff back
	auto Redirect = [&](JSContextRef& Context)
	{
		JsCore::TLocalContext LocalContext( Context, *this );
		Functor( LocalContext );
	};
	JSLockAndRun( mContext, Redirect );
}


template<typename TYPE>
JsCore::TArray JsCore_CreateArray(JsCore::TLocalContext& Context,size_t ElementCount,std::function<TYPE(size_t)> GetElement)
{
	Array<JSValueRef> Values;
	//JSValueRef Values[ElementCount];
	for ( auto i=0;	i<ElementCount;	i++ )
	{
		auto Element = GetElement(i);
		auto Value = JsCore::GetValue( Context.mLocalContext, Element );
		Values.PushBack(Value);
	}
	auto ArrayObject = JsCore::GetArray( Context.mLocalContext, GetArrayBridge(Values) );
	JsCore::TArray Array( Context.mLocalContext, ArrayObject );
	return Array;
}



JsCore::TObject::TObject(JSContextRef Context,JSObjectRef This) :
	mContext	( Context ),
	mThis		( This )
{
	if ( !mContext )
		throw Soy::AssertException("Null context for TObject");

	if ( !mThis )
		throw Soy::AssertException("This is null for TObject");

#if defined(PROTECT_OBJECT_THIS)
	JSValueProtect(mContext, mThis);
#endif
}

JsCore::TObject::~TObject()
{
#if defined(PROTECT_OBJECT_THIS)
	if (mThis)
	{
		JSValueUnprotect(mContext, mThis);
	}
#endif
}


JsCore::TObject& JsCore::TObject::operator=(const TObject& Copy)
{
	//	release self
#if defined(PROTECT_OBJECT_THIS)
	if (mThis)
	{
		JSValueUnprotect(mContext, mThis);
		mThis = nullptr;
	}
#endif
	mThis = Copy.mThis;
	mContext = Copy.mContext;
#if defined(PROTECT_OBJECT_THIS)
	JSValueProtect(mContext, mThis);
#endif
	return *this;
}

bool JsCore::TObject::IsMemberArray(const std::string& MemberName)
{
	auto Member = GetMember(MemberName);
	return JSValueIsArray(mContext, Member);
}

bool JsCore::TObject::HasMember(const std::string& MemberName)
{
	auto Member = GetMember( MemberName );
	if ( JSValueIsUndefined( mContext, Member ) )
		return false;
	return true;	
}

JSValueRef JsCore::TObject::GetMember(const std::string& MemberName)
{
	//	keep splitting the name so we can get Pop.Input.Cat
	TObject This = *this;

	//	leaf = final name
	auto LeafName = MemberName;
	while ( MemberName.length() > 0 )
	{
		auto ChildName = Soy::StringPopUntil( LeafName, '.', false, false );
		
		if ( LeafName.length() == 0 )
		{
			LeafName = ChildName;
			break;
		}

		auto Child = This.GetObject(ChildName);
		This = Child;
	}

	JSValueRef Exception = nullptr;
	auto Property = JSObjectGetProperty( mContext, This.mThis, LeafName, &Exception );
	//auto PropertyType = JSValueGetType( mContext, Property );
	ThrowException( mContext, Exception );
	return Property;	//	we return null/undefineds
}

JsCore::TObject JsCore::TObject::GetObject(const std::string& MemberName)
{
	auto Value = GetMember( MemberName );
	JSValueRef Exception = nullptr;
	auto Object = JSValueToObject( mContext, Value, &Exception );
	JsCore::ThrowException( mContext, Exception, std::string("Object.GetObject(") + MemberName + ")" );
	return TObject( mContext, Object );
}

std::string JsCore::TObject::GetString(const std::string& MemberName)
{
	auto Value = GetMember( MemberName );
	if ( JSValueIsUndefined(mContext,Value) )
		throw Soy::AssertException( MemberName + " is undefined");

	JSValueRef Exception = nullptr;
	auto StringHandle = JSValueToStringCopy( mContext, Value, &Exception );
	JsCore::ThrowException( mContext, Exception, MemberName );
	auto String = JsCore::GetString( mContext, StringHandle );
	return String;
}

uint32_t JsCore::TObject::GetInt(const std::string& MemberName)
{
	auto Value = GetMember( MemberName );
	if ( JSValueIsUndefined(mContext,Value) )
		throw Soy::AssertException( MemberName + " is undefined");

	JSValueRef Exception = nullptr;
	auto Number = JSValueToNumber( mContext, Value, &Exception );
	JsCore::ThrowException( mContext, Exception, MemberName );
	
	//	convert this double to an int!
	auto ValueInt = static_cast<uint32_t>(Number);
	return ValueInt;
}

float JsCore::TObject::GetFloat(const std::string& MemberName)
{
	auto Value = GetMember( MemberName );
	if ( JSValueIsUndefined(mContext,Value) )
		throw Soy::AssertException( MemberName + " is undefined");

	JSValueRef Exception = nullptr;
	auto Number = JSValueToNumber( mContext, Value, &Exception );
	JsCore::ThrowException( mContext, Exception, MemberName );
	
	//	convert this double to an int!
	auto Valuef = static_cast<float>(Number);
	return Valuef;
}

bool JsCore::TObject::GetBool(const std::string& MemberName)
{
	auto Value = GetMember( MemberName );
	if ( JSValueIsUndefined(mContext,Value) )
		throw Soy::AssertException( MemberName + " is undefined");

	//	gr: add a type check here as there's no exception
	auto Bool = JSValueToBoolean( mContext, Value );
	return Bool;
}

JsCore::TFunction JsCore::TObject::GetFunction(const std::string& MemberName)
{
	auto Object = GetObject(MemberName);
	JsCore::TFunction Func( mContext, JSObjectToValue(Object.mThis) );
	return Func;
}


void JsCore::TObject::SetObjectFromString(const std::string& Name, const std::string& JsonString)
{
	auto Object = JsCore::ParseObjectString( this->mContext, JsonString );
	SetObject(Name, Object);
}

void JsCore::TObject::SetObject(const std::string& Name,const TObject& Object)
{
	SetMember( Name, JSObjectToValue(Object.mThis) );
}

void JsCore::TObject::SetFunction(const std::string& Name,JsCore::TFunction& Function)
{
	SetMember( Name, JSObjectToValue(Function.mThis) );
}

void JsCore::TObject::SetMember(const std::string& Name,JSValueRef Value)
{
	JSPropertyAttributes Attribs = kJSPropertyAttributeNone;
	JSValueRef Exception = nullptr;
	JSObjectSetProperty( mContext, mThis, Name, Value, Attribs, &Exception );
	ThrowException( mContext, Exception );
}

void JsCore::TObject::SetArray(const std::string& Name,JsCore::TArray& Array)
{
	SetMember( Name, JSObjectToValue(Array.mThis) );
}

void JsCore::TObject::SetInt(const std::string& Name, uint32_t Value)
{
	SetMember(Name, GetValue(mContext, Value));
}

void JsCore::TObject::SetNull(const std::string& Name)
{
	SetMember(Name, JSValueMakeNull(mContext));
}

void JsCore::TObject::SetUndefined(const std::string& Name)
{
	SetMember(Name, JSValueMakeUndefined(mContext));
}

void JsCore::TObject::SetFloat(const std::string& Name,float Value)
{
	SetMember( Name, GetValue( mContext, Value ) );
}

void JsCore::TObject::SetString(const std::string& Name,const std::string& Value)
{
	SetMember( Name, GetValue( mContext, Value ) );
}

void JsCore::TObject::SetBool(const std::string& Name,bool Value)
{
	SetMember( Name, GetValue( mContext, Value ) );
}


JsCore::TObject JsCore::TContext::CreateObjectInstance(TLocalContext& LocalContext,const std::string& ObjectTypeName)
{
	BufferArray<JSValueRef,1> FakeArgs;
	return CreateObjectInstance( LocalContext, ObjectTypeName, GetArrayBridge(FakeArgs) );
}

JsCore::TObject JsCore::TContext::CreateObjectInstance(TLocalContext& LocalContext,const std::string& ObjectTypeName,ArrayBridge<JSValueRef>&& ConstructorArguments)
{
	//	create basic object
	if ( ObjectTypeName.length() == 0 || ObjectTypeName == "Object" )
	{
		JSClassRef Default = nullptr;
		void* Data = nullptr;
		auto NewObject = JSObjectMake( LocalContext.mLocalContext, Default, Data );
		return TObject( LocalContext.mLocalContext, NewObject );
	}
	
	//	find template
	auto* pObjectTemplate = mObjectTemplates.Find( ObjectTypeName );
	if ( !pObjectTemplate )
	{
		std::stringstream Error;
		Error << "Unknown object typename ";
		Error << ObjectTypeName;
		auto ErrorStr = Error.str();
		throw Soy::AssertException(ErrorStr);
	}
	
	//	gr: should this create wrapper? or does the constructor do it for us...
	//	gr: this does NOT call the js constructor! maybe I'm calling the wrong thing
	//		but it means we're creating C++Object then JsObject instead of the other way
	//	JSObjectCallAsConstructor to call the constructor
	auto& ObjectTemplate = *pObjectTemplate;
	auto& Class = ObjectTemplate.mClass;

	auto& ObjectPointer = ObjectTemplate.AllocInstance( LocalContext.mGlobalContext );
	void* Data = &ObjectPointer;

	auto NewObjectHandle = JSObjectMake( LocalContext.mLocalContext, Class, Data );
	TObject NewObject( LocalContext.mLocalContext, NewObjectHandle );
	ObjectPointer.SetHandle( LocalContext, NewObject );

	//	this should already be setup in jscore...
#if defined(PERSISTENT_OBJECT_HANDLE)
	ObjectPointer.mHandle.SetWeak( ObjectPointer, Class );
#endif
	
	//	construct
	TCallback ConstructorParams(LocalContext);
	ConstructorParams.mThis = JSObjectToValue( NewObject.mThis );
	ConstructorParams.mArguments.Copy( ConstructorArguments );
	
	//	actually call!
	ObjectPointer.Construct( ConstructorParams );
	
	return NewObject;
}


void JsCore::TContext::ConstructObject(TLocalContext& LocalContext,const std::string& ObjectTypeName,JSObjectRef NewObject,ArrayBridge<JSValueRef>&& ConstructorArguments)
{
	//	find template
	auto* pObjectTemplate = mObjectTemplates.Find( ObjectTypeName );
	if ( !pObjectTemplate )
	{
		std::stringstream Error;
		Error << "Unknown object typename ";
		Error << ObjectTypeName;
		auto ErrorStr = Error.str();
		throw Soy::AssertException(ErrorStr);
	}
	
	auto& ObjectTemplate = *pObjectTemplate;
	auto& Class = ObjectTemplate.mClass;
	auto& ObjectPointer = ObjectTemplate.AllocInstance( LocalContext.mGlobalContext );
	void* Data = &ObjectPointer;
	
	//	v8 needs to manually set the private data
	JSObjectSetPrivate( LocalContext.mLocalContext, NewObject, Data );
	Bind::TObject ObjectHandle( LocalContext.mLocalContext, NewObject );
	ObjectPointer.SetHandle( LocalContext, ObjectHandle );
	
	//	for V8, to get a free() callback, we need a persistent to be marked weak
#if defined(PERSISTENT_OBJECT_HANDLE)
	ObjectPointer.mHandle.SetWeak(ObjectPointer, Class);
	/*
	//	gr: make it weak so it will be collected
	//		anything persistent will be in a TPersistent
	//	https://itnext.io/v8-wrapped-objects-lifecycle-42272de712e0
	 mHandle = v8::Persist
	mHandle.SetWeak( this, OnFree, v8::WeakCallbackType::kInternalFields );
	*/
#endif
	
	
	//	construct
	TCallback ConstructorParams(LocalContext);
	ConstructorParams.mThis = Bind::GetValue(LocalContext.mLocalContext, ObjectHandle);
	ConstructorParams.mArguments.Copy( ConstructorArguments );
	
	//	actually call!
	ObjectPointer.Construct( ConstructorParams );
}

void JsCore::TContext::BindRawFunction(const std::string& FunctionName,const std::string& ParentObjectName,JSObjectCallAsFunctionCallback FunctionPtr)
{
	auto Exec = [&](Bind::TLocalContext& LocalContext)
	{
		auto This = GetGlobalObject( LocalContext, ParentObjectName );

		auto FunctionNameJs = JsCore::GetString( LocalContext.mLocalContext, FunctionName );
		JSValueRef Exception = nullptr;
		auto FunctionHandle = JSObjectMakeFunctionWithCallback( LocalContext.mLocalContext, FunctionNameJs, FunctionPtr );
		ThrowException( LocalContext.mLocalContext, Exception );
		TFunction Function( LocalContext.mLocalContext, FunctionHandle );
		This.SetFunction( FunctionName, Function );
	};
	Execute( Exec );
}


JsCore::TPromise JsCore::TContext::CreatePromise(Bind::TLocalContext& LocalContext,const std::string& DebugName)
{
	if ( !mMakePromiseFunction )
	{
		auto* MakePromiseFunctionSource =  R"V0G0N(
		
		let MakePromise = function()
		{
			let PromData = {};
			const GrabPromData = function(Resolve,Reject)
			{
				PromData.Resolve = Resolve;
				PromData.Reject = Reject;
			};
			const prom = new Promise( GrabPromData );
			PromData.Promise = prom;
			prom.Resolve = PromData.Resolve;
			prom.Reject = PromData.Reject;
			return prom;
		}
		MakePromise;
		//MakePromise();
		)V0G0N";
		
		JSStringRef FunctionSourceString = JsCore::GetString( LocalContext.mLocalContext, MakePromiseFunctionSource );
		JSValueRef Exception = nullptr;
		auto FunctionValue = JSEvaluateScript( LocalContext.mLocalContext, FunctionSourceString, nullptr, nullptr, 0, &Exception );
		ThrowException( LocalContext.mLocalContext, Exception );
		
		TFunction MakePromiseFunction( LocalContext.mLocalContext, FunctionValue );
		mMakePromiseFunction = TPersistent( LocalContext, MakePromiseFunction, "MakePromiseFunction" );
	}
	
	Bind::TCallback CallParams( LocalContext );
	auto MakePromiseFunction = mMakePromiseFunction.GetFunction(LocalContext);
	MakePromiseFunction.Call(CallParams);
	auto NewPromiseValue = CallParams.mReturn;
	auto NewPromiseHandle = JsCore::GetObject( LocalContext.mLocalContext, NewPromiseValue );
	TObject NewPromiseObject( LocalContext.mLocalContext, NewPromiseHandle );
	auto Resolve = NewPromiseObject.GetFunction("Resolve");
	auto Reject = NewPromiseObject.GetFunction("Reject");

	TPromise Promise( LocalContext, NewPromiseObject, Resolve, Reject, DebugName );

	return Promise;
}


std::shared_ptr<JsCore::TPromise> JsCore::TContext::CreatePromisePtr(Bind::TLocalContext& LocalContext, const std::string& DebugName)
{
	if (!mMakePromiseFunction)
	{
		auto* MakePromiseFunctionSource = R"V0G0N(
		
		let MakePromise = function()
		{
			let PromData = {};
			const GrabPromData = function(Resolve,Reject)
			{
				PromData.Resolve = Resolve;
				PromData.Reject = Reject;
			};
			const prom = new Promise( GrabPromData );
			PromData.Promise = prom;
			prom.Resolve = PromData.Resolve;
			prom.Reject = PromData.Reject;
			return prom;
		}
		MakePromise;
		//MakePromise();
		)V0G0N";

		JSStringRef FunctionSourceString = JsCore::GetString(LocalContext.mLocalContext, MakePromiseFunctionSource);
		JSValueRef Exception = nullptr;
		auto FunctionValue = JSEvaluateScript(LocalContext.mLocalContext, FunctionSourceString, nullptr, nullptr, 0, &Exception);
		ThrowException(LocalContext.mLocalContext, Exception);

		TFunction MakePromiseFunction(LocalContext.mLocalContext, FunctionValue);
		mMakePromiseFunction = TPersistent(LocalContext, MakePromiseFunction, "MakePromiseFunction");
	}

	Bind::TCallback CallParams(LocalContext);
	auto MakePromiseFunction = mMakePromiseFunction.GetFunction(LocalContext);
	MakePromiseFunction.Call(CallParams);
	auto NewPromiseValue = CallParams.mReturn;
	auto NewPromiseHandle = JsCore::GetObject(LocalContext.mLocalContext, NewPromiseValue);
	TObject NewPromiseObject(LocalContext.mLocalContext, NewPromiseHandle);
	auto Resolve = NewPromiseObject.GetFunction("Resolve");
	auto Reject = NewPromiseObject.GetFunction("Reject");

	auto Promise = std::make_shared<TPromise>( LocalContext, NewPromiseObject, Resolve, Reject, DebugName);

	return Promise;
}


JSValueRef JsCore::TContext::CallFunc(TLocalContext& LocalContext,std::function<void(JsCore::TCallback&)> Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception,const std::string& FunctionContext)
{
	//	call our function from
	try
	{
		//	if context has gone, we might be shutting down
		if ( !mContext )
			throw Soy::AssertException("CallFunc: Context is null, maybe shutting down");

		TCallback Callback(LocalContext);
		Callback.mThis = JSObjectToValue(This);
	
		if ( !Callback.mThis )
			Callback.mThis = JSValueMakeUndefined( LocalContext.mLocalContext );

		for ( auto a=0;	a<ArgumentCount;	a++ )
			Callback.mArguments.PushBack( Arguments[a] );

		//	actually call!
		Function( Callback );
		
		if ( !Callback.mReturn )
			Callback.mReturn = JSValueMakeUndefined( LocalContext.mLocalContext );
		
		return Callback.mReturn;
	}
	catch (std::exception& e)
	{
		std::stringstream Error;
		Error << FunctionContext << " exception: " << e.what();
		std::Debug << Error.str() << std::endl;
		Exception = GetValue( LocalContext.mLocalContext, Error.str() );
		return JSValueMakeUndefined( LocalContext.mLocalContext );
	}
}

bool JsCore::TCallback::IsArgumentArrayU8(size_t Index)
{
	if (Index >= mArguments.GetSize())
		return false;

	auto Context = GetContextRef();
	auto Handle = mArguments[Index];
	JSValueRef Exception = nullptr;
	auto TypedArrayType = JSValueGetTypedArrayType(Context, Handle, &Exception);
	JsCore::ThrowException(Context, Exception, __PRETTY_FUNCTION__);

	if (TypedArrayType != kJSTypedArrayTypeUint8Array)
		return false;

	return true;
}

JsCore::TObject JsCore::TCallback::GetReturnObject()
{
	auto ContextRef = GetContextRef();
	auto ObjectRef = GetObject( ContextRef, mReturn );
	return TObject( ContextRef, ObjectRef );
}

JsCore::TFunction JsCore::TCallback::GetReturnFunction()
{
	auto ContextRef = GetContextRef();
	//auto FunctionRef = GetFunction( ContextRef, mReturn );
	return TFunction( ContextRef, mReturn );
}

JSContextRef JsCore::TCallback::GetContextRef()
{
	return mLocalContext.mLocalContext;
}

JSType JsCore::TCallback::GetArgumentType(size_t Index)
{
	if ( Index >= mArguments.GetSize() )
		return kJSTypeUndefined;
		
	auto Type = JSValueGetType( GetContextRef(), mArguments[Index] );
	return Type;
}

JSValueRef JsCore::TCallback::GetArgumentValue(size_t Index)
{
	if ( Index >= mArguments.GetSize() )
		return JSValueMakeUndefined( GetContextRef() );
	return mArguments[Index];
}

JSValueRef JsCore::TCallback::GetArgumentValueNotUndefined(size_t Index)
{
	if ( Index >= mArguments.GetSize() )
	{
		std::stringstream Error;
		Error << "Argument " << Index << " is undefined (" << Index << "/" << mArguments.GetSize() << ")";
		throw Soy::AssertException(Error);
	}
	
	auto Value = mArguments[Index];
	if ( JSValueIsUndefined( GetContextRef(), Value ) )
	{
		std::stringstream Error;
		Error << "Argument " << Index << " is undefined";
		throw Soy::AssertException(Error);
	}
		
	return Value;
}

std::string JsCore::TCallback::GetArgumentString(size_t Index)
{
	auto Handle = GetArgumentValueNotUndefined( Index );
	auto String = JsCore::GetString( GetContextRef(), Handle );
	return String;
}

std::string JsCore::TCallback::GetArgumentFilename(size_t Index)
{
	auto Filename = GetArgumentString(Index);
	Filename = mContext.GetResolvedFilename( Filename );
	return Filename;
}

JsCore::TFunction JsCore::TCallback::GetArgumentFunction(size_t Index)
{
	auto Handle = GetArgumentValueNotUndefined( Index );
	JsCore::TFunction Function( GetContextRef(), Handle );
	return Function;
}

JsCore::TArray JsCore::TCallback::GetArgumentArray(size_t Index)
{
	auto Handle = GetArgumentValueNotUndefined( Index );
	auto HandleObject = JsCore::GetObject( GetContextRef(), Handle );
	JsCore::TArray Array( GetContextRef(), HandleObject );
	return Array;
}

bool JsCore::TCallback::GetArgumentBool(size_t Index)
{
	auto Handle = GetArgumentValueNotUndefined( Index );
	auto Value = JsCore::GetBool( GetContextRef(), Handle );
	return Value;
}

float JsCore::TCallback::GetArgumentFloat(size_t Index)
{
	auto Handle = GetArgumentValueNotUndefined( Index );
	auto Value = JsCore::GetFloat( GetContextRef(), Handle );
	return Value;
}

JsCore::TObject JsCore::TCallback::GetArgumentObject(size_t Index)
{
	auto Handle = GetArgumentValueNotUndefined( Index );
	auto HandleObject = JsCore::GetObject( GetContextRef(), Handle );
	return JsCore::TObject( GetContextRef(), HandleObject );
}

bool JsCore::IsArray(JSContextRef Context,JSObjectRef Handle)
{
	return IsArray( Context, JSObjectToValue(Handle) );
}

bool JsCore::IsArray(JSContextRef Context,JSValueRef Handle)
{
	//	typed array is not an official js array, but is to us
	JSValueRef Exception = nullptr;
	auto TypedArrayType = JSValueGetTypedArrayType( Context, Handle, &Exception );
	JsCore::ThrowException( Context, Exception, "Testing if value is typed array" );
	
	//	we're a typed array
	if ( TypedArrayType != kJSTypedArrayTypeNone )
	{
		return true;
	}
	
	//	we're a regular array
	if ( JSValueIsArray( Context, Handle ) )
	{
		return true;
	}
	
	return false;
}


bool JsCore::IsFunction(JSContextRef Context,JSValueRef Handle)
{
	if ( !JSValueIsObject( Context, Handle ) )
		return false;
	
	auto Object = GetObject( Context, Handle );
	
	if ( !JSObjectIsFunction( Context, Object ) )
		return false;
	
	return true;
}


void JsCore::TCallback::Return(JsCore::TPersistent& Value)
{
	mReturn = GetValue( GetContextRef(), Value );
}

void JsCore::TCallback::Return(JsCore::TPromise& Value)
{
	mReturn = GetValue( GetContextRef(), Value );
}

void JsCore::TCallback::ReturnNull()
{
	mReturn = JSValueMakeNull( GetContextRef() );
}

void JsCore::TCallback::ReturnUndefined()
{
	mReturn = JSValueMakeUndefined( GetContextRef() );
}


JsCore::TObject JsCore::TCallback::ThisObject()
{
	auto Object = GetObject( GetContextRef(), mThis );
	return TObject( GetContextRef(), Object );
}


void JsCore::TCallback::SetThis(JsCore::TObject& This)
{
	mThis = JSObjectToValue(This.mThis);
}

template<typename TYPE>
void JSCore_SetArgument(Array<JSValueRef>& mArguments,Bind::TLocalContext& LocalContext,size_t Index,const TYPE& Value)
{
	while ( mArguments.GetSize() <= Index )
		mArguments.PushBack( JSValueMakeUndefined(LocalContext.mLocalContext) );
	
	mArguments[Index] = JsCore::GetValue( LocalContext.mLocalContext, Value );
}

void JsCore::TCallback::SetArgument(size_t Index,JSValueRef Value)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Value );
}

void JsCore::TCallback::SetArgumentString(size_t Index,const std::string& Value)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Value );
}

void JsCore::TCallback::SetArgumentInt(size_t Index,uint32_t Value)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Value );
}

void JsCore::TCallback::SetArgumentInt(size_t Index,int32_t Value)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Value );
}

void JsCore::TCallback::SetArgumentBool(size_t Index,bool Value)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Value );
}

void JsCore::TCallback::SetArgumentObject(size_t Index,JsCore::TObject& Value)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Value );
}

void JsCore::TCallback::SetArgumentFunction(size_t Index,JsCore::TFunction& Value)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Value );
}

void JsCore::TCallback::SetArgumentArray(size_t Index,ArrayBridge<std::string>&& Values)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Values );
}

void JsCore::TCallback::SetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Values)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Values );
}

void JsCore::TCallback::SetArgumentArray(size_t Index, ArrayBridge<float>&& Values)
{
	JSCore_SetArgument(mArguments, mLocalContext, Index, Values);
}

void JsCore::TCallback::SetArgumentArray(size_t Index, ArrayBridge<JsCore::TObject>&& Values)
{
	JSCore_SetArgument(mArguments, mLocalContext, Index, Values);
}

void JsCore::TCallback::SetArgumentArray(size_t Index,JsCore::TArray& Value)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Value );
}


JsCore::TObject JsCore::TContext::GetGlobalObject(TLocalContext& LocalContext,const std::string& ObjectName)
{
	auto GlobalThis = JSContextGetGlobalObject( LocalContext.mLocalContext );
	TObject Global( LocalContext.mLocalContext, GlobalThis );
	
	if ( ObjectName.length() == 0 )
		return Global;
	auto Child = Global.GetObject( ObjectName );
	return Child;
}


void JsCore::TContext::CreateGlobalObjectInstance(const std::string& ObjectType,const std::string& Name)
{
	auto Exec = [=](Bind::TLocalContext& LocalContext)
	{
		auto NewObject = CreateObjectInstance( LocalContext, ObjectType );
		auto ParentName = Name;
		auto ObjectName = Soy::StringPopRight( ParentName, '.' );
		auto ParentObject = GetGlobalObject( LocalContext, ParentName );
		ParentObject.SetObject( ObjectName, NewObject );
	};
	Execute( Exec );
}


std::string JsCore::TContext::GetResolvedFilename(const std::string& Filename)
{
	//	gr: expecting this to succeed even if the file doesn't exist
	if ( Platform::IsFullPath(Filename) )
		return Filename;

	//	gr: do this better!
	//	gr: should be able to use NSUrl to resolve ~/ or / etc
	if ( Filename[0] == '/' )
		return Filename;
	
	std::stringstream FullFilename;
	FullFilename << mRootDirectory << Filename;
	return FullFilename.str();
}


JsCore::TPersistent::~TPersistent()
{
	Release();
}

void JsCore::TPersistent::SetWeak(TObjectWrapperBase& Object,JSClassRef Class)
{
#if defined(JSAPI_V8)
	if ( !mObject )
		throw Soy::AssertException("Trying to make a null persistent weak. Currently this is only used at instantiate time");

	if ( !Class.mDestructor )
		throw Soy::AssertException("Trying to make persistent weak, but no destructor callback");
	
	//	we're using the internal fields for storage, so we don't pass any data around
	//	perhaps we need to change this to pass the explicitly allocated object from the construction, but this SHOULD all sync
	//void* Param = nullptr;
	//auto CallbackParam = v8::WeakCallbackType::kInternalFields;
	//	gr: internal field approach just gave nulls, so pass the object
	void* Param = &Object;
	auto CallbackParam = v8::WeakCallbackType::kParameter;
	mObject->mPersistent.SetWeak( Param, Class.mDestructor, CallbackParam );
#endif
#if defined(JSAPI_CHAKRA)
	JSGlobalContextRef Context = nullptr;
	//	gr: this seems dangerous, but there's no explicit weak mode...
	//		just remove one ref count so last use outside the handle deallocs.
	JSValueUnprotect(Context,mObject);
#endif
}
	
	
JsCore::TFunction JsCore::TPersistent::GetFunction(Bind::TLocalContext& Context) const
{
	auto Object = GetObject( Context );
	return Bind::TFunction( Context.mLocalContext, Object.mThis );
}


JsCore::TObject JsCore::TPersistent::GetObject(TLocalContext& Context) const
{
#if defined(JSAPI_V8)
	if ( !mObject )
		return TObject();
	auto Local = this->mObject->GetLocal( Context.mLocalContext.GetIsolate() );
	JSObjectRef LocalValue( Local );
	return Bind::TObject( Context.mLocalContext, LocalValue );
#else
	//	we should ignore the object's context and always use it in a current context
	return TObject( Context.mLocalContext, mObject );
#endif
	/*
	if ( mObject.mContext != Context.mLocalContext )
	{
		//	gr: I think context can change when the context is a lexical (in a promise/await/lambda)
		//		the object stays the same, but not sure if the global/context matters
		//		what WILL matter is the protect/release?
		//std::Debug << "Context has changed" << std::endl;
		static bool ChangeObjectContext = false;
		if ( ChangeObjectContext )
		{
			mObject.mContext = Context.mLocalContext;
		}
		return TObject( Context.mLocalContext, mObject.mThis );
	}
	
	return mObject;
	 */
}

void JsCore::TPersistent::Retain(JSGlobalContextRef Context,JSObjectRef ObjectOrFunc,const std::string& DebugName)
{
	//std::Debug << "Retain context=" << Context << " object=" << ObjectOrFunc << " " << DebugName << std::endl;
	JSValueProtect( Context, JSObjectToValue(ObjectOrFunc) );
}


void JsCore::TPersistent::Release(JSGlobalContextRef Context,JSObjectRef ObjectOrFunc,const std::string& DebugName)
{
	//std::Debug << "Release context=" << Context << " object=" << ObjectOrFunc << " " << DebugName << std::endl;
	JSValueUnprotect( Context, JSObjectToValue(ObjectOrFunc) );
}


void JsCore::TPersistent::Release()
{
	//	can only get context if there is an object
	if ( mObject )
	{
		if ( !mContext )
			throw Soy::AssertException("Has object, but no context");
	}
	
	if ( mObject )
	{
#if defined(JSAPI_V8)
		mContext->OnPersitentReleased(*this);
		mObject.reset();
#else
		Release( mRetainedContext, mObject, mDebugName );
		mContext->OnPersitentReleased(*this);
		mObject = nullptr;
#endif
		mRetainedContext = nullptr;
		mContext = nullptr;
	}
	
}



void JsCore::TPersistent::Retain(TLocalContext& Context,const TObject& Object,const std::string& DebugName)
{
	if ( mObject )
	{
		//std::Debug << std::string("Overwriting existing retain ") << mDebugName << std::string(" to ") << DebugName << std::endl;
		//	throw Soy::AssertException( std::string("Overwriting existing retain ") + mDebugName + std::string(" to ") + DebugName );
		Release();
	}
	
	mDebugName = DebugName;
	mContext = &Context.mGlobalContext;
	mRetainedContext = JSContextGetGlobalContext(Context.mLocalContext);
#if defined(JSAPI_V8)
	mObject = V8::GetPersistent( Context.mLocalContext.GetIsolate(), Object.mThis.mThis );
#else
	mObject = Object.mThis;
	Retain( mRetainedContext, mObject, mDebugName );
#endif

	Context.mGlobalContext.OnPersitentRetained(*this);
}

void JsCore::TPersistent::Retain(TLocalContext& Context,const TFunction& Function,const std::string& DebugName)
{
	Bind::TObject FunctionObject( Context.mLocalContext, Function.mThis );
	Retain( Context, FunctionObject, DebugName );
}

void JsCore::TPersistent::Retain(const TPersistent& That)
{
	if ( this == &That )
		throw Soy::AssertException("Trying to retain self");
	
	//	gr: this was not calling ANY retain() with That, so wasn't releasing anything!
	Release();
	
	//	array of promises causes = copies, which means = TPromise() which meant copying null
	//	bail here rather than send null objects to JS
	//	this hasn't ocurred for a year or so but did when I added PromiseMap
	if (!That.mObject)
		return;

	
	mDebugName = That.mDebugName + " (copy)";
	mContext = That.mContext;
	mRetainedContext = That.mRetainedContext;
	mObject = That.mObject;
#if !defined(JSAPI_V8)
	Retain( mRetainedContext, mObject, mDebugName );
#endif
	
	if ( mContext && mObject )
		mContext->OnPersitentRetained(*this);
}

#if defined(JSAPI_JSCORE)
JSObjectRef	JSObjectMakeTypedArrayWithBytesWithCopy(JSContextRef Context, JSTypedArrayType ArrayType, const uint8_t* ExternalBuffer, size_t ExternalBufferSize, JSValueRef* Exception)
{
	//	JSObjectMakeTypedArrayWithBytesNoCopy makes an externally backed array, which has a destruction callback
	static JSTypedArrayBytesDeallocator Dealloc = [](void* pArrayBuffer, void* DeallocContext)
	{
		auto* ArrayBuffer = static_cast<uint8_t*>(pArrayBuffer);
		delete[] ArrayBuffer;
	};

	//	allocate an array to dealloc

	//	gr: want to do it on a heap, but our heap needs a size, + context + array and we can only pass 1 contextually variable
	auto* AllocatedBuffer = new uint8_t[ExternalBufferSize];
	auto AllocatedBufferSize = ExternalBufferSize;
	memcpy(AllocatedBuffer, ExternalBuffer, ExternalBufferSize);

	//	safely copy from values
	size_t AllocatedArrayCount = 0;
	auto AllocatedArray = GetRemoteArray(AllocatedBuffer, AllocatedBufferSize, AllocatedArrayCount);
	//AllocatedArray.Copy(Values);

	//	make externally backed array that'll dealloc
	void* DeallocContext = nullptr;
	void* AllocatedBufferMutable = const_cast<uint8_t*>(AllocatedBuffer);
	auto ArrayObject = JSObjectMakeTypedArrayWithBytesNoCopy(Context, ArrayType, AllocatedBufferMutable, AllocatedBufferSize, Dealloc, DeallocContext, Exception);
	if(Exception )
		JsCore::ThrowException(Context, *Exception);

	return ArrayObject;
}
#endif



template<typename TYPE>
JSObjectRef JsCore_GetTypedArray(JSContextRef Context,const ArrayBridge<TYPE>& Values,JSTypedArrayType TypedArrayType)
{
	auto* ExternalBuffer = reinterpret_cast<const uint8_t*>(Values.GetArray());
	auto ExternalBufferSize = Values.GetDataSize();

	//	make externally backed array that'll dealloc itself
	JSValueRef Exception = nullptr;
	auto ArrayObject = JSObjectMakeTypedArrayWithBytesWithCopy( Context, TypedArrayType, ExternalBuffer, ExternalBufferSize, &Exception );
	JsCore::ThrowException( Context, Exception );
	
	return ArrayObject;
}

JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<uint8_t>& Values)
{
	return JsCore_GetTypedArray( Context, Values, kJSTypedArrayTypeUint8Array );
}


JSObjectRef JsCore::GetArray(JSContextRef Context, const ArrayBridge<uint16_t>& Values)
{
	return JsCore_GetTypedArray(Context, Values, kJSTypedArrayTypeUint16Array);
}

JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<uint32_t>& Values)
{
	return JsCore_GetTypedArray( Context, Values, kJSTypedArrayTypeUint32Array );
}

JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<float>& Values)
{
	static_assert( sizeof(float) == 32/8, "Float is not 32 bit. Could support both here...");
	return JsCore_GetTypedArray( Context, Values, kJSTypedArrayTypeFloat32Array );
}

JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<JSValueRef>& Values)
{
	auto Size = Values.GetSize();
	static auto WarningArraySize = 3000;
	if ( Size > WarningArraySize )
	{
		//std::stringstream Error;
		auto& Error = std::Debug;
		Error << "Warning: Javascript core seems to have problems (crashing/corruption) with large arrays; " << Size << "/" << WarningArraySize << std::endl;
		//throw Soy::AssertException( Error.str() );
		//Size = WarningArraySize;
	}
	
	JSValueRef Exception = nullptr;
	auto ArrayObject = JSObjectMakeArray( Context, Values.GetSize(), Values.GetArray(), &Exception );
	ThrowException( Context, Exception );
	return ArrayObject;
}


JsCore::TArray::TArray(JSContextRef Context,JSObjectRef Object) :
	mContext	( Context ),
	mThis		( Object )
{
	if ( !IsArray( mContext, Object ) )
	{
		std::stringstream Error;
		Error << "Object is not array";
		throw Soy::AssertException( Error.str() );
	}
}


void JsCore::TArray::Set(size_t Index,JsCore::TObject& Object)
{
	JSValueRef Exception = nullptr;
	JSObjectSetPropertyAtIndex( mContext, mThis, Index, JSObjectToValue(Object.mThis), &Exception );
	ThrowException( mContext, Exception );
}

template<typename SRCTYPE,typename DSTTYPE>
void CopyArray(void* SrcPtrVoid,size_t SrcCount,ArrayBridge<DSTTYPE>& Dst)
{
	auto* SrcPtr = reinterpret_cast<SRCTYPE*>( SrcPtrVoid );
	auto SrcArray = GetRemoteArray<SRCTYPE>( SrcPtr, SrcCount );
	Dst.Copy( SrcArray );
}

template<typename DSTTYPE>
void CopyTypedArray(JSContextRef Context,JSObjectRef ArrayValue,JSTypedArrayType TypedArrayType,ArrayBridge<DSTTYPE>& DestArray)
{
	JSValueRef Exception = nullptr;
	void* SrcPtr = JSObjectGetTypedArrayBytesPtr( Context, ArrayValue, &Exception );
	auto* SrcPtr8 = reinterpret_cast<uint8_t*>( SrcPtr );
	JsCore::ThrowException( Context, Exception );

	//	offset!
	auto SrcByteOffset = JSObjectGetTypedArrayByteOffset( Context, ArrayValue, &Exception );
	JsCore::ThrowException( Context, Exception );
	SrcPtr8 += SrcByteOffset;
	SrcPtr = SrcPtr8;
	
	auto SrcCount = JSObjectGetTypedArrayLength( Context, ArrayValue, &Exception );
	JsCore::ThrowException( Context, Exception );
	auto SrcBytes = JSObjectGetTypedArrayByteLength( Context, ArrayValue, &Exception );
	JsCore::ThrowException( Context, Exception );

	switch ( TypedArrayType )
	{
		case kJSTypedArrayTypeInt8Array:			CopyArray<int8_t>( SrcPtr, SrcCount, DestArray );	return;
		case kJSTypedArrayTypeInt16Array:			CopyArray<int16_t>( SrcPtr, SrcCount, DestArray );	return;
		case kJSTypedArrayTypeInt32Array:			CopyArray<int32_t>( SrcPtr, SrcCount, DestArray );	return;
		case kJSTypedArrayTypeUint8Array:			CopyArray<uint8_t>( SrcPtr, SrcCount, DestArray );	return;
		case kJSTypedArrayTypeUint8ClampedArray:	CopyArray<uint8_t>( SrcPtr, SrcCount, DestArray );	return;
		case kJSTypedArrayTypeUint16Array:			CopyArray<uint16_t>( SrcPtr, SrcCount, DestArray );	return;
		case kJSTypedArrayTypeUint32Array:			CopyArray<uint32_t>( SrcPtr, SrcCount, DestArray );	return;
		case kJSTypedArrayTypeFloat32Array:			CopyArray<float>( SrcPtr, SrcCount, DestArray );	return;
		
		default:
		//case kJSTypedArrayTypeFloat64Array:	CopyArray<int8_t>( SrcPtr, SrcCount, DestArray );	return;
		//case kJSTypedArrayTypeArrayBuffer:	CopyArray<int8_t>( SrcPtr, SrcCount, DestArray );	return;
			break;
	}
	
	throw Soy::AssertException("Unsupported typed array type");
}


template<typename DESTTYPE>
void JsCore_TArray_CopyTo(JsCore::TArray& This,ArrayBridge<DESTTYPE>& Values)
{
	auto& mContext = This.mContext;
	auto& mThis = This.mThis;

	//	check for typed array
	{
		JSValueRef Exception = nullptr;
		auto TypedArrayType = JSValueGetTypedArrayType( mContext, mThis, &Exception );
		JsCore::ThrowException( mContext, Exception );
		if ( TypedArrayType != kJSTypedArrayTypeNone )
		{
			CopyTypedArray( mContext, mThis, TypedArrayType, Values );
			return;
		}
	}
	
	//	proper way, but will include "named" indexes...
	auto Keys = JSObjectCopyPropertyNames( mContext, mThis );
	auto KeyCount = JSPropertyNameArrayGetCount( Keys );
	for ( auto k=0;	k<KeyCount;	k++ )
	{
		auto Key = JSPropertyNameArrayGetNameAtIndex( Keys, k );
		JSValueRef Exception = nullptr;
		auto Value = JSObjectGetProperty( mContext, mThis, Key, &Exception );
		JsCore::ThrowException( mContext, Exception );
		Values.PushBack( JsCore::FromValue<DESTTYPE>( mContext, Value ) );
	}
}

void JsCore::TArray::CopyTo(ArrayBridge<bool>& Values)
{
	JsCore_TArray_CopyTo( *this, Values );
}

void JsCore::TArray::CopyTo(ArrayBridge<uint32_t>& Values)
{
	JsCore_TArray_CopyTo( *this, Values );
}

void JsCore::TArray::CopyTo(ArrayBridge<int32_t>& Values)
{
	JsCore_TArray_CopyTo( *this, Values );
}

void JsCore::TArray::CopyTo(ArrayBridge<uint8_t>& Values)
{
	JsCore_TArray_CopyTo( *this, Values );
}

void JsCore::TArray::CopyTo(ArrayBridge<float>& Values)
{
	JsCore_TArray_CopyTo( *this, Values );
}

void JsCore::TArray::CopyTo(ArrayBridge<std::string>& Values)
{
	JsCore_TArray_CopyTo( *this, Values );
}


void JsCore::TTemplate::RegisterClassWithContext(TLocalContext& Context,const std::string& ParentObjectName,const std::string& OverrideLeafName)
{
	//	add a terminator function
	JSStaticFunction NewFunction = { nullptr, nullptr, kJSPropertyAttributeNone };
	mFunctions.PushBack(NewFunction);
	mDefinition.staticFunctions = mFunctions.GetArray();
	mClass = JSClassCreate( Context.mLocalContext, mDefinition );

	//	catch this failing, if this isn't "valid" then JsObjectMake will just make a dumb object and it won't be obvious that it's not a constructor
	if ( !mClass )
		throw Soy::AssertException("Registering class failed, invalid class object");
	JSClassRetain( mClass );
	
	//	gr: this works, but logic seems a little odd to me
	//		you create an object, representing the class, and set it on the object like
	//		Parent.YourClass = function()
	//	but JsObjectMake also creates objects...
	
	//	property of Parent (eg. global or Pop.x) can be overridden in case we want a nicer class name than the unique one
	std::string PropertyName = mDefinition.className;
	if ( OverrideLeafName.length() )
		PropertyName = OverrideLeafName;
	
	auto ParentObject = Context.mGlobalContext.GetGlobalObject( Context, ParentObjectName );

	//	gr: if you pass null as the parent object, this "class" gets garbage collected and free'd (with null)
	if ( !ParentObject.mThis )
	{
		std::stringstream Error;
		Error << "Creating class (" << mDefinition.className << ") with null parent(\"" << ParentObjectName << "\") will get auto garbage collected";
		throw Soy::AssertException(Error);
	}
	JSObjectRef ClassObject = JSObjectMake( Context.mLocalContext, mClass, nullptr );
	
	JSValueRef Exception = nullptr;
	JSObjectSetProperty( Context.mLocalContext, ParentObject.mThis, PropertyName, JSObjectToValue(ClassObject), kJSPropertyAttributeNone, &Exception );
	ThrowException( Context.mLocalContext, Exception );
}

JsCore::TPromise::TPromise(Bind::TLocalContext& Context,TObject& Promise,TFunction& Resolve,TFunction& Reject,const std::string& DebugName) :
	mPromise	( Context, Promise, DebugName + "(Promise)" ),
	mResolve	( Context, Resolve, DebugName + "(Resolve)" ),
	mReject		( Context, Reject, DebugName + "(Reject)" ),
	mDebugName	( DebugName )
{
}

JsCore::TPromise::~TPromise()
{
	
}


void JsCore::TPromise::Resolve(TLocalContext& LocalContext,JSObjectRef Value) const
{
	Resolve( LocalContext, JSObjectToValue(Value) );
}

void JsCore::TPromise::Resolve(TLocalContext& LocalContext,JSValueRef Value) const
{
	//	gr: this should be a Queue'd call!
	auto Resolve = mResolve.GetFunction(LocalContext);
	try
	{
		//	gr: should This be the promise object?
		Bind::TCallback Params( LocalContext );
		Params.SetArgument( 0, Value );
		Resolve.Call( Params );
	}
	catch(std::exception& e)
	{
		std::stringstream Error;
		Error << "Error executing promise " << mDebugName << ": " << e.what();
		throw Soy::AssertException(Error.str());
	}
	catch(...)
	{
		std::stringstream Error;
		Error << "Error executing promise " << mDebugName << " (unknown exception)";
		throw Soy::AssertException(Error.str());
	}
}

void JsCore::TPromise::ResolveUndefined(Bind::TLocalContext& Context) const
{
	auto Value = JSValueMakeUndefined( Context.mLocalContext );
	Resolve( Context, Value );
}

void JsCore::TPromise::Reject(Bind::TLocalContext& Context,JSValueRef Value) const
{
	Bind::TCallback Params( Context );
	Params .SetArgument( 0, Value );

	auto Reject = mReject.GetFunction(Context);
	Reject.Call( Params );
}


JsCore::TObject JsCore::TObjectWrapperBase::GetHandle(Bind::TLocalContext& Context)
{
#if defined(PERSISTENT_OBJECT_HANDLE)
	return mHandle.GetObject( Context );
#else
	//	gr: always correct context, like persistent, but this cant be persistent or it won't get garbage collected
	return TObject( Context.mLocalContext, mHandle.mThis );
#endif
}

void JsCore::TObjectWrapperBase::SetHandle(Bind::TLocalContext& Context,JsCore::TObject& NewHandle)
{
#if defined(PERSISTENT_OBJECT_HANDLE)
	mHandle = Bind::TPersistent( Context, NewHandle, "This/SetHandle" );
#else
	mHandle = NewHandle;
#endif
}


void JsCore::TContextDebug::OnPersitentRetained(TPersistent& Persistent)
{
	mPersistentObjectCount[Persistent.GetDebugName()]++;
}

void JsCore::TContextDebug::OnPersitentReleased(TPersistent& Persistent)
{
	mPersistentObjectCount[Persistent.GetDebugName()]--;
}


bool JsCore::TJobQueue::Iteration(std::function<void(std::chrono::milliseconds)> Sleep)
{
	auto IterationResult = true;

	//	if the main job queue sleeps, lets flush any microtasks first
	auto JobSleep = [&](std::chrono::milliseconds Ms)
	{
		//	gr: don't seem to need to
		//if ( !mOnIteration(Sleep) )
		//	IterationResult = false;
		Sleep( Ms );
	};

	if ( !SoyWorkerJobThread::Iteration(JobSleep) )
		return false;

	//	gr: run microtasks last
	if ( !mOnIteration(Sleep) )
		IterationResult = false;

	return IterationResult;
}



template<typename TYPE>
TYPE* JsCore::GetPointer(JSContextRef Context,JSValueRef Handle)
{
	//	type check!
	JSValueRef Exception = nullptr;
	auto ArrayType = JSValueGetTypedArrayType( Context, Handle, &Exception );
	Bind::ThrowException( Context, Exception, __FUNCTION__ );
	auto ExpectedArrayType = GetTypedArrayType<TYPE>();
	
	if ( ArrayType != ExpectedArrayType )
	{
		std::stringstream Error;
		Error << "Expected typed array of " << ExpectedArrayType << " but is " << ArrayType;
		throw Soy::AssertException(Error);
	}
	
	auto ArrayValue = JsCore::GetObject(Context,Handle);
	
	//	unsafe land!
	//	get pointer
	void* SrcPtr = JSObjectGetTypedArrayBytesPtr( Context, ArrayValue, &Exception );
	auto* SrcPtr8 = reinterpret_cast<uint8_t*>( SrcPtr );
	JsCore::ThrowException( Context, Exception );
	
	//	offset!
	auto SrcByteOffset = JSObjectGetTypedArrayByteOffset( Context, ArrayValue, &Exception );
	JsCore::ThrowException( Context, Exception );
	SrcPtr8 += SrcByteOffset;
	SrcPtr = SrcPtr8;
	
	return reinterpret_cast<TYPE*>(SrcPtr);
}

uint8_t* JsCore::GetPointer_u8(JSContextRef Context,JSValueRef Handle)
{
	return GetPointer<uint8_t>( Context, Handle );
}

uint16_t* JsCore::GetPointer_u16(JSContextRef Context,JSValueRef Handle)
{
	return GetPointer<uint16_t>( Context, Handle );
}

uint32_t* JsCore::GetPointer_u32(JSContextRef Context,JSValueRef Handle)
{
	return GetPointer<uint32_t>( Context, Handle );
}


int8_t* JsCore::GetPointer_s8(JSContextRef Context,JSValueRef Handle)
{
	return GetPointer<int8_t>( Context, Handle );
}

int16_t* JsCore::GetPointer_s16(JSContextRef Context,JSValueRef Handle)
{
	return GetPointer<int16_t>( Context, Handle );
}

int32_t* JsCore::GetPointer_s32(JSContextRef Context,JSValueRef Handle)
{
	return GetPointer<int32_t>( Context, Handle );
}



float* JsCore::GetPointer_float(JSContextRef Context,JSValueRef Handle)
{
	return GetPointer<float>( Context, Handle );
}

void JsCore::OnValueChangedExternally(JSContextRef Context,JSValueRef Value)
{
	JSValueRef Exception = nullptr;
	auto TypedArrayType = JSValueGetTypedArrayType( Context, Value, &Exception );
	JsCore::ThrowException( Context, Exception, "OnValueChangedExternally Testing if value is typed array" );
	
	if ( TypedArrayType == kJSTypedArrayTypeNone )
		return;
	
	//	mark typed array as changed
	auto Object = GetObject( Context, Value );
	JSObjectTypedArrayDirty( Context, Object );
}

