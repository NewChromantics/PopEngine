#include "TV8Container.h"
#include "TV8Inspector.h"

#include "SoyDebug.h"
#include "SoyFilesystem.h"


//	normally I hate using namespace;'s...
using namespace v8;


bool ReportDefinedReturns = false;






void* TV8Allocator::Allocate(size_t length)
{
	auto* Bytes = static_cast<uint8_t*>( AllocateUninitialized(length) );
	
	for ( auto i=0;	i<length;	i++ )
		Bytes[i] = 0;
	
	return Bytes;
}

void* TV8Allocator::AllocateUninitialized(size_t length)
{
	return mHeap.AllocRaw(length);
}

void TV8Allocator::Free(void* data, size_t length)
{
	mHeap.FreeRaw(data, length);
}

std::string	v8::TCallback::GetArgumentString(size_t Index)
{
	throw Soy::AssertException("todo");
}

int32_t v8::TCallback::GetArgumentInt(size_t Index)
{
	throw Soy::AssertException("todo");
}

bool v8::TCallback::GetArgumentBool(size_t Index)
{
	throw Soy::AssertException("todo");
}

float v8::TCallback::GetArgumentFloat(size_t Index)
{
	throw Soy::AssertException("todo");
}


TV8Container::TV8Container(const std::string& RootDirectory) :
	mImageHeap		( true, true, "Image Heap", 0 , false ),
	mRootDirectory	( RootDirectory ),
	mAllocator		( "V8Container" )
{
	//	well this is an annoying interface
	std::string Flags = "--expose_gc";
	//v8::internal::FLAG_expose_gc = true;
	V8::SetFlagsFromString( Flags.c_str(), static_cast<int>(Flags.length()) );
	
	
	v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	
	auto V8RuntimePath = ::Platform::GetAppResourcesDirectory() + "v8Runtime/";

	
#if V8_VERSION==6
	std::string IcuPath = V8RuntimePath + "icudtl.dat";
	std::string NativesBlobPath = V8RuntimePath + "natives_blob.bin";
	std::string SnapshotBlobPath = V8RuntimePath + "snapshot_blob.bin";

	if ( !V8::InitializeICUDefaultLocation( nullptr, IcuPath.c_str() ) )
		throw Soy::AssertException("Failed to load ICU");
/*
	Array<char> NativesBlob;
	Array<char> SnapshotBlob;
	StartupData NativesBlobData;
	StartupData SnapshotBlobData;
	Soy::FileToArray( GetArrayBridge(NativesBlob), NativesBlobPath );
	Soy::FileToArray( GetArrayBridge(SnapshotBlob), SnapshotBlobPath );

	NativesBlobData={	NativesBlob.GetArray(), static_cast<int>(NativesBlob.GetDataSize())	};
	SnapshotBlobData={	SnapshotBlob.GetArray(), static_cast<int>(SnapshotBlob.GetDataSize())	};
	V8::SetNativesDataBlob(&NativesBlobData);
	V8::SetSnapshotDataBlob(&SnapshotBlobData);
*/
	//V8::InitializeExternalStartupData( mRootDirectory.c_str() );
	V8::InitializeExternalStartupData( NativesBlobPath.c_str(), SnapshotBlobPath.c_str() );
	
#elif V8_VERSION==5
	V8::InitializeICU(nullptr);
	//v8::V8::InitializeExternalStartupData(argv[0]);
	//V8::InitializeExternalStartupData(nullptr);
	V8::InitializeExternalStartupData( Platform::GetExePath().c_str() );
#endif
	
	//std::unique_ptr<v8::Platform> platform = v8::platform::CreateDefaultPlatform();
	mPlatform.reset( v8::platform::CreateDefaultPlatform() );
	V8::InitializePlatform( mPlatform.get() );
	V8::Initialize();

	// Create a new Isolate and make it the current one.
	//	gr: current??
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = &mAllocator;
	//create_params.snapshot_blob = &SnapshotBlobData;

	//	docs say "is owner" but there's no delete...
	mIsolate = v8::Isolate::New(create_params);
	
	//	we run the microtasks manually in our loop. This stops microtasks from occurring
	//	when we finish (end of stack) running when we call a js function arbritrarily 
	mIsolate->SetMicrotasksPolicy( v8::MicrotasksPolicy::kExplicit );
	
	//  for now, single context per isolate
	//	todo: abstract context to be per-script
	CreateContext();

	//CreateInspector();
}

void TV8Container::ProcessJobs(std::function<bool()> IsRunning)
{
	do
	{
		//std::Debug << "Pump message" << std::endl;
		v8::Locker Locker(mIsolate);
		mIsolate->Enter();
		bool MoreJobs = v8::platform::PumpMessageLoop( mPlatform.get(), mIsolate);
		mIsolate->Exit();
		
		if ( !MoreJobs )
			break;
	}
	while ( IsRunning() );
	//std::Debug << "EOF messages" << std::endl;
	
	
	auto RunMicroTasks = [this](v8::Local<v8::Context>)
	{
		mIsolate->RunMicrotasks();
	};
	RunScoped(RunMicroTasks);
}


std::string TV8Container::GetResolvedFilename(const std::string& Filename) const
{
	//	gr: do this better!
	//	gr: should be able to use NSUrl to resolve ~/ or / etc
	if ( Filename[0] == '/' )
		return Filename;
	
	std::stringstream FullFilename;
	FullFilename << mRootDirectory << Filename;
	return FullFilename.str();
}


void TV8Container::CreateContext()
{
    //#error check https://stackoverflow.com/questions/33168903/c-scope-and-google-v8-script-context
	v8::Locker locker(mIsolate);
	auto* isolate = mIsolate;
	v8::Isolate::Scope isolate_scope(isolate);

    //  always need a handle scope to collect locals
	v8::HandleScope handle_scope(isolate);
	Local<Context> ContextLocal = v8::Context::New(isolate);
    
    Context::Scope context_scope( ContextLocal );
    
	//  save the persistent	handle
	mContext = v8::GetPersistent( *isolate, ContextLocal );
}

void TV8Container::CreateInspector()
{
	v8::Locker locker(mIsolate);
	auto* isolate = mIsolate;
	v8::Isolate::Scope isolate_scope(isolate);
	
	//  always need a handle scope to collect locals
	v8::HandleScope handle_scope(isolate);

	mInspector.reset( new TV8Inspector(*this) );
}


void TV8Container::LoadScript(const std::string& Source,const std::string& SourceFilename)
{
	auto LoadScript = [=](v8::Local<v8::Context> Context)
	{
		this->LoadScript( Context, Source, SourceFilename );
	};
	
	QueueScoped( LoadScript );
}


Local<Value> TV8Container::LoadScript(Local<Context> context,const std::string& Source,const std::string& SourceFilename)
{
	auto* CStr = Source.c_str();
	if ( CStr == nullptr )
		CStr = "";
	
	auto* Isolate = context->GetIsolate();
	auto StringHandle = String::NewFromUtf8( Isolate, CStr );
	return LoadScript( context, StringHandle, SourceFilename );
}


Local<Value> TV8Container::LoadScript(Local<Context> context,Local<String> Source,const std::string& SourceFilename)
{
	auto* Isolate = context->GetIsolate();
	
	//	compile the source code.
	Local<Script> NewScript;
	{
		TryCatch trycatch(Isolate);
		
		auto OriginStr = v8::GetString(*Isolate, std::string("file://")+SourceFilename );
		auto OriginRow = v8::Integer::New( Isolate, 0 );
		auto OriginCol = v8::Integer::New( Isolate, 0 );
		auto Cors = v8::Boolean::New( Isolate, true );
		v8::ScriptOrigin Origin( OriginStr, OriginRow, OriginCol, Cors );
		
		auto NewScriptMaybe = Script::Compile(context, Source, &Origin );
		if ( NewScriptMaybe.IsEmpty() )
		{
			std::stringstream Error;
			Error << "Compiling script (" << SourceFilename << ")";
			throw V8Exception( trycatch, Error.str() );
		}
		NewScript = NewScriptMaybe.ToLocalChecked();
	}
	
	{
		TryCatch trycatch(Isolate);
		auto ScriptResultMaybe = NewScript->Run(context);
		if ( ScriptResultMaybe.IsEmpty() )
		{
			std::stringstream Error;
			Error << "Running script (" << SourceFilename << ")";
			throw V8Exception( trycatch, Error.str() );
		}
		
		//	gr: scripts can never return anything, so this would always be undefined...
		auto ScriptResult = ScriptResultMaybe.ToLocalChecked();
		if ( !ScriptResult->IsUndefined() )
		{
			v8::String::Utf8Value MainResultStr( ScriptResult );
			std::Debug << "LoadScript( " << SourceFilename << ") -> " << *MainResultStr << " (" << v8::GetTypeName(ScriptResult) << ")" << std::endl;
		}
		return ScriptResult;
	}
}


Bind::TObject TV8Container::GetRootGlobalObject()
{
	//	this should be inside a scope...
	auto Context = mContext->GetLocal( GetIsolate() );
	auto Global = Context->Global();
	v8::TObject GlobalObject( *this, Global );
	return GlobalObject;
}

void TV8Container::BindObjectType(const std::string& ObjectName,std::function<Local<FunctionTemplate>(TV8Container&)> GetTemplate,TV8ObjectTemplate::ALLOCATOR Allocator,const std::string& ParentObjectName)
{
	throw Soy::AssertException("Todo: genericify this!");
	
	auto Bind = [&](Local<v8::Context> Context)
	{
		/*
		auto* Isolate = Context->GetIsolate();
		auto Global = GetGlobalObject( ParentObjectName );
		
    	//	create new function
    	auto Template = GetTemplate(*this);
    	auto FuncWrapperValue = Template->GetFunction();
		auto ObjectNameStr = v8::GetString( *Isolate, ObjectName);
    	auto SetResult = Global->Set( Context, ObjectNameStr, FuncWrapperValue );
		if ( SetResult.IsNothing() || !SetResult.ToChecked() )
		{
			std::stringstream Error;
			Error << "Failed to set " << ObjectName << " on Global." << ParentObjectName;
			throw Soy::AssertException(Error.str());
		}
		
		//	store the template so we can reference it later
		auto ObjectTemplate = Template->InstanceTemplate();
		auto ObjectTemplatePersistent = v8::GetPersistent( *Isolate, ObjectTemplate );
		TV8ObjectTemplate NewTemplate( ObjectTemplatePersistent, ObjectName );
		NewTemplate.mAllocator = Allocator;
		mObjectTemplates.PushBack(NewTemplate);
		 */
	};
	RunScoped(Bind);
}



void TV8Container::BindRawFunction(v8::Local<v8::Object> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&))
{
	auto Bind = [&](Local<v8::Context> Context)
	{
		auto* Isolate = Context->GetIsolate();

		v8::Local<v8::FunctionTemplate> LogFuncWrapper = v8::FunctionTemplate::New( Isolate, RawFunction );
		auto LogFuncWrapperValue = LogFuncWrapper->GetFunction();
		auto* FunctionNameCstr = FunctionName;
		auto SetResult = This->Set( Context, v8::String::NewFromUtf8(Isolate, FunctionNameCstr), LogFuncWrapperValue);
		if ( !SetResult.ToChecked() )
		{
			std::stringstream Error;
			Error << "Failed to set function " << FunctionName << " on object (" << v8::GetTypeName(This) << ")";
			throw Soy::AssertException( Error.str() );
		}
	};
	RunScoped(Bind);
	
}


void TV8Container::BindRawFunction(v8::Local<v8::ObjectTemplate> This,const char* FunctionName,void(*RawFunction)(const v8::FunctionCallbackInfo<v8::Value>&))
{
	auto Bind = [&](Local<v8::Context> Context)
	{
		auto* Isolate = Context->GetIsolate();
		
		v8::Local<v8::FunctionTemplate> FuncWrapper = v8::FunctionTemplate::New( Isolate, RawFunction );
		
		This->Set( Isolate, FunctionName, FuncWrapper);
	};
	RunScoped(Bind);
}


void TV8Container::QueueScoped(std::function<void(v8::Local<v8::Context>)> Lambda)
{
	//	gr: who owns this task?
	auto* Task = new LambdaTask( Lambda, *this );
	this->mPlatform->CallOnForegroundThread( mIsolate, Task );
}


void TV8Container::QueueDelayScoped(std::function<void(v8::Local<v8::Context>)> Lambda,size_t DelayMs)
{
	//	gr: who owns this task?
	auto* Task = new LambdaTask( Lambda, *this );
	auto DelayDouble = DelayMs / 1000.0;
	this->mPlatform->CallDelayedOnForegroundThread( mIsolate, Task, DelayDouble );
}

void TV8Container::Yield(size_t SleepMilliseconds)
{
	//	gr: temporary unlock, but need to exit&enter too
	{
		mIsolate->Exit();
		v8::Unlocker unlocker(mIsolate);
	
		//	isolate unlock for a moment, let another thread jump in and run stuff
		auto ms = std::chrono::milliseconds(SleepMilliseconds);
		std::this_thread::sleep_for( ms );
	}
	//	re-enter after unlocker has gone out of scope
	mIsolate->Enter();
}


void TV8Container::RunScoped(std::function<void(v8::Local<v8::Context>)> Lambda)
{
	auto* isolate = mIsolate;
	
	//	gr: we're supposed to lock the isolate here, but the setup we have,
	//	this should only ever be called on the JS thread[s] anyway
	//	maybe have a recursive mutex and throw if already locked
	v8::Locker locker(mIsolate);
	mIsolate->Enter();
	try
	{
		//  setup scope. handle scope always required to GC locals
		Isolate::Scope isolate_scope(isolate);
		HandleScope handle_scope(isolate);
		//	grab a local
		auto Context = mContext->GetLocal(*isolate);
		Context::Scope context_scope( Context );

		//	gr: auto catch and turn into a c++ exception
		TryCatch trycatch(isolate);
		Lambda( Context );
		if ( trycatch.HasCaught() )
			throw V8Exception( trycatch, "Running Javascript func" );
		mIsolate->Exit();
	}
	catch(...)
	{
		mIsolate->Exit();
		throw;
	}
}



v8::Local<v8::Value> TV8Container::ExecuteFuncAndCatch(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>& Params)
{
	auto* isolate = ContextHandle->GetIsolate();
	try
	{
		return ExecuteFunc( ContextHandle, FunctionHandle, This, Params );
	}
	/*	gr: can we send the exception object straight back?
	catch(V8Exception& e)
	{
		return e.mLocalException;
	}
	 */
	catch(std::exception& e)
	{
		//	catch exceptions and turn them into new v8 exceptions
		auto Exception = v8::GetException( *isolate, e );
		return Exception;
	}
}



v8::Local<v8::Value> TV8Container::ExecuteFunc(v8::Local<v8::Context> ContextHandle,v8::Local<v8::Function> FunctionHandle,v8::Local<v8::Object> This,ArrayBridge<v8::Local<v8::Value>>& Params)
{
	auto& Func = FunctionHandle;
	auto* isolate = ContextHandle->GetIsolate();

	//	default this to the global
	if ( This.IsEmpty() )
		This = ContextHandle->Global();
		
		
	auto ArgCount = Params.GetSize();
	auto* Args = Params.GetArray();
	
	//	run, and catch any v8 exceptions and throw them back to C
	TryCatch trycatch(isolate);
	Soy::TScopeTimerPrint Timer("JS ExecuteFunc",50);
	auto ResultMaybe = Func->Call( ContextHandle, This, size_cast<int>(ArgCount), Args );
	Timer.Stop();
	if ( ResultMaybe.IsEmpty() )
	{
		throw V8Exception( trycatch, "ExecuteFunc(???)");
	}

	auto Result = ResultMaybe.ToLocalChecked();
		
	//	report anything that isn't undefined
	if ( ReportDefinedReturns && !Result->IsUndefined() )
	{
		String::Utf8Value ResultStr(Result);
		std::Debug << *ResultStr << std::endl;
	}
	return Result;
}


Local<Function> v8::GetFunction(Local<Context> ContextHandle,Local<Object> This,const std::string& FunctionName)
{
	auto* Isolate = ContextHandle->GetIsolate();
	auto FuncNameKey = v8::GetString( *Isolate, FunctionName );
	
	//  get the global object for this name
	auto FunctionHandle = This->Get( ContextHandle, FuncNameKey).ToLocalChecked();
	
	auto Func = v8::SafeCast<v8::Function>( FunctionHandle );
	return Func;
}





void OnFree(const WeakCallbackInfo<void>& data)
{
	std::Debug << "Leaking object created with CreateObjectInstance()!" << std::endl;
	auto* Image = data.GetParameter();
	//delete Image;
}

TV8ObjectTemplate::ALLOCATOR TV8Container::GetAllocator(const char* TYPENAME)
{
	//	find template
	auto* pObjectTemplate = mObjectTemplates.Find( TYPENAME );
	if ( !pObjectTemplate )
	{
		std::stringstream Error;
		Error << "Unknown object typename " << TYPENAME;
		auto ErrorStr = Error.str();
		throw Soy::AssertException(ErrorStr);
	}
	return pObjectTemplate->mAllocator;
}


Bind::TObject TV8Container::CreateObjectInstance(const std::string& ObjectTypeName)
{
	auto& Isolate = GetIsolate();

	//	create basic object
	if ( ObjectTypeName.length() == 0 || ObjectTypeName == "Object" )
	{
		auto NewObject = v8::Object::New( &Isolate );
		v8::TObject NewObjectv8( *this, NewObject );
		return NewObjectv8;
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
	auto ObjectTemplateLocal = ObjectTemplate.mTemplate->GetLocal(Isolate);
	auto NewObject = ObjectTemplateLocal->NewInstance();
	
	v8::TObject NewObjectv8( *this, NewObject );
	return NewObjectv8;
}

v8::Local<v8::Object> TV8Container::CreateObjectInstance(const std::string& ObjectTypeName,void* Object)
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
	
	//	instance new one
	auto& Isolate = GetIsolate();
	auto& ObjectTemplate = *pObjectTemplate;
	auto ObjectTemplateLocal = ObjectTemplate.mTemplate->GetLocal(Isolate);
	auto NewObject = ObjectTemplateLocal->NewInstance();

	//	make a persistent handle here
	v8::Persist<v8::Object> NewObjectHandle;
	NewObjectHandle.Reset( &Isolate, NewObject );
	NewObjectHandle.SetWeak( Object, OnFree, v8::WeakCallbackType::kInternalFields );
	
	//	gr: do this assignment in the class as we may have class specific stuff
	//		really it'll be done in a binding/wrapper base class anyway
	auto ObjectPointerHandle = External::New( &Isolate, Object );
	auto ThisHandle = External::New( &Isolate, this );
	NewObject->SetInternalField(0, ObjectPointerHandle);
	NewObject->SetInternalField(1, ThisHandle);

	return NewObject;
}

void TV8Container::CreateGlobalObjectInstance(const std::string& ObjectTypeName,const std::string& FullObjectName)
{
	auto Allocate = [&](v8::Local<v8::Context> Context)
	{
		Bind::TContext::CreateGlobalObjectInstance( ObjectTypeName, FullObjectName );
		/*
		auto ParentName = FullObjectName;
		auto ObjectName = Soy::StringPopRight( ParentName, '.' );
		
		auto& Isolate = *Context->GetIsolate();
		auto Global = GetGlobalObject( Context, ParentName );
		auto NewObject = CreateObjectInstance( ObjectTypeName );
		auto ObjectNameStr = v8::GetString( Isolate, ObjectName );
		auto SetResult = Global->Set( Context, ObjectNameStr, NewObject );
		if ( !SetResult.ToChecked() )
		{
			std::stringstream Error;
			Error << "Failed to set " << ObjectName << "(" << ObjectTypeName << ") on Global." << ParentName;
			throw Soy::AssertException(Error.str());
		}
		 */
	};
	RunScoped( Allocate );
}



std::string v8::GetTypeName(v8::Local<v8::Value> Handle)
{
	if ( Handle->IsUndefined() )	return "Undefined";
	if ( Handle->IsNull() )			return "Null";
	if ( Handle->IsFunction() )		return "Function";
	if ( Handle->IsString())		return "String";
	if ( Handle->IsMap() )			return "Map";
	if ( Handle->IsDate())			return "Date";
	if ( Handle->IsArray() )		return "Array";
	if ( Handle->IsBoolean() )		return "Boolean";
	if ( Handle->IsNumber() )		return "Number";
	if ( Handle->IsPromise() )		return "Promise";
	if ( Handle->IsFloat32Array() )	return "Float32Array";
	if ( Handle->IsFloat64Array() )	return "Float64Array";
	if ( Handle->IsUint8ClampedArray() )	return "Uint8ClampedArray";
	if ( Handle->IsInt8Array() )	return "Int8Array";
	if ( Handle->IsInt16Array() )	return "Int16Array";
	if ( Handle->IsInt32Array() )	return "Int32Array";
	if ( Handle->IsUint8Array() )	return "Uint8Array";
	if ( Handle->IsUint16Array() )	return "Uint16Array";
	if ( Handle->IsUint32Array() )	return "Uint32Array";
	
	if ( Handle->IsObject() )		return "Object";

	return "Unknown type";
}


//	uint8_t -> uint8clampedarray
Local<v8::Value> v8::GetTypedArray(v8::Isolate& Isolate,ArrayBridge<uint8_t>&& Values)
{
	std::stringstream TimerName;
	TimerName << "v8::GetTypedArray( " << Values.GetSize() << " )";
	Soy::TScopeTimerPrint Timer(TimerName.str().c_str(), 5 );
	
	Soy::TScopeTimerPrint Timer3("GetTypedArray<uint8_t> alloc v8 arrays", 5 );
	auto Size = Values.GetDataSize();
	auto Rgba8Buffer = v8::ArrayBuffer::New( &Isolate, Size );
	auto Rgba8BufferContents = Rgba8Buffer->GetContents();
	Timer3.Stop();
	auto Rgba8DataArray = GetRemoteArray( static_cast<uint8_t*>( Rgba8BufferContents.Data() ), Rgba8BufferContents.ByteLength() );

	{
		Soy::TScopeTimerPrint Timer2("GetTypedArray<uint8_t> Copy()", 5 );
		Rgba8DataArray.Copy( Values );
	}
	
	Soy::TScopeTimerPrint Timer4("GetTypedArray<uint8_t> v8::Uint8ClampedArray::New", 5 );
	auto Rgba8 = v8::Uint8ClampedArray::New( Rgba8Buffer, 0, Rgba8Buffer->ByteLength() );
	Timer3.Stop();
	
	return Rgba8;
}


//	uint8_t -> uint8clampedarray
void v8::CopyToTypedArray(v8::Isolate& Isolate,ArrayBridge<uint8_t>&& Values,Local<v8::Value> ArrayHandle)
{
	std::stringstream TimerName;
	TimerName << "v8::CopyToTypedArray( " << Values.GetSize() << " )";
	Soy::TScopeTimerPrint Timer(TimerName.str().c_str(), 10 );

	auto u8ArrayHandle = v8::SafeCast<v8::Uint8ClampedArray>( ArrayHandle );
	auto Buffer = u8ArrayHandle->Buffer();
	auto BufferContents = Buffer->GetContents();
	auto u8DataArray = GetRemoteArray( static_cast<uint8_t*>( BufferContents.Data() ), BufferContents.ByteLength() );
	u8DataArray.Copy( Values );
	/*
	auto Size = Values.GetDataSize();
	auto Rgba8Buffer = v8::ArrayBuffer::New( &Isolate, Size );
	auto Rgba8BufferContents = Rgba8Buffer->GetContents();
	auto Rgba8DataArray = GetRemoteArray( static_cast<uint8_t*>( Rgba8BufferContents.Data() ), Rgba8BufferContents.ByteLength() );
	Rgba8DataArray.Copy( Values );
	auto Rgba8 = v8::Uint8ClampedArray::New( Rgba8Buffer, 0, Rgba8Buffer->ByteLength() );
	return Rgba8;
	 */
}


void v8::EnumArray(Local<Array> ArrayHandle,std::function<void(size_t,Local<Value>)> EnumElement)
{
	for ( auto i=0;	i<ArrayHandle->Length();	i++ )
	{
		auto ElementHandle = ArrayHandle->Get(i);
		EnumElement( i, ElementHandle );
	}
}


void v8::EnumArray(Local<Value> ValueHandle,ArrayBridge<float>&& FloatArray,const std::string& Context)
{
	EnumArray( ValueHandle, FloatArray, Context );
}

void v8::EnumArray(Local<Value> ValueHandle,ArrayBridge<int>&& IntArray,const std::string& Context)
{
	EnumArray( ValueHandle, IntArray, Context );
}




void v8::EnumArray(v8::Local<v8::Value> ValueHandle,ArrayBridge<float>& FloatArray,const std::string& Context)
{
	if ( ValueHandle->IsNumber() )
	{
		auto ValueFloat = Local<Number>::Cast( ValueHandle );
		FloatArray.PushBack( ValueFloat->Value() );
	}
	else if ( ValueHandle->IsArray() )
	{
		//	we recursively expand arrays
		//	really we should only allow one level deep and check against the uniform (to allow arrays of vec4)
		auto ValueArray = Local<v8::Array>::Cast( ValueHandle );
		for ( auto i=0;	i<ValueArray->Length();	i++ )
		{
			auto ElementHandle = ValueArray->Get(i);
			EnumArray( ElementHandle, FloatArray, Context );
		}
	}
	else if ( ValueHandle->IsFloat32Array() )
	{
		EnumArray<Float32Array>( ValueHandle, GetArrayBridge(FloatArray) );
	}
	else
	{
		std::stringstream Error;
		Error << "Unhandled element type(" << v8::GetTypeName(ValueHandle) << ") in EnumArray<float>. Context: " << Context;
		throw Soy::AssertException(Error.str());
	}
}


void v8::EnumArray(v8::Local<v8::Value> ValueHandle,ArrayBridge<int>& IntArray,const std::string& Context)
{
	if ( ValueHandle->IsNumber() )
	{
		auto ValueFloat = Local<Number>::Cast( ValueHandle );
		IntArray.PushBack( ValueFloat->Value() );
	}
	else if ( ValueHandle->IsArray() )
	{
		//	we recursively expand arrays
		//	really we should only allow one level deep and check against the uniform (to allow arrays of vec4)
		auto ValueArray = Local<v8::Array>::Cast( ValueHandle );
		for ( auto i=0;	i<ValueArray->Length();	i++ )
		{
			auto ElementHandle = ValueArray->Get(i);
			EnumArray( ElementHandle, IntArray, Context );
		}
	}
	else if ( ValueHandle->IsInt32Array() )
	{
		::Array<int32_t> Ints;
		EnumArray<Int32Array>( ValueHandle, GetArrayBridge(Ints) );
		IntArray.PushBackArray(Ints);
	}
	else if ( ValueHandle->IsUint32Array() )
	{
		::Array<uint32_t> Ints;
		EnumArray<Uint32Array>( ValueHandle, GetArrayBridge(Ints) );
		IntArray.PushBackArray(Ints);
	}
	else if ( ValueHandle->IsInt16Array() )
	{
		::Array<int16_t> Ints;
		EnumArray<Int16Array>( ValueHandle, GetArrayBridge(Ints) );
		IntArray.PushBackArray(Ints);
	}
	else if ( ValueHandle->IsUint16Array() )
	{
		::Array<uint16_t> Ints;
		EnumArray<Uint16Array>( ValueHandle, GetArrayBridge(Ints) );
		IntArray.PushBackArray(Ints);
	}
	else if ( ValueHandle->IsInt8Array() )
	{
		::Array<int8_t> Ints;
		EnumArray<Int8Array>( ValueHandle, GetArrayBridge(Ints) );
		IntArray.PushBackArray(Ints);
	}
	else if ( ValueHandle->IsUint8Array() )
	{
		::Array<uint8_t> Ints;
		EnumArray<Uint8Array>( ValueHandle, GetArrayBridge(Ints) );
		IntArray.PushBackArray(Ints);
	}
	else if ( ValueHandle->IsUint8ClampedArray() )
	{
		::Array<uint8_t> Ints;
		EnumArray<Uint8ClampedArray>( ValueHandle, GetArrayBridge(Ints) );
		IntArray.PushBackArray(Ints);
	}
	else
	{
		std::stringstream Error;
		Error << "Unhandled element type(" << v8::GetTypeName(ValueHandle) << ") in EnumArray<int>. Context: " << Context;
		throw Soy::AssertException(Error.str());
	}
}


void v8::LambdaTask::Run()
{
	mContainer.RunScoped( mLambda );
}

std::string v8::GetString(Local<Value> Str)
{
	if ( !Str->IsString() )
		throw Soy::AssertException("Not a string");
	
	String::Utf8Value ExceptionStr(Str);
	const auto* ExceptionCStr = *ExceptionStr;
	if ( ExceptionCStr == nullptr )
		ExceptionCStr = "<null> (Possibly not a string)";
	
	std::string NewStr( ExceptionCStr );
	return NewStr;
}


Local<Value> v8::GetString(v8::Isolate& Isolate,const std::string& Str)
{
	auto* CStr = Str.c_str();
	return GetString(Isolate,CStr);
}

Local<Value> v8::GetString(v8::Isolate& Isolate,const char* CStr)
{
	if ( CStr == nullptr )
		CStr = "";

	auto StringHandle = String::NewFromUtf8( &Isolate, CStr );
	auto StringValue = Local<Value>::Cast( StringHandle );
	return StringValue;
}


v8::Local<v8::Array> v8::GetArray(v8::Isolate& Isolate,size_t ElementCount,std::function<Local<Value>(size_t)> GetElement)
{
	auto ArrayHandle = Array::New( &Isolate );
	for ( auto i=0;	i<ElementCount;	i++ )
	{
		auto ValueHandle = GetElement(i);
		ArrayHandle->Set( i, ValueHandle );
	}
	return ArrayHandle;
}

void* v8::GetObject(v8::Local<v8::Value> Handle)
{
	//	if this isn't an external, lets assume it's it's inernal field
	if ( !Handle->IsExternal())
	{
		if ( Handle->IsObject() )
		{
			auto HandleObject = v8::Local<v8::Object>::Cast( Handle );
			Handle = HandleObject->GetInternalField(0);
		}
	}
	
	if ( !Handle->IsExternal() )
	{
		std::stringstream Error;
		Error << "Getting object from Value(" << v8::GetTypeName(Handle) << ") is not internally backed (!IsExternal)";
		throw Soy::AssertException(Error.str());
	}
	
	//	gr: this needs to do type checks, and we need to verify the internal type as we're blindly casting!
	//	gr: also, to deal with multiple inheritence,
	//		cast this to the base object wrapper, then dynamic cast to T (which'll solve all our problems)
	auto* VoidPtr = v8::Local<v8::External>::Cast( Handle )->Value();
	if ( VoidPtr == nullptr )
		throw Soy::AssertException("Internal Field is null");
	return VoidPtr;
}


v8::TCallback::TCallback(const v8::FunctionCallbackInfo<v8::Value>& Params,TV8Container& Container) :
	Bind::TCallback	( static_cast<Bind::TContext&>(Container) ),
	mParams			( Params ),
	mIsolate		( mParams.GetIsolate() ),
	mContext		( mIsolate->GetCurrentContext() )
{
}
	

void v8::TCallback::GetArgumentArray(size_t Index,ArrayBridge<uint8_t>&& Array)
{
	auto Handle = mParams[Index];
	v8::EnumArray<v8::Uint8Array>( Handle, Array );
}

void v8::TCallback::GetArgumentArray(size_t Index,ArrayBridge<float>&& Array)
{
	auto Handle = mParams[Index];
	v8::EnumArray( Handle, Array, "GetArgumentArray<float>" );
}

/*
Bind::TObject v8::TCallback::ThisObject()
{
	return TObject( mParams.This() );
}
*/

v8::TPromise::TPromise(v8::Isolate& Isolate) :
	mIsolate	( Isolate )
{
	auto Resolver = v8::Promise::Resolver::New( &mIsolate );
	mResolver = v8::GetPersistent( mIsolate, Resolver );
}

void v8::TPromise::Resolve(Bind::TObject Value)
{
	auto ResolverLocal = mResolver->GetLocal(mIsolate);
	auto& Object = dynamic_cast<v8::TObject&>(Value);
	ResolverLocal->Resolve( Object.mObject );
}

void v8::TPromise::Reject(const std::string& Value)
{
	auto ResolverLocal = mResolver->GetLocal(mIsolate);
	auto String = v8::GetString( mIsolate, Value );
	ResolverLocal->Reject( String );
}
