//	gr: can't exclude file per-configuration in xcode grr
#if defined(JSAPI_V8)
#include "V8Bind.h"
#include "SoyDebug.h"
#include "SoyFileSystem.h"

#include "libplatform/libplatform.h"
#include "include/v8.h"

#include "TBind.h"

#define THROW_TODO	throw Soy::AssertException( std::string("todo: ") + std::string(__FUNCTION__) )

const JSClassDefinition kJSClassDefinitionEmpty = {};


JSType JSValueGetType(JSValueRef Value);

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

JSContextGroupRef::JSContextGroupRef(std::nullptr_t)
{
	//	gr: don't throw. Just let this be in an invalid state for initialisation of variables
}

JSContextGroupRef::JSContextGroupRef(const std::string& RuntimePath)
{
	mVirtualMachine.reset( new V8::TVirtualMachine(RuntimePath));
}

	/*
void JSObjectRef::operator=(std::nullptr_t Null)
{
	this->mThis.Clear();
}

void JSObjectRef::operator=(JSObjectRef That)
{
	this->mThis = That.mThis;
}
*/

JSValueRef::JSValueRef(JSObjectRef Object) :
	TLocalRef	( ToValue(Object.mThis) )
{
}

JSValueRef::JSValueRef(v8::Local<v8::Value>& Local) :
	TLocalRef	( Local )
{
}

JSValueRef::JSValueRef(v8::Local<v8::Value>&& Local) :
	TLocalRef	( Local )
{
}

void JSValueRef::operator=(JSObjectRef That)
{
	this->mThis = That.mThis;
}

void JSValueRef::operator=(std::nullptr_t Null)
{
	this->mThis.Clear();
}


void JSStringRef::operator=(std::nullptr_t Null)
{
	this->mThis.Clear();
}






void JSObjectSetPrivate(JSObjectRef Object,void* Data)
{
	//	should already be null
	auto* PrevData = JSObjectGetPrivate( Object );
	if ( PrevData )
		throw Soy::AssertException("Private data non-null, expected to be unset");

	auto& Isolate = Object.GetIsolate();
	auto External = v8::External::New( &Isolate, Data );
	
	Object.mThis->SetInternalField( V8::InternalFieldDataIndex, External );
}

void* JSObjectGetPrivate(JSObjectRef Object)
{
	if ( !Object )
		return nullptr;
	
	auto Handle = Object.mThis->GetInternalField( V8::InternalFieldDataIndex );
	
	//	if undefined, we treat as not set (null)
	if ( Handle->IsUndefined() )
		return nullptr;
	
	//	but if it is set, should be an external
	auto Type = JSValueGetType( JSValueRef(Handle) );
	if ( !Handle->IsExternal() )
		throw Soy::AssertException("Private field of object is not an external");
	auto External = Handle.As<v8::External>();
	auto* VoidPtr = External->Value();

	return VoidPtr;
}

JSObjectRef	JSObjectMake(JSContextRef Context,JSClassRef Class,void* Data)
{
	if ( !Class )
	{
		if ( Data )
			throw Soy::AssertException("JSObjectMake without class, with data, excepting null data if no class");
		
		auto NewObject = v8::Object::New( &Context.GetIsolate() );
		return JSObjectRef( NewObject );
	}

	if ( !Class.mTemplate )
		throw Soy::AssertException("Expected template in class");

	//	if there is data, it's an instance, if not, we're probably setting up the constructor for the namespace
	//	that's how we use the logic in JavascriptCore anyway
	if ( !Data )
	{
		auto ConstructorTemplate = Class.mConstructor->GetLocal( Context.GetIsolate() );
		auto ConstructorMaybe = ConstructorTemplate->GetFunction( Context.mThis );
		V8::IsOkay( ConstructorMaybe, Context.GetIsolate(), Context.GetTryCatch(), "Getting function from constructor template");
		auto Constructor = ConstructorMaybe.ToLocalChecked();
		return JSObjectRef( Constructor );
	}
	
	//	create instance
	auto ObjectTemplate = Class.mTemplate->GetLocal( Context.GetIsolate() );
	auto NewObjectMaybe = ObjectTemplate->NewInstance( Context.mThis );
	V8::IsOkay( NewObjectMaybe, Context.GetIsolate(), Context.GetTryCatch(), "ObjectTemplate->NewInstance");
	auto NewObjectLocal = NewObjectMaybe.ToLocalChecked();
	
	JSObjectRef NewObjectRef( NewObjectLocal );

	//	auto assign private data like JsCore does
	JSObjectSetPrivate( NewObjectRef, Data );
	
	return NewObjectRef;
}

JSValueRef	JSObjectGetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef* Exception)
{
	auto ValueMaybe = This.mThis->Get( Context.mThis, Name.mThis );
	V8::IsOkay( ValueMaybe, Context.GetIsolate(), Context.GetTryCatch(), std::string("GetProperty ") + Name.GetString(Context) );
	
	auto Value = ValueMaybe.ToLocalChecked();
	return JSValueRef( Value );
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

JSType JSValueGetType(JSValueRef Value)
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
	TEST_IS( DataView, kJSTypeObject );

	TEST_IS( Array, kJSTypeObject );
	TEST_IS( ArrayBuffer, kJSTypeObject );
	TEST_IS( ArrayBufferView, kJSTypeObject );
	TEST_IS( TypedArray, kJSTypeObject );
	TEST_IS( Uint8Array, kJSTypeObject );
	TEST_IS( Uint8ClampedArray, kJSTypeObject );
	TEST_IS( Int8Array, kJSTypeObject );
	TEST_IS( Uint16Array, kJSTypeObject );
	TEST_IS( Int16Array, kJSTypeObject );
	TEST_IS( Uint32Array, kJSTypeObject );
	TEST_IS( Int32Array, kJSTypeObject );
	TEST_IS( Float32Array, kJSTypeObject );
	TEST_IS( Float64Array, kJSTypeObject );
	TEST_IS( BigInt64Array, kJSTypeObject );
	TEST_IS( BigUint64Array, kJSTypeObject );
	TEST_IS( SharedArrayBuffer, kJSTypeObject );
	
	TEST_IS( Boolean, kJSTypeBoolean );
	
	TEST_IS( Number, kJSTypeNumber );
	TEST_IS( Int32, kJSTypeNumber );
	TEST_IS( Uint32, kJSTypeNumber );
	
	throw Soy::AssertException("v8 value didn't match any type");
}


JSType JSValueGetType(JSContextRef Context,JSValueRef Value)
{
	return JSValueGetType( Value );
}


bool JSValueIsObject(JSContextRef Context,JSValueRef Value)
{
	return Value.mThis->IsObject();
}

bool JSValueIsObject(JSContextRef Context,JSObjectRef Value)
{
	if ( !Value )
		return false;
	return Value.mThis->IsObject();
}

JSObjectRef JSValueToObject(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	if ( !Value )
		throw Soy::AssertException("Value is nullptr, not object");
	
	//	gr: for JavascriptCore compatibility, we consider multiple things as objects
	auto Type = JSValueGetType( Context, Value );
	if ( Type != kJSTypeObject )
		throw Soy::AssertException("Value is not an object");
	
	auto ObjectLocal = Value.mThis.As<v8::Object>();
	return JSObjectRef( ObjectLocal );
}

void		JSValueProtect(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}

void		JSValueUnprotect(JSContextRef Context,JSValueRef Value)
{
	THROW_TODO;
}


JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef Context,JSObjectRef This)
{
	if ( !This )
		throw Soy::AssertException("JSObjectCopyPropertyNames on null");

	//	gr: use OwnPropertyNames?
	auto PropertyNamesMaybe = This.mThis->GetPropertyNames(Context.mThis);
	V8::IsOkay(PropertyNamesMaybe, Context.GetIsolate(), Context.GetTryCatch(), __FUNCTION__);
	auto PropertyNames = PropertyNamesMaybe.ToLocalChecked();

	JSPropertyNameArrayRef Names;
	Names.mIsolate = &Context.GetIsolate();
	for ( auto i=0;	i<PropertyNames->Length();	i++ )
	{
		auto Element = PropertyNames->Get(i);
		auto StringRef = JSValueToStringCopy(Context, Element);
		auto String = StringRef.GetString(Context);
		Names.mNames.PushBack(String);
	}

	return Names;
}

size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef Keys)
{
	return Keys.mNames.GetSize();
}

JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef Keys,size_t Index)
{
	if ( Index >= Keys.mNames.GetSize() )
	{
		std::stringstream Error;
		Error << "JSPropertyNameArrayGetNameAtIndex " << Index << "/" << Keys.mNames.GetSize() << " out of bounds";
		throw Soy::AssertException(Error);
	}

	auto& Name = Keys.mNames[Index];
	return JSStringRef( *Keys.mIsolate, Name );
}


bool JSValueIsNumber(JSContextRef Context,JSValueRef Value)
{
	return Value.mThis->IsNumber();
}

double JSValueToNumber(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	//	reinterpret as number
	auto NumberHandle = Value.mThis->ToNumber(Context.mThis);
	V8::IsOkay(NumberHandle, Context.GetIsolate(), Context.GetTryCatch(), "JSValueToNumber");
	auto Number = NumberHandle.ToLocalChecked();
	return Number->Value();
}

JSValueRef JSValueMakeNumber(JSContextRef Context,double Value)
{
	auto Number = v8::Number::New( &Context.GetIsolate(), Value );
	return JSValueRef( Number );
}


bool JSObjectIsFunction(JSContextRef Context,JSObjectRef Value)
{
	return Value.mThis->IsFunction();
}

JSValueRef JSObjectCallAsFunction(JSContextRef Context,JSObjectRef Object,JSObjectRef This,size_t ArgumentCount,JSValueRef* Arguments,JSValueRef* Exception)
{
	//	cast object to function & call
	if ( !Object )
		throw Soy::AssertException("Trying to call object/function which is null");
	
	auto Function = Object.mThis.As<v8::Function>();

	Array<v8::Local<v8::Value>> ArgumentValues;
	for ( auto a=0;	a<ArgumentCount;	a++ )
		ArgumentValues.PushBack( Arguments[a].mThis );
	
	//	v8 needs a non-null for this. JSCore accepts null
	if ( !This )
		This = JSContextGetGlobalObject(Context);
	
	auto ResultMaybe = Function->Call( Context.mThis, This.mThis, ArgumentValues.GetSize(), ArgumentValues.GetArray() );
	V8::IsOkay( ResultMaybe, Context.GetIsolate(), Context.GetTryCatch(), "JSObjectCallAsFunction" );
	
	auto Result = ResultMaybe.ToLocalChecked();
	return JSValueRef( Result );
}

JSValueRef JSObjectMakeFunctionWithCallback(JSContextRef Context,JSStringRef Name,JSObjectCallAsFunctionCallback FunctionPtr)
{
	auto FunctionMaybe = v8::Function::New( Context.mThis, FunctionPtr );
	if ( FunctionMaybe.IsEmpty() )
		throw Soy::AssertException("Failed to create function");
	
	auto FunctionHandle = FunctionMaybe.ToLocalChecked();
	auto FunctionValue = ToValue( FunctionHandle );
	return JSValueRef( FunctionValue );
}


bool JSValueToBoolean(JSContextRef Context,JSValueRef Value)
{
	//	reinterpret as boolean
	auto BooleanHandle = Value.mThis->ToBoolean(&Context.GetIsolate());
	return BooleanHandle->Value();
}

JSValueRef JSValueMakeBoolean(JSContextRef Context,bool Value)
{
	auto Boolean = v8::Boolean::New( &Context.GetIsolate(), Value );
	return JSValueRef( Boolean );
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
	auto ArrayHandle = v8::Array::New( &Context.GetIsolate() );
	for ( auto i=0;	i<ElementCount;	i++ )
	{
		auto ValueHandle = Elements[i];
		ArrayHandle->Set( i, ValueHandle.mThis );
	}
	
	return JSObjectRef( ArrayHandle );
}

bool JSValueIsArray(JSContextRef Context,JSValueRef Value)
{
	if ( !Value )
		return false;
	
	if ( Value.mThis->IsArray() )	return true;
	if ( Value.mThis->IsArrayBuffer() )	return true;
	if ( Value.mThis->IsArrayBufferView() )	return true;
	if ( Value.mThis->IsTypedArray() )	return true;
	if ( Value.mThis->IsUint8Array() )	return true;
	if ( Value.mThis->IsUint8ClampedArray() )	return true;
	if ( Value.mThis->IsInt8Array() )	return true;
	if ( Value.mThis->IsUint16Array() )	return true;
	if ( Value.mThis->IsInt16Array() )	return true;
	if ( Value.mThis->IsUint32Array() )	return true;
	if ( Value.mThis->IsInt32Array() )	return true;
	if ( Value.mThis->IsFloat32Array() )	return true;
	if ( Value.mThis->IsFloat64Array() )	return true;
	if ( Value.mThis->IsBigInt64Array() )	return true;
	if ( Value.mThis->IsBigUint64Array() )	return true;
	if ( Value.mThis->IsSharedArrayBuffer() )	return true;
	
	return false;
}

JSTypedArrayType JSValueGetTypedArrayType(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	if ( Value.mThis->IsUint8Array() )	return kJSTypedArrayTypeUint8Array;
	if ( Value.mThis->IsUint8ClampedArray() )	return kJSTypedArrayTypeUint8ClampedArray;
	if ( Value.mThis->IsInt8Array() )	return kJSTypedArrayTypeInt8Array;
	if ( Value.mThis->IsUint16Array() )	return kJSTypedArrayTypeUint16Array;
	if ( Value.mThis->IsInt16Array() )	return kJSTypedArrayTypeInt16Array;
	if ( Value.mThis->IsUint32Array() )	return kJSTypedArrayTypeUint32Array;
	if ( Value.mThis->IsInt32Array() )	return kJSTypedArrayTypeInt32Array;
	if ( Value.mThis->IsFloat32Array() )	return kJSTypedArrayTypeFloat32Array;
	
	if ( Value.mThis->IsFloat64Array() )
		throw Soy::AssertException("Currently not supporting Float64 array (missing from JavaScriptCore)");
	if ( Value.mThis->IsBigInt64Array() )
		throw Soy::AssertException("Currently not supporting BigInt64 array (missing from JavaScriptCore)");
	if ( Value.mThis->IsBigUint64Array() )
		throw Soy::AssertException("Currently not supporting BigUint64 array (missing from JavaScriptCore)");

	
	return kJSTypedArrayTypeNone;
}

template<typename ArrayType,typename RealType>
JSObjectRef MakeTypedArrayView(JSContextRef Context,v8::Local<v8::ArrayBuffer>& ArrayBuffer)
{
	auto ByteOffset = 0;
	auto Length = ArrayBuffer->ByteLength() / sizeof(RealType);
	auto Array = ArrayType::New(ArrayBuffer, ByteOffset, Length);
	auto Object = Array.template As<v8::Object>();
	JSObjectRef ArrayObject(Object);
	return ArrayObject;
}



JSObjectRef	JSObjectMakeTypedArrayWithBytesWithCopy(JSContextRef Context,JSTypedArrayType ArrayType,const uint8_t* ExternalBuffer,size_t ExternalBufferSize,JSValueRef* Exception)
{
	//	gr: for V8, we need to make a pool of auto releasing objects, or an objectwrapper that manages the array or something...
	//		but for now, we'll just copy the bytes.
	auto ExternalBufferArray = GetRemoteArray( const_cast<uint8_t*>( ExternalBuffer ), ExternalBufferSize );

	//	make an array buffer
	auto ArrayBuffer = v8::ArrayBuffer::New( &Context.GetIsolate(), ExternalBufferSize );
	auto ArrayBufferContents = ArrayBuffer->GetContents();
	auto ArrayBufferArray = GetRemoteArray( static_cast<uint8_t*>( ArrayBufferContents.Data() ), ArrayBufferContents.ByteLength() );

	ArrayBufferArray.Copy( ExternalBufferArray );

	JSObjectRef ArrayObject(nullptr);

	//	make view of that array buffer
	switch (ArrayType)
	{
		case kJSTypedArrayTypeInt8Array:			return MakeTypedArrayView<v8::Int8Array,int8_t>(Context, ArrayBuffer);
		case kJSTypedArrayTypeInt16Array:			return MakeTypedArrayView<v8::Int16Array,int16_t>(Context, ArrayBuffer);
		case kJSTypedArrayTypeInt32Array:			return MakeTypedArrayView<v8::Int32Array,int32_t>(Context, ArrayBuffer);
		case kJSTypedArrayTypeUint8Array:			return MakeTypedArrayView<v8::Uint8Array,uint8_t>(Context, ArrayBuffer);
		case kJSTypedArrayTypeUint8ClampedArray:	return MakeTypedArrayView<v8::Uint8ClampedArray,uint8_t>(Context, ArrayBuffer);
		case kJSTypedArrayTypeUint16Array:			return MakeTypedArrayView<v8::Uint16Array,uint16_t>(Context, ArrayBuffer);
		case kJSTypedArrayTypeUint32Array:			return MakeTypedArrayView<v8::Uint32Array,uint32_t>(Context, ArrayBuffer);
		case kJSTypedArrayTypeFloat32Array:			return MakeTypedArrayView<v8::Float32Array,float>(Context, ArrayBuffer);
		default:break;
	}

	std::stringstream Error;
	Error << "Unhandled ArrayType " << ArrayType << " in " << __FUNCTION__;
	throw Soy::AssertException(Error);
}

JSObjectRef	JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef Context, JSTypedArrayType ArrayType, void* ExternalBuffer, size_t ExternalBufferSize, JSTypedArrayBytesDeallocator Dealloc, void* DeallocContext, JSValueRef* Exception)
{
	throw Soy::AssertException("v8 cannot use JSObjectMakeTypedArrayWithBytesNoCopy as we dont do the dealloc");
}

v8::Local<v8::TypedArray> GetTypedArray(JSObjectRef& ArrayObject)
{
	auto Array = ArrayObject.mThis.As<v8::TypedArray>();
	if ( Array.IsEmpty() )
		throw Soy::AssertException("JSObjectGetTypedArrayBytesPtr on object that isn't a typed array");

	return Array;
}

void* JSObjectGetTypedArrayBytesPtr(JSContextRef Context,JSObjectRef ArrayObject,JSValueRef* Exception)
{
	//	hack, but we don't get a pointer
	//	the contents should only be used in a small scope, but we can't really control that
	//	and REALLY it'll only be on one thread, but we can make sure of that with ThreadLocalStorage
	//	obviously a big penalty for calling this more than once
	__thread static Array<uint8_t>* pBytesCopy = nullptr;
	if ( !pBytesCopy )
		pBytesCopy = new Array<uint8_t>();
	auto& BytesCopy = *pBytesCopy;

	auto TypedArray = GetTypedArray(ArrayObject);
	auto TypedArraySize = TypedArray->ByteLength();
	BytesCopy.SetSize(TypedArraySize);
	auto BytesWritten = TypedArray->CopyContents(BytesCopy.GetArray(), BytesCopy.GetDataSize());
	if ( BytesWritten != TypedArraySize )
	{
		std::stringstream Error;
		Error << "Typed array extracted " << BytesWritten << "/" << TypedArraySize << " bytes";
		throw Soy::AssertException(Error);
	}

	return BytesCopy.GetArray();
}

size_t JSObjectGetTypedArrayByteOffset(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	//	gr: because in JSObjectGetTypedArrayBytesPtr we always copy out the contents
	//		the byte offset will always be zero to the caller as its relative to that
	return 0;
}

size_t JSObjectGetTypedArrayLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	auto TypedArray = GetTypedArray(Array);
	return TypedArray->Length();
}

size_t JSObjectGetTypedArrayByteLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception)
{
	auto TypedArray = GetTypedArray(Array);
	return TypedArray->ByteLength();
}


JSValueRef JSEvaluateScript(JSContextRef Context,JSStringRef Source,JSObjectRef This,JSStringRef Filename,int LineNumber,JSValueRef* Exception)
{
	auto* Isolate = &Context.GetIsolate();
	//	compile into script
	std::string UrlFilename = std::string("file://") + Bind::GetString( Context, Filename );
	JSStringRef OriginStr( Context, UrlFilename );
	auto OriginRow = v8::Integer::New( Isolate, 0 );
	auto OriginCol = v8::Integer::New( Isolate, 0 );
	auto Cors = v8::Boolean::New( Isolate, true );
	v8::ScriptOrigin Origin( OriginStr.mThis, OriginRow, OriginCol, Cors );

	auto NewScriptReturn = v8::Script::Compile( Context.mThis, Source.mThis, &Origin );
	V8::IsOkay( NewScriptReturn, Context.GetIsolate(), Context.GetTryCatch(), "Script failed to compile");
	auto NewScript = NewScriptReturn.ToLocalChecked();
	
	//	now run it
	auto ResultMaybe = NewScript->Run( Context.mThis );
	V8::IsOkay( ResultMaybe, Context.GetIsolate(), Context.GetTryCatch(), "Script failed to run");
	auto ResultValue = ResultMaybe.ToLocalChecked();
	return JSValueRef( ResultValue );
}

JSGlobalContextRef JSContextGetGlobalContext(JSContextRef Context)
{
	auto& ContextInstance = Context.GetContext();
	return ContextInstance.mContext;
}

JSObjectRef JSContextGetGlobalObject(JSContextRef Context)
{
	auto Global = Context.mThis->Global();
	return JSObjectRef( Global );
}

JSContextGroupRef JSContextGroupCreate()
{
	throw Soy::AssertException("In v8 implementation we need the runtime directory, use overloaded version");
}

JSContextGroupRef JSContextGroupCreateWithRuntime(const std::string& RuntimeDirectory)
{
	JSContextGroupRef NewVirtualMachine( RuntimeDirectory );
	return NewVirtualMachine;
}

void JSContextGroupRelease(JSContextGroupRef ContextGroup)
{
	//	try and release all members here and maybe check for dangling refcounts
}

JSGlobalContextRef JSGlobalContextCreateInGroup(JSContextGroupRef ContextGroup,JSClassRef GlobalClass)
{
	JSGlobalContextRef NewContext(nullptr);
	ContextGroup.CreateContext(NewContext);
	return NewContext;
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

void JSGarbageCollect(JSContextRef Context)
{
	Context.GetIsolate().RequestGarbageCollectionForTesting( v8::Isolate::kFullGarbageCollection );
}


JSStringRef	JSStringCreateWithUTF8CString(JSContextRef Context,const char* Buffer)
{
	return JSStringRef( Context, Buffer );
}

size_t JSStringGetUTF8CString(JSContextRef Context,JSStringRef String,char* Buffer,size_t BufferSize)
{
	if ( BufferSize == 0 )
		return 0;
	
#if V8_VERSION==7
	v8::String::Utf8Value ExceptionStr( &Context.GetIsolate(), String.mThis );
#else
	v8::String::Utf8Value ExceptionStr( String.mThis );
#endif

	//	+1 to add terminator
	auto Length = ExceptionStr.length()+1;
	const auto* Chars = *ExceptionStr;

	if ( Length == 0 )
		return 0;
	if ( Length < 0 )
		throw Soy::AssertException("String has negative length");
	
	Length = std::min<int>( Length-1, BufferSize-1 );
	
	for ( auto i=0;	i<Length;	i++ )
	{
		Buffer[i] = Chars[i];
	}
	Buffer[Length] = 0;
	return Length+1;
}

size_t JSStringGetLength(JSStringRef String)
{
	if ( !String )
		return 0;
	return String.mThis->Length();
}

JSStringRef	JSValueToStringCopy(JSContextRef Context,JSValueRef Value,JSValueRef* Exception)
{
	//	this function gets a value in a string representation
	/*
	if ( !Value.mThis->IsString() )
		throw Soy::AssertException("Value is not string");
	*/
	//	ToString converts, As is a cast
	auto ValueStringMaybe = Value.mThis->ToString(Context.mThis);
	V8::IsOkay(ValueStringMaybe, Context.GetIsolate(), Context.GetTryCatch(), "JSValueToStringCopy");
	auto ValueString = ValueStringMaybe.ToLocalChecked();
	JSStringRef ValueStringRef( ValueString );
	auto NewString = Bind::GetString( Context, ValueStringRef );
	JSStringRef NewStringRef( Context, NewString );
	return NewStringRef;
}

JSValueRef JSValueMakeString(JSContextRef Context,JSStringRef String)
{
	//	normally copies string
	auto Value = ToValue( String.mThis );
	return JSValueRef( Value );
}

void JSStringRelease(JSStringRef String)
{
	//	can just let this go out of scope for now
}


void Constructor(const v8::FunctionCallbackInfo<v8::Value>& Meta)
{
	std::Debug << "Constructor" << std::endl;
};


JSClassRef JSClassCreate(JSContextRef Context,JSClassDefinition& Definition)
{
	auto* Isolate = &Context.GetIsolate();

	//	make constructor
	//auto* Pointer = nullptr;
	//auto PointerHandle = External::New( Isolate, Pointer ).As<Value>();
	//auto ConstructorFunc = v8::FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	auto ConstructorFunc = v8::FunctionTemplate::New( Isolate, Definition.callAsConstructor );

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
			auto& FunctionDefinition = Definition.staticFunctions[i];
			i++;
			if ( FunctionDefinition.name == nullptr )
				break;
			
			//	bind function to template
			auto This = InstanceTemplate;
			v8::Local<v8::FunctionTemplate> FunctionTemplateLocal = v8::FunctionTemplate::New( Isolate, FunctionDefinition.callAsFunction );
			auto FunctionLocal = FunctionTemplateLocal->GetFunction( Context.mThis );
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
	auto Constructor = V8::GetPersistent( *Isolate, ConstructorFunc );
	
	JSClassRef NewClass( nullptr );
	NewClass.mTemplate = Template;
	NewClass.mConstructor = Constructor;
	NewClass.mDestructor = Definition.finalize;
	
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
	
	
	
#if (V8_VERSION==6) || (V8_VERSION==7)
	std::string IcuPath = RuntimePath + "icudtl.dat";
	std::string NativesBlobPath = RuntimePath + "natives_blob.bin";
#if defined(TARGET_WINDOWS)
	std::string SnapshotBlobPath = RuntimePath + "snapshot_blob_win64.bin";
#else
		std::string SnapshotBlobPath = RuntimePath + "snapshot_blob_osx.bin";
#endif

	if ( !v8::V8::InitializeICU( IcuPath.c_str() ) )
		throw Soy::AssertException("Failed to load ICU");

	Array<char> NativesBlob;
	Array<char> SnapshotBlob;
	v8::StartupData NativesBlobData;
	v8::StartupData SnapshotBlobData;
	Soy::FileToArray( GetArrayBridge(NativesBlob), NativesBlobPath );
	Soy::FileToArray( GetArrayBridge(SnapshotBlob), SnapshotBlobPath );

	NativesBlobData={	NativesBlob.GetArray(), static_cast<int>(NativesBlob.GetDataSize())	};
	SnapshotBlobData={	SnapshotBlob.GetArray(), static_cast<int>(SnapshotBlob.GetDataSize())	};
	//v8::V8::SetNativesDataBlob(&NativesBlobData);
	//v8::V8::SetSnapshotDataBlob(&SnapshotBlobData);
	v8::V8::InitializeExternalStartupData( NativesBlobPath.c_str(), SnapshotBlobPath.c_str() );
	
#elif V8_VERSION==5
	V8::InitializeICU(nullptr);
	//v8::V8::InitializeExternalStartupData(argv[0]);
	//V8::InitializeExternalStartupData(nullptr);
	V8::InitializeExternalStartupData( Platform::GetExePath().c_str() );
#endif
	
	//	create allocator
	mAllocator.reset( new V8::TAllocator() );
	
#if (V8_VERSION==7)
	mPlatform = v8::platform::NewDefaultPlatform();
#else
	//std::unique_ptr<v8::Platform> platform = v8::platform::CreateDefaultPlatform();
	mPlatform.reset( v8::platform::CreateDefaultPlatform() );
#endif
	v8::V8::InitializePlatform( mPlatform.get() );
	v8::V8::Initialize();
	
	// Create a new Isolate and make it the current one.
	//	gr: current??
	static v8::Isolate::CreateParams create_params;
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



void JSContextGroupRef::CreateContext(JSGlobalContextRef& NewContext)
{
	//JSGlobalContextRef NewContext(nullptr);
	NewContext.mParent = *this;
	std::function<void(v8::Isolate&)> Exec = [&](v8::Isolate& Isolate)
	{
		auto ContextLocal = v8::Context::New(&Isolate);
		v8::Context::Scope context_scope( ContextLocal );

		NewContext.mContext = V8::GetPersistent( Isolate, ContextLocal );
	};
	auto& vm = GetVirtualMachine();
	vm.ExecuteInIsolate( Exec );
	//return NewContext;
}


//	major abstraction from V8 to JSCore
//	JSCore has no global->local (maybe it should execute a run-next-in-queue func)
void JSLockAndRun(JSGlobalContextRef GlobalContext,std::function<void(JSContextRef&)> Functor)
{
	GlobalContext.ExecuteInContext( Functor );
}

void JSValueProtect(JSGlobalContextRef Context,JSValueRef Value)
{
	THROW_TODO;
	//	gr: deal with this later
	//	mght need an explicit TPersistent to store an object and not just inc/dec a ref count
}

void JSValueUnprotect(JSGlobalContextRef Context,JSValueRef Value)
{
	THROW_TODO;
	//	gr: deal with this later
	//	mght need an explicit TPersistent to store an object and not just inc/dec a ref count
}

V8::TVirtualMachine& JSGlobalContextRef::GetVirtualMachine()
{
	return mParent.GetVirtualMachine();
}

bool V8::TVirtualMachine::ProcessJobQueue(std::function<void(std::chrono::milliseconds)>& Sleep)
{
	//	gr: if no jobs, force a sleep here?

	bool MoreTasks = true;
	auto PumpMessageLoop = [&](v8::Isolate& Isolate)
	{
		auto Blocking = false;
		auto Behavior = Blocking ? v8::platform::MessageLoopBehavior::kWaitForWork : v8::platform::MessageLoopBehavior::kDoNotWait;
		//	this returns true if a task was processed
		MoreTasks = v8::platform::PumpMessageLoop( mPlatform.get(), &Isolate, Behavior );
	};

	while( MoreTasks )
	{
		ExecuteInIsolate(PumpMessageLoop);
	}
	
	auto RunMicroTasks = [&](v8::Isolate& Isolate)
	{
		Isolate.RunMicrotasks();
	};
	ExecuteInIsolate(RunMicroTasks);
	
	return true;
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
			v8::TryCatch TryCatch(mIsolate);
			Functor( *mIsolate );
			if ( TryCatch.HasCaught() )
				throw V8::TException( *mIsolate, TryCatch, "Some v8 exception");
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
		v8::TryCatch TryCatch( &Isolate );
		
		//	grab a local
		auto LocalContext = mContext->GetLocal(Isolate);
		JSContextRef LocalContextRef(LocalContext);
		LocalContextRef.mTryCatch = &TryCatch;
		
		v8::Context::Scope context_scope( LocalContext );
		Functor( LocalContextRef );
		
		if ( TryCatch.HasCaught() )
			throw V8::TException( Isolate, TryCatch, "Executing Context");
	};
	
	auto& vm = GetVirtualMachine();
	vm.ExecuteInIsolate( Exec );
}


JSContextRef::JSContextRef(v8::Local<v8::Context>& Local) :
	TLocalRef	( Local )
{
}

JSContextRef::JSContextRef(v8::Local<v8::Context>&& Local) :
	TLocalRef	( Local )
{
}

v8::Isolate& JSContextRef::GetIsolate()
{
	auto* Isolate = this->mThis->GetIsolate();
	return *Isolate;
}

JsCore::TContext& JSContextRef::GetContext()
{
	auto* pContextVoid = mThis->GetAlignedPointerFromEmbedderData(0);
	if ( !pContextVoid )
		throw Soy::AssertException("Aligned data not set");
	auto* pContext = reinterpret_cast<JsCore::TContext*>(pContextVoid);
	return *pContext;
}

void JSContextRef::SetContext(JsCore::TContext& Context)
{
	mThis->SetAlignedPointerInEmbedderData(0, &Context);
}

v8::TryCatch& JSContextRef::GetTryCatch()
{
	return *mTryCatch;
}

JSObjectRef::JSObjectRef(v8::Local<v8::Object>& Local) :
	TLocalRef	( Local )
{
}

JSObjectRef::JSObjectRef(v8::Local<v8::Object>&& Local) :
	TLocalRef	( Local )
{
}

JSStringRef::JSStringRef(v8::Local<v8::String>& Local) :
	TLocalRef	( Local )
{
}

JSStringRef::JSStringRef(JSContextRef Context,const std::string& String) :
	JSStringRef	( Context.GetIsolate(), String )
{
}

JSStringRef::JSStringRef(v8::Isolate& Isolate,const std::string& String)
{
	auto Handle = v8::String::NewFromUtf8( &Isolate, String.c_str() );
	mThis = Handle;
}

std::string JSStringRef::GetString(JSContextRef Context)
{
	auto Str = Bind::GetString( Context, mThis );
	return Str;
}


V8::TException::TException(v8::Isolate& Isolate,v8::TryCatch& TryCatch,const std::string& Context) :
	mError	( Context )
{
	//	get the exception from v8
	auto Exception = TryCatch.Exception();
	
	if ( Exception.IsEmpty() )
	{
		mError += "<Empty Exception>";
		return;
	}
	
	//	get the description
	v8::String::Utf8Value ExceptionStr( &Isolate, Exception );
	auto ExceptionCStr = *ExceptionStr;
	if ( ExceptionCStr == nullptr )
	{
		mError += ": <null> possibly not an exception";
	}
	else
	{
		mError += ": ";
		mError += ExceptionCStr;
	}
	
	//	get stack trace
	auto StackTrace = v8::Exception::GetStackTrace( Exception );
	if ( StackTrace.IsEmpty() )
	{
		mError += "\n<missing stacktrace>";
	}
	else
	{
		for ( int fi=0;	fi<StackTrace->GetFrameCount();	fi++ )
		{
			auto Frame = StackTrace->GetFrame( &Isolate, fi );
			v8::String::Utf8Value FuncName( &Isolate, Frame->GetFunctionName() );
			mError += "\n";
			mError += "in ";
			mError += *FuncName;
		}
	}
	
}

JSValueRef JSValueMakeFromJSONString(JSContextRef Context, JSStringRef String)
{
	auto LocalMaybe = v8::JSON::Parse(Context.mThis, String.mThis);
	V8::IsOkay(LocalMaybe, Context.GetIsolate(), Context.GetTryCatch(), "JSValueMakeFromJSONString");
	auto Local = LocalMaybe.ToLocalChecked();
	return JSValueRef(Local);
}


JSValueRef JSObjectToValue(JSObjectRef Object)
{
	auto Value = Object.GetValue();
	return JSValueRef( Value );
}

#endif //JSAPI_V8
