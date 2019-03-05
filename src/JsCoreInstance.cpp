#include "JsCoreInstance.h"
#include "SoyAssert.h"
#include "SoyFilesystem.h"
#include "TApiCommon.h"




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
	auto StringJs = JSValueToStringCopy( Context, Handle, &Exception );
	return GetString( Context, StringJs );
}


int32_t	JsCore::GetInt(JSContextRef Context,JSValueRef Handle)
{
	//	convert to string
	JSValueRef Exception = nullptr;
	auto DoubleJs = JSValueToNumber( Context, Handle, &Exception );

	auto Int = static_cast<int32_t>( DoubleJs );
	return Int;
}

JSStringRef JsCore::GetString(JSContextRef Context,const std::string& String)
{
	auto Handle = JSStringCreateWithUTF8CString( String.c_str() );
	return Handle;
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
		/*
		ApiOpengl::Bind( *mV8Container );
		ApiOpencl::Bind( *mV8Container );
		ApiDlib::Bind( *mV8Container );
		ApiMedia::Bind( *mV8Container );
		ApiWebsocket::Bind( *mV8Container );
		ApiHttp::Bind( *mV8Container );
		ApiSocket::Bind( *mV8Container );
		
		//	gr: start the thread immediately, there should be no problems having the thread running before queueing a job
		this->Start();
		*/
		std::string BootupSource;
		Soy::FileToString( mRootDirectory + ScriptFilename, BootupSource );
		/*
		auto* Container = mV8Container.get();
		auto LoadScript = [=](v8::Local<v8::Context> Context)
		{
			Container->LoadScript( Context, BootupSource, ScriptFilename );
		};
		
		mV8Container->QueueScoped( LoadScript );
		 */
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

	std::stringstream Error;
	auto ExceptionString = GetString( Context, ExceptionHandle );
	Error << "Exception in " << ThrowContext << ": " << ExceptionString;
	throw Soy::AssertException(Error.str());
}




JsCore::TContext::TContext(TInstance& Instance,JSGlobalContextRef Context,const std::string& RootDirectory) :
	mInstance		( Instance ),
	mContext		( Context ),
	mRootDirectory	( RootDirectory )
{
}

JsCore::TContext::~TContext()
{
	JSGlobalContextRelease( mContext );
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




JsCore::TObject::TObject(JSContextRef Context,JSObjectRef This) :
	mContext	( Context )
{
	if ( !mContext )
		throw Soy::AssertException("Null context for TObject");

	if ( This == nullptr )
		This = JSContextGetGlobalObject( mContext );
	if ( !mContext )
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
		if ( ChildName.length() == 0 )
			break;

		auto Child = This.GetObject(ChildName);
		This = Child;
	}

	JSValueRef Exception = nullptr;
	auto PropertyName = JsCore::GetString( mContext, MemberName );
	auto Property = JSObjectGetProperty( mContext, This.mThis, PropertyName, &Exception );
	ThrowException( mContext, Exception );
	return Property;	//	we return null/undefineds
}

JsCore::TObject JsCore::TObject::GetObject(const std::string& MemberName)
{
	auto Value = GetMember( MemberName );
	JSValueRef Exception = nullptr;
	auto Object = JSValueToObject( mContext, Value, &Exception );
	JsCore::ThrowException( mContext, Exception, MemberName );
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


void JsCore::TObject::SetObject(const std::string& Name,const TObject& Object)
{
	SetMember( Name, Object.mThis );
}

void JsCore::TObject::SetMember(const std::string& Name,JSValueRef Value)
{
	auto NameJs = JsCore::GetString( mContext, Name );
	JSPropertyAttributes Attribs;
	JSValueRef Exception = nullptr;
	JSObjectSetProperty( mContext, mThis, NameJs, Value, Attribs, &Exception );
	ThrowException( mContext, Exception );
}


JSObjectRef JsCore::TContext::GetGlobalObject(const std::string& Name)
{
	TObject This( mContext, nullptr );
	auto Member = This.GetObject( Name );
	return Member.mThis;
	
}


JsCore::TObject JsCore::TContext::CreateObjectInstance(const std::string& ObjectTypeName)
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
	
	//	instance new one
	auto& ObjectTemplate = *pObjectTemplate;
	auto& Class = ObjectTemplate.mClass;
	void* Data = nullptr;
	auto NewObject = JSObjectMake( mContext, Class, Data );
	return TObject( mContext, NewObject );
}

void JsCore::TContext::CreateGlobalObjectInstance(TString ObjectType,TString Name)
{
	auto NewObject = CreateObjectInstance( ObjectType );
	auto ParentName = Name;
	auto ObjectName = Soy::StringPopRight( ParentName, '.' );
	auto ParentObjectHandle = GetGlobalObject( ParentName );
	TObject ParentObject( mContext, ParentObjectHandle );
	ParentObject.SetObject( ObjectName, NewObject );
}


void JsCore::TContext::BindRawFunction(const char* FunctionName,JSObjectCallAsFunctionCallback Function)
{
	auto FunctionNameJs = JSStringCreateWithUTF8CString(FunctionName);
	JSObjectRef This = JSContextGetGlobalObject( mContext );
	auto Attributes = kJSPropertyAttributeNone;

	auto FunctionHandle = JSObjectMakeFunctionWithCallback( mContext, FunctionNameJs, Function );
	JSValueRef Exception = nullptr;
	JSObjectSetProperty( mContext, This, FunctionNameJs, FunctionHandle, Attributes, &Exception );
	ThrowException(Exception);
}

JSValueRef JsCore::TContext::CallFunc(std::function<JSValueRef(TCallbackInfo&)> Function,JSContextRef Context,JSObjectRef FunctionJs,JSObjectRef This,size_t ArgumentCount,const JSValueRef Arguments[],JSValueRef& Exception)
{
	try
	{
		TCallbackInfo CallbackInfo(mInstance);
		CallbackInfo.mContext = mContext;//Context;
		CallbackInfo.mThis = This;
		for ( auto a=0;	a<ArgumentCount;	a++ )
		{
			CallbackInfo.mArguments.PushBack( Arguments[a] );
		}
		auto Result = Function( CallbackInfo );
		return Result;
	}
	catch (std::exception& e)
	{
		auto ExceptionStr = JSStringCreateWithUTF8CString( e.what() );
		Exception = JSValueMakeString( Context, ExceptionStr );
		return JSValueMakeUndefined( Context );
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


std::string JsCore::TCallbackInfo::GetArgumentString(size_t Index) const
{
	auto Handle = mArguments[Index];
	auto String = JsCore::GetString( mContext, Handle );
	return String;
}


int32_t JsCore::TCallbackInfo::GetArgumentInt(size_t Index) const
{
	auto Handle = mArguments[Index];
	auto Value = JsCore::GetInt( mContext, Handle );
	return Value;
}
