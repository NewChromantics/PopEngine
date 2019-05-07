//#include "JsCoreBind.h"
#include "TBind.h"
#include "SoyAssert.h"
#include "SoyFilesystem.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"
#include "TApiMedia.h"
#include "TApiWebsocket.h"
#include "TApiSocket.h"
#include "TApiHttp.h"
#include "TApiSerial.h"
#include "TApiVarjo.h"

#if !defined(PLATFORM_WINDOWS)
//#include "TApiOpencl.h"
#include "TApiDlib.h"
#include "TApiCoreMl.h"
#include "TApiEzsift.h"
#include "TApiInput.h"
#include "TApiOpencv.h"
#include "TApiBluetooth.h"
#endif


#if !defined(JSAPI_V8)
JSContextGroupRef	JSContextGroupCreate(const std::string& RuntimeDirectory)
{
	return JSContextGroupCreate();
}
#endif

namespace JsCore
{
	std::map<JSGlobalContextRef,TContext*> ContextCache;
	
	void		AddContextCache(TContext& Context,JSGlobalContextRef Ref);
	void		RemoveContextCache(TContext& Context);
	
	prmem::Heap	gGlobalObjectHeap(true, true, "JsCore::GlobalObjectHeap",0,true);
}

prmem::Heap& JsCore::GetGlobalObjectHeap()
{
	return gGlobalObjectHeap;
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


JsCore::TFunction::TFunction(JSContextRef Context,JSValueRef Value)// :
	//mContext	( Context )
{
	mThis = GetObject( Context, Value );
	
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
	//if ( Params.mThis == nullptr )
	//	Params.mThis = JSContextGetGlobalObject( mContext );
	auto This = Params.mThis ? GetObject( Context, Params.mThis ) : nullptr;
	
	//	call
	JSValueRef Exception = nullptr;
	auto Result = JSObjectCallAsFunction( Context, FunctionHandle, This, Params.mArguments.GetSize(), Params.mArguments.GetArray(), &Exception );

	ThrowException( Context, Exception );
	
	Params.mReturn = Result;
}

std::string	JsCore::GetString(JSContextRef Context,JSStringRef Handle)
{
	//	don't actually need a local context
	return GetString( Handle );
}


std::string	JsCore::GetString(JSStringRef Handle)
{
	Array<char> Buffer;
	//	gr: length doesn't include terminator, but JSStringGetUTF8CString writes one
	Buffer.SetSize(JSStringGetLength(Handle));
	Buffer.PushBack('\0');

	size_t bytesWritten = JSStringGetUTF8CString(Handle, Buffer.GetArray(), Buffer.GetSize() );
	Buffer.SetSize(bytesWritten);
	if ( Buffer.IsEmpty() )
		return std::string();

	//	the last byte is a null \0 which std::string doesn't need.
	std::string utf_string = std::string( Buffer.GetArray(), bytesWritten -1);
	return utf_string;
}

std::string	JsCore::GetString(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	JSValueRef Exception = nullptr;
	auto HandleType = JSValueGetType( Context, Handle );
	auto StringJs = JSValueToStringCopy( Context, Handle, &Exception );
	ThrowException( Context, Exception );
	auto Str = GetString( Context, StringJs );
	JSStringRelease( StringJs );
	return Str;
}



float JsCore::GetFloat(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	JSValueRef Exception = nullptr;
	auto DoubleJs = JSValueToNumber( Context, Handle, &Exception );
	auto Float = static_cast<float>( DoubleJs );
	return Float;
}

bool JsCore::GetBool(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	auto Bool = JSValueToBoolean( Context, Handle );
	return Bool;
}


JSStringRef JsCore::GetString(JSContextRef Context,const std::string& String)
{
	//	JSCore doesn't need a context, but v8 does
	auto Handle = JSStringCreateWithUTF8CString( Context, String.c_str() );
	return Handle;
}

JSValueRef JsCore::GetValue(JSContextRef Context,const std::string& String)
{
	auto StringHandle = JSStringCreateWithUTF8CString( Context, String.c_str() );
	auto ValueHandle = JSValueMakeString( Context, StringHandle );
	JSStringRelease(StringHandle);
	return ValueHandle;
}

JSValueRef JsCore::GetValue(JSContextRef Context,JSObjectRef Value)
{
	return Value;
}

JSValueRef JsCore::GetValue(JSContextRef Context,bool Value)
{
	return JSValueMakeBoolean( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,size_t Value)
{
	//	javascript doesn't support 64bit (kinda), so throw if number goes over 32bit
	if ( Value > std::numeric_limits<uint32_t>::max() )
	{
		std::stringstream Error;
		Error << "Javascript doesn't support 64bit integers, so this value(" << Value <<") is out of range (max 32bit=" << std::numeric_limits<uint32_t>::max() << ")";
		throw Soy::AssertException( Error.str() );
	}

	auto Value32 = static_cast<uint32_t>( Value );
	return GetValue( Context, Value32 );
}

JSValueRef JsCore::GetValue(JSContextRef Context,uint32_t Value)
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

JSValueRef JsCore::GetValue(JSContextRef Context,uint8_t Value)
{
	return JSValueMakeNumber( Context, Value );
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TObject& Object)
{
	return Object.mThis;
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TFunction& Object)
{
	return Object.mThis;
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TArray& Object)
{
	return Object.mThis;
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
	return Object.mThis;
#endif
	
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TPromise& Object)
{
	return GetValue( Context, Object.mPromise );
}

//	gr: windows needs this as Bind::TInstance
Bind::TInstance::TInstance(const std::string& RootDirectory,const std::string& ScriptFilename,std::function<void(int32_t)> OnShutdown) :
	mContextGroupThread	( std::string("JSCore thread ") + ScriptFilename ),
	mRootDirectory		( RootDirectory ),
	mOnShutdown			( OnShutdown )
{
	auto CreateVirtualMachine = [this,ScriptFilename,RootDirectory]()
	{
		#if !defined(TARGET_WINDOWS)
		{
			auto ThisRunloop = CFRunLoopGetCurrent();
			auto MainRunloop = CFRunLoopGetMain();

			if ( ThisRunloop == MainRunloop )
				throw Soy::AssertException("Need to create JS VM on a different thread to main");
		}
		#endif

		//	for v8
		std::string RuntimePath = Platform::GetAppResourcesDirectory() + "/v8Runtime/";
		
		this->mContextGroup = std::move(JSContextGroupCreate( RuntimePath ));
		if ( !mContextGroup )
			throw Soy::AssertException("JSContextGroupCreate failed");
	
		
		//	bind first
		try
		{
			//	create a context
			auto Context = CreateContext(RootDirectory);
			
			ApiPop::Bind( *Context );
			ApiOpengl::Bind( *Context );
			ApiMedia::Bind( *Context );
			ApiWebsocket::Bind( *Context );
			ApiHttp::Bind( *Context );
			ApiSocket::Bind( *Context );
			ApiSerial::Bind( *Context );

		#if !defined(PLATFORM_WINDOWS)
			ApiVarjo::Bind( *Context );
			//ApiOpencl::Bind( *Context );
			ApiDlib::Bind( *Context );
			ApiCoreMl::Bind( *Context );
			ApiEzsift::Bind( *Context );
			ApiInput::Bind( *Context );
			ApiOpencv::Bind( *Context );
			ApiBluetooth::Bind( *Context );
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

#if defined(PLATFORM_WINDOWS)
	JsCore::LoadDll();
	CreateVirtualMachine();
#else
	//	gr: these exceptions are getting swallowed!
	mContextGroupThread.PushJob( CreateVirtualMachine );
	mContextGroupThread.Start();
#endif
}

JsCore::TInstance::~TInstance()
{
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

bool JsCore::TInstance::OnJobQueueIteration(std::function<void (std::chrono::milliseconds)> &Sleep)
{
	if ( !mContextGroup )
		return true;
	
#if defined(JSAPI_V8)
	auto& vm = mContextGroup.GetVirtualMachine();
	return vm.ProcessJobQueue( Sleep );
#else
	return true;
#endif
}

std::shared_ptr<JsCore::TContext> JsCore::TInstance::CreateContext(const std::string& Name)
{
	JSClassRef Global = nullptr;
	
	auto Context = JSGlobalContextCreateInGroup( mContextGroup, Global );
#if !defined(JSAPI_V8)
	JSGlobalContextSetName( Context, JsCore::GetString( Context, Name ) );
#endif

	std::shared_ptr<JsCore::TContext> pContext( new TContext( *this, Context, mRootDirectory ) );
	mContexts.PushBack( pContext );

	//	set pointer
	auto SetContext = [&](Bind::TLocalContext& LocalContext)
	{
		LocalContext.mLocalContext.SetContext( *pContext );
	};
	pContext->Execute( SetContext );
	
	return pContext;
}


void JsCore::TInstance::DestroyContext(JsCore::TContext& Context)
{
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
	//	gr: does this need to defer?
	//	do callback
	if ( mOnShutdown )
		mOnShutdown(ExitCode);
}

void JsCore::ThrowException(JSContextRef Context,JSValueRef ExceptionHandle,const std::string& ThrowContext)
{
	auto ExceptionType = JSValueGetType( Context, ExceptionHandle );
	//	not an exception
	if ( ExceptionType == kJSTypeUndefined || ExceptionType == kJSTypeNull )
		return;

	auto GetString_NoThrow = [](JSContextRef Context,JSValueRef Handle)
	{
		JSValueRef Exception = nullptr;
		auto HandleString = JSValueToStringCopy( Context, Handle, &Exception );
		if ( Exception )
		{
			auto HandleType = JSValueGetType( Context, Handle );
			std::stringstream Error;
			Error << "Exception->String threw exception. Exception is type " << HandleType;
			return Error.str();
		}
		auto Str = JsCore::GetString( Context, HandleString );
		JSStringRelease(HandleString);
		return Str;
	};
	
	std::stringstream Error;
	auto ExceptionString = GetString_NoThrow( Context, ExceptionHandle );
	Error << "Exception in " << ThrowContext << ": " << ExceptionString;
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
	mJobQueue.Stop();

	//	tell instance to clean up
	//mInstance.DestroyContext(*this);
	mInstance.Shutdown(ExitCode);
}

void JsCore::TContext::GarbageCollect(JSContextRef LocalContext)
{
	//	seems like this would be a good idea...
	std::lock_guard<std::recursive_mutex> Lock(mExecuteLock);
	
	JSGarbageCollect( LocalContext );
}


void JsCore::TContext::LoadScript(const std::string& Source,const std::string& Filename)
{
	auto Exec = [=](Bind::TLocalContext& Context)
	{
		auto ThisHandle = JSObjectRef(nullptr);
		auto SourceJs = JSStringCreateWithUTF8CString( Context.mLocalContext, Source.c_str() );
		auto FilenameJs = JSStringCreateWithUTF8CString( Context.mLocalContext, Filename.c_str() );
		auto LineNumber = 0;
		JSValueRef Exception = nullptr;
		auto ResultHandle = JSEvaluateScript( Context.mLocalContext, SourceJs, ThisHandle, FilenameJs, LineNumber, &Exception );
		ThrowException( Context.mLocalContext, Exception, Filename );
	};
	//	this exec meant the load was taking so long, JS funcs were happening on the queue thread
	//	and that seemed to cause some crashes
	//	gr: on windows, this... has some problem where the main thread seems to get stuck? maybe to do with creating windows on non-main threads?
	//		GetMessage blocks and we never get wm_paints, even though JS vm is running in the background
#if defined(TARGET_WINDOWS)
	Execute( Exec );
#else
	Queue(Exec);
#endif
}

template<typename CLOCKTYPE=std::chrono::high_resolution_clock>
class TJob_DefferedUntil : public PopWorker::TJob_Function
{
public:
	TJob_DefferedUntil(std::function<void()> Functor,typename CLOCKTYPE::time_point FutureTime) :
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
	
	
	auto FunctorWrapper = [=]()
	{
		//	need to catch this?
		Execute( Functor );
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


#if !defined(JSAPI_V8)
void JSLockAndRun(JSGlobalContextRef GlobalContext,std::function<void(JSContextRef&)> Functor)
{
	//	gr: this may be the source of problems, this should be a properly locally scoped context...
	std::function<void(JsCore::TLocalContext&)> Functor
	JSContextRef ContextRef = mContext;
	TLocalContext LocalContext( ContextRef, *this );
	Functor( LocalContext );
}
#endif

void JsCore::TContext::Execute(std::function<void(JsCore::TLocalContext&)> Functor)
{
	//	gr: lock so only one JS operation happens at a time
	//		doing this to test stability (this also emulates v8 a lot more)
	std::lock_guard<std::recursive_mutex> Lock(mExecuteLock);
	
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
	auto PropertyName = JsCore::GetString( mContext, LeafName );
	auto Property = JSObjectGetProperty( mContext, This.mThis, PropertyName, &Exception );
	JSStringRelease(PropertyName);
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
	JsCore::TFunction Func( mContext, Object.mThis );
	return Func;
}


void JsCore::TObject::SetObject(const std::string& Name,const TObject& Object)
{
	SetMember( Name, Object.mThis );
}

void JsCore::TObject::SetFunction(const std::string& Name,JsCore::TFunction& Function)
{
	SetMember( Name, Function.mThis );
}

void JsCore::TObject::SetMember(const std::string& Name,JSValueRef Value)
{
	auto NameJs = JsCore::GetString( mContext, Name );
	JSPropertyAttributes Attribs = kJSPropertyAttributeNone;
	JSValueRef Exception = nullptr;
	JSObjectSetProperty( mContext, mThis, NameJs, Value, Attribs, &Exception );
	JSStringRelease( NameJs );
	ThrowException( mContext, Exception );
}

void JsCore::TObject::SetArray(const std::string& Name,JsCore::TArray& Array)
{
	SetMember( Name, Array.mThis );
}

void JsCore::TObject::SetInt(const std::string& Name,uint32_t Value)
{
	SetMember( Name, GetValue( mContext, Value ) );
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

	//	construct
	TCallback ConstructorParams(LocalContext);
	ConstructorParams.mThis = NewObject.mThis;
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
	JSObjectSetPrivate( NewObject, Data );
	Bind::TObject ObjectHandle( LocalContext.mLocalContext, NewObject );
	ObjectPointer.SetHandle( LocalContext, ObjectHandle );
	
	//	for V8, to get a free() callback, we need a persistent to be marked weak
#if defined(JSAPI_V8)
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
	ConstructorParams.mThis = NewObject.mThis;
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


JSValueRef JsCore::TContext::CallFunc(TLocalContext& LocalContext,std::function<void(JsCore::TCallback&)> Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception,const std::string& FunctionContext)
{
	//	call our function from
	try
	{
		//	if context has gone, we might be shutting down
		if ( !mContext )
			throw Soy::AssertException("CallFunc: Context is null, maybe shutting down");

		TCallback Callback(LocalContext);
		Callback.mThis = This;
	
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

std::string JsCore::TCallback::GetArgumentString(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
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
	auto Handle = GetArgumentValue( Index );
	JsCore::TFunction Function( GetContextRef(), Handle );
	return Function;
}

JsCore::TArray JsCore::TCallback::GetArgumentArray(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto HandleObject = JsCore::GetObject( GetContextRef(), Handle );
	JsCore::TArray Array( GetContextRef(), HandleObject );
	return Array;
}

bool JsCore::TCallback::GetArgumentBool(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto Value = JsCore::GetBool( GetContextRef(), Handle );
	return Value;
}

float JsCore::TCallback::GetArgumentFloat(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto Value = JsCore::GetFloat( GetContextRef(), Handle );
	return Value;
}

JsCore::TObject JsCore::TCallback::GetArgumentObject(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto HandleObject = JsCore::GetObject( GetContextRef(), Handle );
	return JsCore::TObject( GetContextRef(), HandleObject );
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
	mThis = This.mThis;
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

void JsCore::TCallback::SetArgumentArray(size_t Index,ArrayBridge<float>&& Values)
{
	JSCore_SetArgument( mArguments, mLocalContext, Index, Values );
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
	return TObject( Context.mLocalContext, mObject.mThis );
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
	JSValueProtect( Context, ObjectOrFunc );
}


void JsCore::TPersistent::Release(JSGlobalContextRef Context,JSObjectRef ObjectOrFunc,const std::string& DebugName)
{
	//std::Debug << "Release context=" << Context << " object=" << ObjectOrFunc << " " << DebugName << std::endl;
	JSValueUnprotect( Context, ObjectOrFunc );
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
		Release( mRetainedContext, mObject.mThis, mDebugName );
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
	mObject = Object;
	Retain( mRetainedContext, mObject.mThis, mDebugName );
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
	
	
	mDebugName = That.mDebugName + " (copy)";
	mContext = That.mContext;
	mRetainedContext = That.mRetainedContext;
#if defined(JSAPI_V8)
	mObject = That.mObject;
#else
	Retain( mRetainedContext, That.mObject, mDebugName );
#endif
	
	mContext->OnPersitentRetained(*this);
}


template<typename TYPE>
JSObjectRef JsCore_GetTypedArray(JSContextRef Context,const ArrayBridge<TYPE>& Values,JSTypedArrayType TypedArrayType)
{
	//	JSObjectMakeTypedArrayWithBytesNoCopy makes an externally backed array, which has a destruction callback
	static JSTypedArrayBytesDeallocator Dealloc = [](void* pArrayBuffer,void* DeallocContext)
	{
		auto* ArrayBuffer = static_cast<TYPE*>( pArrayBuffer );
		delete[] ArrayBuffer;
	};
	
	//	allocate an array
	//	gr: want to do it on a heap, but our heap needs a size, + context + array and we can only pass 1 contextually variable
	auto* AllocatedBuffer = new TYPE[Values.GetSize()];
	auto AllocatedBufferSize = sizeof(TYPE) * Values.GetSize();
	if ( AllocatedBufferSize != Values.GetDataSize() )
		throw Soy::AssertException("Array size mismatch");

	//	safely copy from values
	size_t AllocatedArrayCount = 0;
	auto AllocatedArray = GetRemoteArray( AllocatedBuffer, AllocatedBufferSize, AllocatedArrayCount );
	AllocatedArray.Copy( Values );

	//	make externally backed array that'll dealloc
	void* DeallocContext = nullptr;
	JSValueRef Exception = nullptr;
	auto ArrayObject = JSObjectMakeTypedArrayWithBytesNoCopy( Context, TypedArrayType, AllocatedBuffer, AllocatedBufferSize, Dealloc, DeallocContext, &Exception );
	JsCore::ThrowException( Context, Exception );
	
	return ArrayObject;
}

JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<uint8_t>& Values)
{
	return JsCore_GetTypedArray( Context, Values, kJSTypedArrayTypeUint8Array );
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
	static auto WarningArraySize = 300;
	if ( Size > WarningArraySize )
	{
		//std::stringstream Error;
		auto& Error = std::Debug;
		Error << "Warning: Javascript core seems to have problems (crashing/corruption) with large arrays; " << Size << "/" << WarningArraySize << std::endl;
		//throw Soy::AssertException( Error.str() );
		Size = WarningArraySize;
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
	JSObjectSetPropertyAtIndex( mContext, mThis, Index, Object.mThis, &Exception );
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


void JsCore::TTemplate::RegisterClassWithContext(TLocalContext& Context,const std::string& ParentObjectName,const std::string& OverrideLeafName)
{
	//	add a terminator function
	JSStaticFunction NewFunction = { nullptr, nullptr, kJSPropertyAttributeNone };
	mFunctions.PushBack(NewFunction);
	mDefinition.staticFunctions = mFunctions.GetArray();
	mClass = JSClassCreate( Context.mLocalContext, &mDefinition );
	JSClassRetain( mClass );
	
	//	gr: this works, but logic seems a little odd to me
	//		you create an object, representing the class, and set it on the object like
	//		Parent.YourClass = function()
	//	but JsObjectMake also creates objects...
	
	//	property of Parent (eg. global or Pop.x) can be overridden in case we want a nicer class name than the unique one
	auto PropertyName = GetString( Context.mLocalContext, mDefinition.className );
	if ( OverrideLeafName.length() )
		PropertyName = GetString( Context.mLocalContext, OverrideLeafName );
	
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
	JSObjectSetProperty( Context.mLocalContext, ParentObject.mThis, PropertyName, ClassObject, kJSPropertyAttributeNone, &Exception );
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
	mHandle = mObject;
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
