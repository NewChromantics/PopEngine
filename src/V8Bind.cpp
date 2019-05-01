#include "V8Bind.h"
#include "SoyDebug.h"
#include "SoyFileSystem.h"

#include "libplatform/libplatform.h"
#include "include/v8.h"

#include "TBind.h"

#define THROW_TODO	throw Soy::AssertException( __FUNCTION__ )

/*
template<typename TYPE>
bool		IsType(Local<Value>& ValueHandle);

#define ISTYPE_DEFINITION(TYPE)	\
template<> inline bool v8::IsType<v8::TYPE>(Local<Value>& ValueHandle)	{	return ValueHandle->Is##TYPE();	}

ISTYPE_DEFINITION(Int8Array);
ISTYPE_DEFINITION(Uint8Array);
ISTYPE_DEFINITION(Uint8ClampedArray);
ISTYPE_DEFINITION(Int16Array);
ISTYPE_DEFINITION(Uint16Array);
ISTYPE_DEFINITION(Int32Array);
ISTYPE_DEFINITION(Uint32Array);
ISTYPE_DEFINITION(Float32Array);
ISTYPE_DEFINITION(Number);
ISTYPE_DEFINITION(Function);
ISTYPE_DEFINITION(Boolean);
ISTYPE_DEFINITION(Array);
*/


/*
//	our own type caster which throws if cast fails.
//	needed because my v8 built doesnt have cast checks, and I can't determine if they're enabled or not
template<typename TYPE>
inline v8::Local<TYPE> v8::SafeCast(v8::Local<v8::Value> ValueHandle)
{
	if ( !IsType<TYPE>(ValueHandle) )
	{
		std::stringstream Error;
		Error << "Trying to cast " << GetTypeName(ValueHandle) << " to other type " << Soy::GetTypeName<TYPE>();
		throw Soy::AssertException(Error.str());
	}
	return ValueHandle.As<TYPE>();
}
*/

template<typename TYPE>
v8::Local<v8::Value> ToValue(v8::Local<TYPE>& Value)
{
	return Value.template As<v8::Value>();
}

JSContextGroupRef::JSContextGroupRef(std::nullptr_t) :
	V8::TVirtualMachine	(nullptr)
{
	//	gr: don't throw. Just let this be in an invalid state for initialisation of variables
}

	
void JSObjectRef::operator=(std::nullptr_t Null)
{
	THROW_TODO;
}

void JSObjectRef::operator=(JSObjectRef That)
{
	THROW_TODO;
}


JSValueRef::JSValueRef(JSObjectRef Object) :
	LocalRef	( ToValue(Object.mThis) )
{
}

JSValueRef::JSValueRef(v8::Local<v8::Value>& Local) :
	LocalRef	( Local )
{
}

void JSValueRef::operator=(JSObjectRef That)
{
	THROW_TODO;
}

void JSValueRef::operator=(std::nullptr_t Null)
{
	THROW_TODO;
}


void JSStringRef::operator=(std::nullptr_t Null)
{
	THROW_TODO;
}






void		JSObjectSetPrivate(JSObjectRef Object,void* Data)
{
	THROW_TODO;
}

void*		JSObjectGetPrivate(JSObjectRef Object)
{
	THROW_TODO;
}

JSObjectRef	JSObjectMake(JSContextRef Context,JSClassRef Class,void*)
{
	if ( !Class )
	{
		auto NewObject = v8::Object::New( &Context.GetIsolate() );
		return JSObjectRef( NewObject );
	}

	if ( !Class.mTemplate )
		throw Soy::AssertException("Expected template in class");

	auto ObjectTemplate = Class.mTemplate->GetLocal( Context.GetIsolate() );
	auto NewObjectLocal = ObjectTemplate->NewInstance();
	return JSObjectRef( NewObjectLocal );
}

JSValueRef	JSObjectGetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef* Exception)
{
	THROW_TODO;
}

void JSObjectSetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception )
{
	auto NameHandle = ToValue( Name.mThis );
	auto Result = This.mThis->Set( Context.mThis, NameHandle, Value.mThis );

	if ( Result.IsNothing() || !Result.ToChecked() )
		throw Soy::AssertException("Failed to set member");
}

void		JSObjectSetPropertyAtIndex(JSContextRef Context,JSObjectRef This,size_t Index,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}


JSType		JSValueGetType(JSContextRef Context,JSValueRef Value)
{
	if ( !Value )
		return kJSTypeUndefined;
#define TEST_IS(TYPE,JSTYPE)	if ( Value.mThis->Is##TYPE() )	return JSTYPE
	TEST_IS( Undefined, kJSTypeUndefined );
	TEST_IS( Null, kJSTypeNull );
	TEST_IS( String, kJSTypeString );
	
	TEST_IS( Object, kJSTypeObject );
	TEST_IS( ArgumentsObject, kJSTypeObject );
	TEST_IS( Promise, kJSTypeObject );
	TEST_IS( Function, kJSTypeObject );
	TEST_IS( Array, kJSTypeObject );
	TEST_IS( ArrayBufferView, kJSTypeObject );
	TEST_IS( TypedArray, kJSTypeObject );
	TEST_IS( Uint8Array, kJSTypeObject );
	TEST_IS( Array, kJSTypeObject );

	TEST_IS( Boolean, kJSTypeBoolean );
	
	TEST_IS( Number, kJSTypeNumber );
	TEST_IS( Int32, kJSTypeNumber );
	TEST_IS( Uint32, kJSTypeNumber );
	
	throw Soy::AssertException("v8 value didn't match any type");
}

bool		JSValueIsObject(JSContextRef Context,JSValueRef Value)
{
	return Value.mThis->IsObject();
}

bool		JSValueIsObject(JSContextRef Context,JSObjectRef Value)
{
	THROW_TODO;
}

JSObjectRef JSValueToObject(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

void		JSValueProtect(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

void		JSValueUnprotect(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}


JSPropertyNameArrayRef	JSObjectCopyPropertyNames(JSContextRef Context,JSObjectRef This)
{
	THROW_TODO;
}

size_t		JSPropertyNameArrayGetCount(JSPropertyNameArrayRef Keys)
{
	THROW_TODO;
}

JSStringRef	JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef Keys,size_t Index)
{
	THROW_TODO;
}


bool		JSValueIsNumber(JSContextRef Context,JSValueRef Value)
{
	return Value.mThis->IsNumber();
}

double		JSValueToNumber(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSValueRef	JSValueMakeNumber(JSContextRef Context,int Value)
{
	THROW_TODO;
}


bool		JSObjectIsFunction(JSContextRef Context,JSObjectRef Value)
{
	return Value.mThis->IsFunction();
}

JSValueRef	JSObjectCallAsFunction(JSContextRef Context,JSObjectRef Object,JSObjectRef This,size_t ArgumentCount,JSValueRef* Arguments,JSValueRef* Exception)
{
	THROW_TODO;
}

JSValueRef	JSObjectMakeFunctionWithCallback(JSContextRef Context,JSStringRef Name,JSObjectCallAsFunctionCallback FunctionPtr)
{
	THROW_TODO;
}


bool		JSValueToBoolean(JSContextRef Context,JSValueRef Value)
{
	auto Bool = Value.mThis.As<v8::Boolean>();
	return Bool->Value();
}

JSValueRef	JSValueMakeBoolean(JSContextRef Context,bool Value)
{
	THROW_TODO;
}


JSValueRef	JSValueMakeUndefined(JSContextRef Context)
{
	auto Undefined = v8::Undefined( &Context.GetIsolate() );
	auto Value = ToValue( Undefined );
	return JSValueRef( Value );
}

bool		JSValueIsUndefined(JSContextRef Context,JSValueRef Value)
{
	return Value.mThis->IsUndefined();
}


JSValueRef	JSValueMakeNull(JSContextRef Context)
{
	auto Null = v8::Null( &Context.GetIsolate() );
	auto Value = ToValue( Null );
	return JSValueRef( Value );
}

bool		JSValueIsNull(JSContextRef Context,JSValueRef Value)
{
	return Value.mThis->IsNull();
}


JSObjectRef	JSObjectMakeArray(JSContextRef Context,size_t ElementCount,const JSValueRef* Elements,JSValueRef* Exception)
{
	THROW_TODO;
}

bool		JSValueIsArray(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

JSTypedArrayType	JSValueGetTypedArrayType(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSObjectRef	JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef Context,JSTypedArrayType ArrayType,void* Buffer,size_t BufferSize,JSTypedArrayBytesDeallocator Dealloc,void* DeallocContext,JSValueRef* Exception)
{
	THROW_TODO;
}

void*		JSObjectGetTypedArrayBytesPtr(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	THROW_TODO;
}

size_t		JSObjectGetTypedArrayByteOffset(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	THROW_TODO;
}

size_t		JSObjectGetTypedArrayLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	THROW_TODO;
}

size_t		JSObjectGetTypedArrayByteLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	THROW_TODO;
}


JSValueRef			JSEvaluateScript(JSContextRef Context,JSStringRef Source,JSObjectRef This,JSStringRef Filename,int LineNumber,JSValueRef* Exception)
{
	THROW_TODO;
}

JSGlobalContextRef	JSContextGetGlobalContext(JSContextRef Context)
{
	THROW_TODO;
}

JSObjectRef			JSContextGetGlobalObject(JSContextRef Context)
{
	auto Global = Context.mThis->Global();
	return JSObjectRef( Global );
}

JSContextGroupRef	JSContextGroupCreate()
{
	throw Soy::AssertException("In v8 implementation we need the runtime directory, use overloaded version");
}

JSContextGroupRef	JSContextGroupCreate(const std::string& RuntimeDirectory)
{
	JSContextGroupRef NewVirtualMachine( RuntimeDirectory );
	return NewVirtualMachine;
}

void JSContextGroupRelease(JSContextGroupRef ContextGroup)
{
	//	try and release all members here and maybe check for dangling refcounts
}

JSGlobalContextRef		JSGlobalContextCreateInGroup(JSContextGroupRef ContextGroup,JSClassRef GlobalClass)
{
	return ContextGroup.CreateContext();
}

void				JSGlobalContextSetName(JSGlobalContextRef Context,JSStringRef Name)
{
	//	todo: get name from string and set
	Context.mName = "New Name";
}

void				JSGlobalContextRelease(JSGlobalContextRef Context)
{
	THROW_TODO;
}

void				JSGarbageCollect(JSContextRef Context)
{
	THROW_TODO;
}


JSStringRef	JSStringCreateWithUTF8CString(JSContextRef Context,const char* Buffer)
{
	auto& Isolate = Context.GetIsolate();
	auto Handle = v8::String::NewFromUtf8( &Isolate, Buffer );
	return JSStringRef( Handle );
}

size_t		JSStringGetUTF8CString(JSStringRef String,char* Buffer,size_t BufferSize)
{
	THROW_TODO;
}

size_t		JSStringGetLength(JSStringRef String)
{
	THROW_TODO;
}

JSStringRef	JSValueToStringCopy(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}

JSValueRef	JSValueMakeString(JSContextRef Context,JSStringRef String)
{
	THROW_TODO;
}

void		JSStringRelease(JSStringRef String)
{
	//	can just let this go out of scope for now
}


void Constructor(const v8::FunctionCallbackInfo<v8::Value>& Meta)
{
	std::Debug << "Constructor" << std::endl;
};


JSClassRef	JSClassCreate(JSContextRef Context,JSClassDefinition* Definition)
{
	auto* Isolate = &Context.GetIsolate();

	//	make constructor
	//auto* Pointer = nullptr;
	//auto PointerHandle = External::New( Isolate, Pointer ).As<Value>();
	//auto ConstructorFunc = v8::FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	auto ConstructorFunc = v8::FunctionTemplate::New( Isolate, Definition->callAsConstructor );

	//	gr: from v8::Local<v8::FunctionTemplate> TObjectWrapper<TYPENAME,TYPE>::CreateTemplate(TV8Container& Container)
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);

	//	bind the static funcs
	{
		int i=0;
		while ( true )
		{
			auto& FunctionDefinition = Definition->staticFunctions[i];
			i++;
			if ( FunctionDefinition.name == nullptr )
				break;
			
			//	bind function to template
			auto This = InstanceTemplate;
			v8::Local<v8::FunctionTemplate> FunctionTemplateLocal = v8::FunctionTemplate::New( Isolate, FunctionDefinition.callAsFunction );
			auto FunctionLocal = FunctionTemplateLocal->GetFunction();
			//auto FunctionNameStr = JSStringCreateWithUTF8CString( Context, FunctionDefinition.name );
			This->Set( Isolate, FunctionDefinition.name, FunctionTemplateLocal);
			/*auto SetResult = This->Set( Isolate, FunctionDefinition.name, FunctionLocal);
			if ( !SetResult.ToChecked() || SetResult.IsNothing() )
			{
				std::stringstream Error;
				Error << "Failed to set function " << FunctionDefinition.name << " on class ";
				throw Soy::AssertException( Error );
			}
			*/
		}
	}
	
	
	auto Template = V8::GetPersistent( *Isolate, InstanceTemplate );

	JSClassRef NewClass( nullptr );
	NewClass.mTemplate = Template;
	return NewClass;

	
/*
	//	need a v8 version of whatever func
	typedef JSObjectRef(*JSObjectCallAsConstructorCallback) (JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

	typedef void (*FunctionCallback)(const FunctionCallbackInfo<Value>& info);
*/

	/*
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);
	
	throw Soy::AssertException("Needs refactor to Bind::");
	//Container.BindFunction<ExecuteKernel_FunctionName>( InstanceTemplate, ExecuteKernel );
	
	return ConstructorFunc;
	
	
	
	auto ObjectTemplateLocal = v8::ObjectTemplate::New( &Context.GetIsolate() );
	
	//v8::Local<v8::FunctionTemplate> LogFuncWrapper = v8::FunctionTemplate::New( Isolate, RawFunction );
	
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
	THROW_TODO;
	*/
}

void		JSClassRetain(JSClassRef Class)
{
	//	already retained
}


V8::TVirtualMachine::TVirtualMachine(const std::string& RuntimePath)
{
	//	gr: isolate crashes if runtime dir is wrong
	//	gr: FileExists currently works for OSX, maybe need an explicit func
	if ( !Platform::FileExists(RuntimePath) )
		throw Soy::AssertException( std::string("V8 Runtime path doesn't exist: ") + RuntimePath );

	//	well this is an annoying interface
	std::string Flags = "--expose_gc";
	//v8::internal::FLAG_expose_gc = true;
	v8::V8::SetFlagsFromString( Flags.c_str(), static_cast<int>(Flags.length()) );

	v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	
	
	
#if V8_VERSION==6
	std::string IcuPath = RuntimePath + "icudtl.dat";
	std::string NativesBlobPath = RuntimePath + "natives_blob.bin";
	std::string SnapshotBlobPath = RuntimePath + "snapshot_blob.bin";
	
	if ( !v8::V8::InitializeICUDefaultLocation( nullptr, IcuPath.c_str() ) )
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
	//v8::V8::InitializeExternalStartupData( mRootDirectory.c_str() );
	v8::V8::InitializeExternalStartupData( NativesBlobPath.c_str(), SnapshotBlobPath.c_str() );
	
#elif V8_VERSION==5
	V8::InitializeICU(nullptr);
	//v8::V8::InitializeExternalStartupData(argv[0]);
	//V8::InitializeExternalStartupData(nullptr);
	V8::InitializeExternalStartupData( Platform::GetExePath().c_str() );
#endif
	
	//	create allocator
	mAllocator.reset( new V8::TAllocator() );
	
	//std::unique_ptr<v8::Platform> platform = v8::platform::CreateDefaultPlatform();
	mPlatform.reset( v8::platform::CreateDefaultPlatform() );
	v8::V8::InitializePlatform( mPlatform.get() );
	v8::V8::Initialize();
	
	// Create a new Isolate and make it the current one.
	//	gr: current??
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = mAllocator.get();
	//create_params.snapshot_blob = &SnapshotBlobData;
	
	//	docs say "is owner" but there's no delete...
	mIsolate = v8::Isolate::New(create_params);
	
	//	we run the microtasks manually in our loop. This stops microtasks from occurring
	//	when we finish (end of stack) running when we call a js function arbritrarily
	mIsolate->SetMicrotasksPolicy( v8::MicrotasksPolicy::kExplicit );
};




void* V8::TAllocator::Allocate(size_t length)
{
	auto* Bytes = static_cast<uint8_t*>( AllocateUninitialized(length) );
	
	for ( auto i=0;	i<length;	i++ )
	Bytes[i] = 0;
	
	return Bytes;
}

void* V8::TAllocator::AllocateUninitialized(size_t length)
{
	return mHeap.AllocRaw(length);
}

void V8::TAllocator::Free(void* data, size_t length)
{
	mHeap.FreeRaw(data, length);
}



JSGlobalContextRef JSContextGroupRef::CreateContext()
{
	JSGlobalContextRef NewContext(nullptr);
	NewContext.mParent = this;
	std::function<void(v8::Isolate&)> Exec = [&](v8::Isolate& Isolate)
	{
		//	v8::Local<v8::Context>
		auto ContextLocal = v8::Context::New(&Isolate);
		//Context::Scope context_scope( ContextLocal );
		
		NewContext.mContext = V8::GetPersistent( Isolate, ContextLocal );
	};
	ExecuteInIsolate( Exec );
	return NewContext;
}


//	major abstraction from V8 to JSCore
//	JSCore has no global->local (maybe it should execute a run-next-in-queue func)
void JSLockAndRun(JSGlobalContextRef GlobalContext,std::function<void(JSContextRef&)> Functor)
{
	GlobalContext.ExecuteInContext( Functor );
}

void JSValueProtect(JSGlobalContextRef Context,JSValueRef Value)
{
	//	gr: deal with this later
	//	mght need an explicit TPersistent to store an object and not just inc/dec a ref count
}

void JSValueUnprotect(JSGlobalContextRef Context,JSValueRef Value)
{
	//	gr: deal with this later
	//	mght need an explicit TPersistent to store an object and not just inc/dec a ref count
}

V8::TVirtualMachine& JSGlobalContextRef::GetVirtualMachine()
{
	return mParent->GetVirtualMachine();
}


void V8::TVirtualMachine::ExecuteInIsolate(std::function<void(v8::Isolate&)> Functor)
{
	//	gr: we're supposed to lock the isolate here, but the setup we have,
	//	this should only ever be called on the JS thread[s] anyway
	//	maybe have a recursive mutex and throw if already locked
	v8::Locker locker(mIsolate);
	mIsolate->Enter();
	try
	{
		//  setup scope. handle scope always required to GC locals
		v8::Isolate::Scope isolate_scope(mIsolate);
		v8::HandleScope handle_scope(mIsolate);
		
		//	gr: auto catch and turn into a c++ exception
		{
			v8::TryCatch trycatch(mIsolate);
			Functor( *mIsolate );
			if ( trycatch.HasCaught() )
				throw Soy::AssertException("Some v8 exception");
				//throw V8Exception( trycatch, "Running Javascript func" );
		}
		mIsolate->Exit();
	}
	catch(...)
	{
		mIsolate->Exit();
		throw;
	}
}

	
void JSGlobalContextRef::ExecuteInContext(std::function<void(JSContextRef&)> Functor)
{
	std::function<void(v8::Isolate&)> Exec = [&](v8::Isolate& Isolate)
	{
		//	grab a local
		auto LocalContext = mContext->GetLocal(Isolate);
		JSContextRef LocalContextRef(LocalContext);
		v8::Context::Scope context_scope( LocalContext );
		Functor( LocalContextRef );
	};
	
	auto& vm = GetVirtualMachine();
	vm.ExecuteInIsolate( Exec );
}


JSContextRef::JSContextRef(v8::Local<v8::Context>& Local) :
	LocalRef	( Local )
{
}

v8::Isolate& JSContextRef::GetIsolate()
{
	auto* Isolate = this->mThis->GetIsolate();
	return *Isolate;
}

JSObjectRef::JSObjectRef(v8::Local<v8::Object>& Local) :
	LocalRef	( Local )
{
}

JSStringRef::JSStringRef(v8::Local<v8::String>& Local) :
	LocalRef	( Local )
{
}


