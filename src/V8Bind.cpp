#include "V8Bind.h"
#include "SoyDebug.h"

#include "libplatform/libplatform.h"
#include "include/v8.h"

#include "SoyFileSystem.h"

#define THROW_TODO	throw Soy::AssertException( __FUNCTION__ )


JSContextGroupRef::JSContextGroupRef(std::nullptr_t) :
	V8::TVirtualMachine	(nullptr)
{
	//	gr: don't throw. Just let this be in an invalid state for initialisation of variables
}


JSObjectRef::JSObjectRef(std::nullptr_t)
{
	THROW_TODO;
}
	
void JSObjectRef::operator=(std::nullptr_t Null)
{
	THROW_TODO;
}

void JSObjectRef::operator=(JSObjectRef That)
{
	THROW_TODO;
}

JSValueRef::JSValueRef()
{
	THROW_TODO;
}

JSValueRef::JSValueRef(JSObjectRef Object)
{
	THROW_TODO;
}

JSValueRef::JSValueRef(std::nullptr_t)
{
	THROW_TODO;
}
	
void JSValueRef::operator=(JSObjectRef That)
{
	THROW_TODO;
}

void JSValueRef::operator=(std::nullptr_t Null)
{
	THROW_TODO;
}


JSStringRef::JSStringRef(std::nullptr_t)
{
	THROW_TODO;
}
	
void JSStringRef::operator=(std::nullptr_t Null)
{
	THROW_TODO;
}


JSClassRef::JSClassRef(std::nullptr_t)
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
	THROW_TODO;
}

JSValueRef	JSObjectGetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef* Exception)
{
	THROW_TODO;
}

void		JSObjectSetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception )
{
	THROW_TODO;
}

void		JSObjectSetPropertyAtIndex(JSContextRef Context,JSObjectRef This,size_t Index,JSValueRef Value,JSValueRef* Exception)
{
	THROW_TODO;
}


JSType		JSValueGetType(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

bool		JSValueIsObject(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
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
	THROW_TODO;
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
	THROW_TODO;
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
	THROW_TODO;
}

JSValueRef	JSValueMakeBoolean(JSContextRef Context,bool Value)
{
	THROW_TODO;
}


JSValueRef	JSValueMakeUndefined(JSContextRef Context)
{
	THROW_TODO;
}

bool		JSValueIsUndefined(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}


JSValueRef	JSValueMakeNull(JSContextRef Context)
{
	THROW_TODO;
}

bool		JSValueIsNull(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
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
	THROW_TODO;
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

JSContextRef		JSGlobalContextCreateInGroup(JSContextGroupRef ContextGroup,JSClassRef GlobalClass)
{
	THROW_TODO;
}

void				JSGlobalContextSetName(JSContextRef Context,JSStringRef Name)
{
	THROW_TODO;
}

void				JSGlobalContextRelease(JSContextRef Context)
{
	THROW_TODO;
}

void				JSGarbageCollect(JSContextRef Context)
{
	THROW_TODO;
}


JSStringRef	JSStringCreateWithUTF8CString(const char* Buffer)
{
	THROW_TODO;
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
	THROW_TODO;
}


JSClassRef	JSClassCreate(JSClassDefinition* Definition)
{
	THROW_TODO;
}

void		JSClassRetain(JSClassRef Class)
{
	THROW_TODO;
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
