#include "JsCoreInstance.h"
#include "SoyAssert.h"
#include "SoyFilesystem.h"
#include "TApiCommon.h"
#include "TApiOpengl.h"
//#include "TApiOpencl.h"
#include "TApiDlib.h"
#include "TApiMedia.h"
#include "TApiWebsocket.h"
#include "TApiSocket.h"
#include "TApiHttp.h"
#include "TApiCoreMl.h"
#include "TApiEzsift.h"
#include "TApiInput.h"
#include "TApiOpencv.h"


namespace JsCore
{
	std::map<JSGlobalContextRef,TContext*> ContextCache;
	
	void		AddContextCache(TContext& Context,JSGlobalContextRef Ref);
	void		RemoveContextCache(TContext& Context);
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
	return const_cast<JSObjectRef>( Value );
}


JsCore::TFunction::TFunction(JSContextRef Context,JSValueRef Value) :
	mContext	( Context )
{
	mThis = GetObject( Context, Value );
	
	if ( !JSObjectIsFunction(Context, mThis) )
		throw Soy::AssertException("Object should be function");
}


void JsCore::TFunction::Call(Bind::TObject& This) const
{
	Call( This.mThis, nullptr );
}

void JsCore::TFunction::Call(Bind::TCallback& Params) const
{
	if ( Params.mThis == nullptr )
		Params.mThis = JSContextGetGlobalObject( mContext );
	
	auto FunctionHandle = mThis;
	auto This = GetObject( mContext, Params.mThis );

	JSValueRef Exception = nullptr;
	auto Result = JSObjectCallAsFunction( mContext, FunctionHandle, This, Params.mArguments.GetSize(), Params.mArguments.GetArray(), &Exception );

	ThrowException( mContext, Exception );
	Params.mReturn = Result;
}

JSValueRef JsCore::TFunction::Call(JSObjectRef This,JSValueRef Arg0) const
{
	auto& Context = JsCore::GetContext( mContext );
	Bind::TCallback Params( Context );
	Params.mThis = This;
	
	if ( Arg0 != nullptr )
		Params.mArguments.PushBack( Arg0 );
	
	Call( Params );
	
	return Params.mReturn;
}


std::string	JsCore::GetString(JSContextRef Context,JSStringRef Handle)
{
	size_t maxBufferSize = JSStringGetMaximumUTF8CStringSize(Handle);
	char utf8Buffer[maxBufferSize];
	size_t bytesWritten = JSStringGetUTF8CString(Handle, utf8Buffer, maxBufferSize);
	//	the last byte is a null \0 which std::string doesn't need.
	std::string utf_string = std::string(utf8Buffer, bytesWritten -1);
	return utf_string;
}

std::string	JsCore::GetString(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	JSValueRef Exception = nullptr;
	auto HandleType = JSValueGetType( Context, Handle );
	auto StringJs = JSValueToStringCopy( Context, Handle, &Exception );
	ThrowException( Context, Exception );
	return GetString( Context, StringJs );
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
	auto Handle = JSStringCreateWithUTF8CString( String.c_str() );
	return Handle;
}

JSValueRef JsCore::GetValue(JSContextRef Context,const std::string& String)
{
	auto StringHandle = JSStringCreateWithUTF8CString( String.c_str() );
	auto ValueHandle = JSValueMakeString( Context, StringHandle );
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

JSValueRef JsCore::GetValue(JSContextRef Context,const TArray& Object)
{
	return Object.mThis;
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TPersistent& Object)
{
	if ( Object.mFunction.mThis )
		return Object.mFunction.mThis;

	if ( Object.mObject.mThis )
		return Object.mObject.mThis;
	
	throw Soy::AssertException("return null, or undefined here?");
	return nullptr;
}

JSValueRef JsCore::GetValue(JSContextRef Context,const TPromise& Object)
{
	return GetValue( Context, Object.mPromise );
}


JsCore::TInstance::TInstance(const std::string& RootDirectory,const std::string& ScriptFilename) :
	mContextGroup	( JSContextGroupCreate() ),
	mRootDirectory	( RootDirectory )
{
	if ( !mContextGroup )
		throw Soy::AssertException("JSContextGroupCreate failed");
	
	
	//	bind first
	try
	{
		//	create a context
		mContext = CreateContext();
		
		ApiPop::Bind( *mContext );
		ApiOpengl::Bind( *mContext );
		//ApiOpencl::Bind( *mContext );
		ApiDlib::Bind( *mContext );
		ApiMedia::Bind( *mContext );
		ApiWebsocket::Bind( *mContext );
		ApiHttp::Bind( *mContext );
		ApiSocket::Bind( *mContext );
		ApiCoreMl::Bind( *mContext );
		ApiEzsift::Bind( *mContext );
		ApiInput::Bind( *mContext );
		ApiOpencv::Bind( *mContext );
		
		std::string BootupSource;
		Soy::FileToString( mRootDirectory + ScriptFilename, BootupSource );
		mContext->LoadScript( BootupSource, ScriptFilename );
	}
	catch(std::exception& e)
	{
		//	clean up
		mContext.reset();
		throw;
	}
}

JsCore::TInstance::~TInstance()
{
	JSContextGroupRelease(mContextGroup);
}

std::shared_ptr<JsCore::TContext> JsCore::TInstance::CreateContext()
{
	JSClassRef Global = nullptr;
	
	auto Context = JSGlobalContextCreateInGroup( mContextGroup, Global );
	std::shared_ptr<JsCore::TContext> pContext( new TContext( *this, Context, mRootDirectory ) );
	//mContexts.PushBack( pContext );
	return pContext;
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
		
		size_t maxBufferSize = JSStringGetMaximumUTF8CStringSize( HandleString );
		char utf8Buffer[maxBufferSize];
		size_t bytesWritten = JSStringGetUTF8CString( HandleString, utf8Buffer, maxBufferSize);
		//	the last byte is a null \0 which std::string doesn't need.
		std::string utf_string = std::string(utf8Buffer, bytesWritten -1);
		return utf_string;
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
	mJobQueue		( *this )
{
	AddContextCache( *this, mContext );
	mJobQueue.Start();
}

JsCore::TContext::~TContext()
{
	JSGlobalContextRelease( mContext );
	RemoveContextCache( *this );
}

void JsCore::TContext::LoadScript(const std::string& Source,const std::string& Filename)
{
	auto ThisHandle = JSObjectRef(nullptr);
	auto SourceJs = JSStringCreateWithUTF8CString(Source.c_str());
	auto FilenameJs = JSStringCreateWithUTF8CString(Filename.c_str());
	auto LineNumber = 0;
	JSValueRef Exception = nullptr;
	auto ResultHandle = JSEvaluateScript( mContext, SourceJs, ThisHandle, FilenameJs, LineNumber, &Exception );
	ThrowException(Exception);
}


void JsCore::TContext::QueueDelay(std::function<void(JsCore::TContext&)> Functor,size_t DelayMs)
{
	//	make a promise or thread job that skips if time hasn't elapsed?
	Queue( Functor );
}

void JsCore::TContext::Queue(std::function<void(JsCore::TContext&)> Functor)
{
	auto FunctorWrapper = [=]()
	{
		//	need to catch this?
		Execute( Functor );
	};
	mJobQueue.PushJob( FunctorWrapper );
	//	todo: make a job queue to queue up jobs so that the caller thread
	//			doesnt block
	//	Javascript core is threadsafe, but we don't want to block our own threads
	//	and caller code is expecting to lose ownership of the functor anyway
}

void JsCore::TContext::Execute(std::function<void(JsCore::TContext&)> Functor)
{
	//	javascript core is threadsafe, so we can just call
	//	but maybe we need to set a javascript exception, if this is
	//	being called from js to relay stuff back
	Functor( *this );
}


template<typename TYPE>
JsCore::TArray JsCore_CreateArray(JsCore::TContext& Context,size_t ElementCount,std::function<TYPE(size_t)> GetElement)
{
	auto& mContext = Context.mContext;
	
	JSValueRef Values[ElementCount];
	for ( auto i=0;	i<ElementCount;	i++ )
	{
		auto Element = GetElement(i);
		Values[i] = JsCore::GetValue( mContext, Element );
	}
	auto ValuesArray = GetRemoteArray( Values, ElementCount );
	auto ArrayObject = JsCore::GetArray( mContext, GetArrayBridge(ValuesArray) );
	JsCore::TArray Array( mContext, ArrayObject );
	return Array;
}


Bind::TArray JsCore::TContext::CreateArray(size_t ElementCount,std::function<std::string(size_t)> GetElement)
{
	return JsCore_CreateArray( *this, ElementCount, GetElement );
}

Bind::TArray JsCore::TContext::CreateArray(size_t ElementCount,std::function<JsCore::TObject(size_t)> GetElement)
{
	return JsCore_CreateArray( *this, ElementCount, GetElement );
}

Bind::TArray JsCore::TContext::CreateArray(size_t ElementCount,std::function<JsCore::TArray(size_t)> GetElement)
{
	return JsCore_CreateArray( *this, ElementCount, GetElement );
}

Bind::TArray JsCore::TContext::CreateArray(size_t ElementCount,std::function<int32_t(size_t)> GetElement)
{
	return JsCore_CreateArray( *this, ElementCount, GetElement );
}

Bind::TArray JsCore::TContext::CreateArray(ArrayBridge<uint8_t>&& Values)
{
	auto ArrayObject = JsCore::GetArray( mContext, Values );
	JsCore::TArray Array( mContext, ArrayObject );
	return Array;
}

Bind::TArray JsCore::TContext::CreateArray(ArrayBridge<float>&& Values)
{
	auto ArrayObject = JsCore::GetArray( mContext, Values );
	JsCore::TArray Array( mContext, ArrayObject );
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
	JSValueRef Exception = nullptr;
	auto StringHandle = JSValueToStringCopy( mContext, Value, &Exception );
	JsCore::ThrowException( mContext, Exception, MemberName );
	auto String = JsCore::GetString( mContext, StringHandle );
	return String;
}

uint32_t JsCore::TObject::GetInt(const std::string& MemberName)
{
	auto Value = GetMember( MemberName );
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
	JSValueRef Exception = nullptr;
	auto Number = JSValueToNumber( mContext, Value, &Exception );
	JsCore::ThrowException( mContext, Exception, MemberName );
	
	//	convert this double to an int!
	auto Valuef = static_cast<float>(Number);
	return Valuef;
}

Bind::TFunction JsCore::TObject::GetFunction(const std::string& MemberName)
{
	auto Object = GetObject(MemberName);
	Bind::TFunction Func( mContext, Object.mThis );
	return Func;
}

void* JsCore::TObject::GetThis()
{
	return JSObjectGetPrivate( mThis );
}


void JsCore::TObject::SetObject(const std::string& Name,const TObject& Object)
{
	SetMember( Name, Object.mThis );
}

void JsCore::TObject::SetFunction(const std::string& Name,Bind::TFunction& Function)
{
	SetMember( Name, Function.mThis );
}

void JsCore::TObject::SetMember(const std::string& Name,JSValueRef Value)
{
	auto NameJs = JsCore::GetString( mContext, Name );
	JSPropertyAttributes Attribs;
	JSValueRef Exception = nullptr;
	JSObjectSetProperty( mContext, mThis, NameJs, Value, Attribs, &Exception );
	ThrowException( mContext, Exception );
}

void JsCore::TObject::SetArray(const std::string& Name,ArrayBridge<bool>&& Values)
{
	auto Array = JsCore::GetArray( mContext, Values );
	SetMember( Name, Array );
}

void JsCore::TObject::SetArray(const std::string& Name,ArrayBridge<Bind::TObject>&& Values)
{
	auto Array = JsCore::GetArray( mContext, Values );
	SetMember( Name, Array );
}

void JsCore::TObject::SetArray(const std::string& Name,Bind::TArray& Array)
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


JsCore::TObject JsCore::TContext::CreateObjectInstance(const std::string& ObjectTypeName)
{
	BufferArray<JSValueRef,1> FakeArgs;
	return CreateObjectInstance( ObjectTypeName, GetArrayBridge(FakeArgs) );
}

Bind::TObject JsCore::TContext::CreateObjectInstance(const std::string& ObjectTypeName,ArrayBridge<JSValueRef>&& ConstructorArguments)
{
	//	create basic object
	if ( ObjectTypeName.length() == 0 || ObjectTypeName == "Object" )
	{
		JSClassRef Default = nullptr;
		void* Data = nullptr;
		auto NewObject = JSObjectMake( mContext, Default, Data );
		return TObject( mContext, NewObject );
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
	auto& ObjectPointer = ObjectTemplate.AllocInstance();
	void* Data = &ObjectPointer;
	auto NewObjectHandle = JSObjectMake( mContext, Class, Data );
	TObject NewObject( mContext, NewObjectHandle );
	ObjectPointer.SetHandle( NewObject );

	//	construct
	TCallback ConstructorParams(*this);
	ConstructorParams.mThis = NewObject.mThis;
	ConstructorParams.mArguments.Copy( ConstructorArguments );
	
	//	actually call!
	ObjectPointer.Construct( ConstructorParams );
	
	return NewObject;
}


void JsCore::TContext::BindRawFunction(const std::string& FunctionName,const std::string& ParentObjectName,JSObjectCallAsFunctionCallback FunctionPtr)
{
	auto This = GetGlobalObject( ParentObjectName );

	auto FunctionNameJs = JsCore::GetString( mContext, FunctionName );
	JSValueRef Exception = nullptr;
	auto FunctionHandle = JSObjectMakeFunctionWithCallback( mContext, FunctionNameJs, FunctionPtr );
	ThrowException(Exception);
	TFunction Function( mContext, FunctionHandle );
	This.SetFunction( FunctionName, Function );
}


JsCore::TPromise JsCore::TContext::CreatePromise()
{
	if ( !mMakePromiseFunction.mThis )
	{
		auto* MakePromiseFunctionSource =  R"V0G0N(
		
		let MakePromise = function()
		{
			var PromData = {};
			var prom = new Promise( function(Resolve,Reject) { PromData.Resolve = Resolve; PromData.Reject = Reject; } );
			PromData.Promise = prom;
			prom.Resolve = PromData.Resolve;
			prom.Reject = PromData.Reject;
			return prom;
		}
		MakePromise;
		//MakePromise();
		)V0G0N";
		
		JSStringRef FunctionSourceString = JsCore::GetString( mContext, MakePromiseFunctionSource );
		JSValueRef Exception = nullptr;
		auto FunctionValue = JSEvaluateScript( mContext, FunctionSourceString, nullptr, nullptr, 0, &Exception );
		ThrowException( Exception );
		
		mMakePromiseFunction = TFunction( mContext, FunctionValue );
	}
	
	auto NewPromiseValue = mMakePromiseFunction.Call();
	auto NewPromiseHandle = JsCore::GetObject( mContext, NewPromiseValue );
	TObject NewPromiseObject( mContext, NewPromiseHandle );
	auto Resolve = NewPromiseObject.GetFunction("Resolve");
	auto Reject = NewPromiseObject.GetFunction("Reject");

	TPromise Promise( NewPromiseObject, Resolve, Reject );
/*
	TObject NewPromiseObject( mContext, NewPromiseHandle );
	
	auto NewPromiseObject = const_cast<JSObjectRef>(NewPromiseHandle);
	JSValueRef Exception = nullptr;
	auto Resolve = const_cast<JSObjectRef>(JSObjectGetProperty( Context, NewPromiseObject, JSStringCreateWithUTF8CString("Resolve"), &Exception ) );
	auto Reject = const_cast<JSObjectRef>(JSObjectGetProperty( Context, NewPromiseObject, JSStringCreateWithUTF8CString("Reject"), &Exception ) );
	
	JsCore::TPromise Promise( Context, NewPromiseObject, Resolve, Reject );
	*/
	return Promise;
}


JSValueRef JsCore::TContext::CallFunc(std::function<void(Bind::TCallback&)> Function,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception,const std::string& FunctionContext)
{
	//	call our function from
	try
	{
		TCallback Callback(*this);
		Callback.mThis = This;
	
		if ( Callback.mThis == nullptr )
			Callback.mThis = JSValueMakeUndefined( mContext );

		for ( auto a=0;	a<ArgumentCount;	a++ )
			Callback.mArguments.PushBack( Arguments[a] );

		//	actually call!
		Function( Callback );
		
		if ( Callback.mReturn == nullptr )
			Callback.mReturn = JSValueMakeUndefined( mContext );
		
		return Callback.mReturn;
	}
	catch (std::exception& e)
	{
		std::stringstream Error;
		Error << FunctionContext << " exception: " << e.what();
		Exception = GetValue( mContext, Error.str() );
		return JSValueMakeUndefined( mContext );
	}
}


/*
JSValueRef ObjectCallAsFunctionCallback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
	cout << "Hello World" << endl;
	return JSValueMakeUndefined(ctx);
}


JsCore::TInstance::
{
	JSObjectRef globalObject = JSContextGetGlobalObject(globalContext);
	
	JSStringRef logFunctionName = JSStringCreateWithUTF8CString("log");
	JSObjectRef functionObject = JSObjectMakeFunctionWithCallback(globalContext, logFunctionName, &ObjectCallAsFunctionCallback);
	
	JSObjectSetProperty(globalContext, globalObject, logFunctionName, functionObject, kJSPropertyAttributeNone, nullptr);
	
	JSStringRef logCallStatement = JSStringCreateWithUTF8CString("log()");
	
	JSEvaluateScript(globalContext, logCallStatement, nullptr, nullptr, 1,nullptr);
	
 
	JSGlobalContextRelease(globalContext);
	JSStringRelease(logFunctionName);
	JSStringRelease(logCallStatement);
	}

*/

JSType JsCore::TCallback::GetArgumentType(size_t Index)
{
	if ( Index >= mArguments.GetSize() )
		return kJSTypeUndefined;
		
	auto Type = JSValueGetType( mContext.mContext, mArguments[Index] );
	return Type;
}

JSValueRef JsCore::TCallback::GetArgumentValue(size_t Index)
{
	if ( Index >= mArguments.GetSize() )
		return JSValueMakeUndefined( mContext.mContext );
	return mArguments[Index];
}

std::string JsCore::TCallback::GetArgumentString(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto String = JsCore::GetString( mContext.mContext, Handle );
	return String;
}

std::string JsCore::TCallback::GetArgumentFilename(size_t Index)
{
	auto Filename = GetArgumentString(Index);
	Filename = mContext.GetResolvedFilename( Filename );
	return Filename;
}

Bind::TFunction JsCore::TCallback::GetArgumentFunction(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	JsCore::TFunction Function( mContext.mContext, Handle );
	return Function;
}

Bind::TArray JsCore::TCallback::GetArgumentArray(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto HandleObject = JsCore::GetObject( mContext.mContext, Handle );
	Bind::TArray Array( mContext.mContext, HandleObject );
	return Array;
}

bool JsCore::TCallback::GetArgumentBool(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto Value = JsCore::GetBool( mContext.mContext, Handle );
	return Value;
}

float JsCore::TCallback::GetArgumentFloat(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto Value = JsCore::GetFloat( mContext.mContext, Handle );
	return Value;
}

Bind::TObject JsCore::TCallback::GetArgumentObject(size_t Index)
{
	auto Handle = GetArgumentValue( Index );
	auto HandleObject = JsCore::GetObject( mContext.mContext, Handle );
	return Bind::TObject( mContext.mContext, HandleObject );
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


void JsCore::TCallback::Return(Bind::TPersistent& Value)
{
	mReturn = GetValue( mContext.mContext, Value );
}

void JsCore::TCallback::Return(Bind::TPromise& Value)
{
	mReturn = GetValue( mContext.mContext, Value );
}

void JsCore::TCallback::ReturnNull()
{
	mReturn = JSValueMakeNull( mContext.mContext );
}

void JsCore::TCallback::ReturnUndefined()
{
	mReturn = JSValueMakeUndefined( mContext.mContext );
}


Bind::TObject JsCore::TCallback::ThisObject()
{
	auto Object = GetObject( mContext.mContext, mThis );
	return TObject( mContext.mContext, Object );
}


void JsCore::TCallback::SetThis(Bind::TObject& This)
{
	mThis = This.mThis;
}

template<typename TYPE>
void SetArgument(Array<JSValueRef>& mArguments,JsCore::TContext& Context,size_t Index,const TYPE& Value)
{
	while ( mArguments.GetSize() <= Index )
		mArguments.PushBack( JSValueMakeUndefined(Context.mContext) );
	
	mArguments[Index] = JsCore::GetValue( Context.mContext, Value );
}

void JsCore::TCallback::SetArgumentString(size_t Index,const std::string& Value)
{
	SetArgument( mArguments, mContext, Index, Value );
}

void JsCore::TCallback::SetArgumentInt(size_t Index,uint32_t Value)
{
	SetArgument( mArguments, mContext, Index, Value );
}

void JsCore::TCallback::SetArgumentObject(size_t Index,Bind::TObject& Value)
{
	SetArgument( mArguments, mContext, Index, Value );
}

void JsCore::TCallback::SetArgumentArray(size_t Index,ArrayBridge<std::string>&& Values)
{
	SetArgument( mArguments, mContext, Index, Values );
}

void JsCore::TCallback::SetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Values)
{
	SetArgument( mArguments, mContext, Index, Values );
}

void JsCore::TCallback::SetArgumentArray(size_t Index,ArrayBridge<float>&& Values)
{
	SetArgument( mArguments, mContext, Index, Values );
}

void JsCore::TCallback::SetArgumentArray(size_t Index,Bind::TArray& Value)
{
	SetArgument( mArguments, mContext, Index, Value );
}


JsCore::TObject JsCore::TContext::GetGlobalObject(const std::string& ObjectName)
{
	auto GlobalThis = JSContextGetGlobalObject( mContext );
	TObject Global( mContext, GlobalThis );
	
	if ( ObjectName.length() == 0 )
		return Global;
	auto Child = Global.GetObject( ObjectName );
	return Child;
}


void JsCore::TContext::CreateGlobalObjectInstance(const std::string& ObjectType,const std::string&  Name)
{
	auto NewObject = CreateObjectInstance( ObjectType );
	auto ParentName = Name;
	auto ObjectName = Soy::StringPopRight( ParentName, '.' );
	auto ParentObject = GetGlobalObject( ParentName );
	ParentObject.SetObject( ObjectName, NewObject );
}

JsCore::TPersistent JsCore::TContext::CreatePersistent(JsCore::TObject& Object)
{
	return JsCore::TPersistent( Object );
}

JsCore::TPersistent JsCore::TContext::CreatePersistent(JsCore::TFunction& Object)
{
	return JsCore::TPersistent( Object );
}

std::string JsCore::TContext::GetResolvedFilename(const std::string& Filename)
{
	//	gr: do this better!
	//	gr: should be able to use NSUrl to resolve ~/ or / etc
	if ( Filename[0] == '/' )
		return Filename;
	
	std::stringstream FullFilename;
	FullFilename << mRootDirectory << Filename;
	return FullFilename.str();
}


Bind::TPersistent::~TPersistent()
{
	if ( mObject.mThis != nullptr )
		JSValueUnprotect( mObject.mContext, mObject.mThis );
	 
	if ( mFunction.mThis != nullptr )
		JSValueUnprotect( mFunction.mContext, mFunction.mThis );

}

JSContextRef Bind::TPersistent::GetContext() const
{
	if ( mObject.mContext )
		return mObject.mContext;
	
	if ( mFunction.mContext )
		return mFunction.mContext;

	throw Soy::AssertException("Trying to get context from persistent with no object");
}

void Bind::TPersistent::Retain(const TObject& Object)
{
	mObject = Object;
	JSValueProtect( mObject.mContext, mObject.mThis );
}

void Bind::TPersistent::Retain(const TFunction& Function)
{
	mFunction = Function;
	JSValueProtect( mFunction.mContext, mFunction.mThis );
}

void Bind::TPersistent::Retain(const TPersistent& That)
{
	if ( That.mFunction.mThis != nullptr )
		Retain( That.mFunction );
	
	if ( That.mObject.mThis != nullptr )
		Retain( That.mObject );
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

JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<float>& Values)
{
	static_assert( sizeof(float) == 32/8, "Float is not 32 bit. Could support both here...");
	return JsCore_GetTypedArray( Context, Values, kJSTypedArrayTypeFloat32Array );
}

JSObjectRef JsCore::GetArray(JSContextRef Context,const ArrayBridge<JSValueRef>& Values)
{
	auto Size = Values.GetSize();
	static auto WarningArraySize = 800;
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


void JsCore::TArray::Set(size_t Index,Bind::TObject& Object)
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
	JsCore::ThrowException( Context, Exception );
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


void JsCore::TTemplate::RegisterClassWithContext(TContext& Context,const std::string& ParentObjectName)
{
	//	add a terminator function
	JSStaticFunction NewFunction = { nullptr, nullptr, kJSPropertyAttributeNone };
	mFunctions.PushBack(NewFunction);
	mDefinition.staticFunctions = mFunctions.GetArray();
	mClass = JSClassCreate( &mDefinition );
	JSClassRetain( mClass );
	
	//	gr: this works, but logic seems a little odd to me
	//		you create an object, representing the class, and set it on the object like
	//		Parent.YourClass = function()
	//	but JsObjectMake also creates objects...
	auto PropertyName = GetString( Context.mContext, mDefinition.className );
	auto ParentObject = Context.GetGlobalObject( ParentObjectName );
	JSObjectRef ClassObject = JSObjectMake( Context.mContext, mClass, nullptr );
	JSValueRef Exception = nullptr;
	JSObjectSetProperty( Context.mContext, ParentObject.mThis, PropertyName, ClassObject, kJSPropertyAttributeNone, &Exception );
	ThrowException( Context.mContext, Exception );
}

JsCore::TPromise::TPromise(TObject& Promise,TFunction& Resolve,TFunction& Reject) :
	mPromise	( Promise ),
	mResolve	( Resolve ),
	mReject		( Reject )
{
}

JsCore::TPromise::~TPromise()
{
	
}

void JsCore::TPromise::Resolve(JSValueRef Value) const
{
	//	gr: what is This supposed to be?
	JSObjectRef This = nullptr;
	auto Resolve = mResolve.GetFunction();
	Resolve.Call( This, Value );
}

void JsCore::TPromise::Reject(JSValueRef Value) const
{
	//	gr: what is This supposed to be?
	JSObjectRef This = nullptr;
	auto Reject = mReject.GetFunction();
	Reject.Call( This, Value );
}


JsCore::TObject JsCore::TObjectWrapperBase::GetHandle()
{
	//	just in case we've created C side, and not vis JS constructor
	//	make sure the handle has been set before access
	if ( !mHandle.IsObject() )
		throw Soy::AssertException("Accessing object handle before proper JS construction");
	
	return mHandle.GetObject();
}

void JsCore::TObjectWrapperBase::SetHandle(Bind::TObject& NewHandle)
{
	if ( mHandle.IsObject() )
		throw Soy::AssertException("Setting handle on object wrapper when it's non-null");
	
	mHandle = NewHandle;
}

