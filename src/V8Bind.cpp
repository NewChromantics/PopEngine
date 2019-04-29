#include "V8Bind.h"
#include "SoyDebug.h"


#define THROW_TODO	throw Soy::AssertException( __FUNCTION__ )



JSContextRef::JSContextRef(std::nullptr_t)
{
	THROW_TODO;
}
	
void JSContextRef::operator=(std::nullptr_t Null)
{
	THROW_TODO;
}

bool JSContextRef::operator!=(std::nullptr_t Null) const
{
	THROW_TODO;
}

JSContextRef::operator bool() const
{
	THROW_TODO;
}

JSContextGroupRef::JSContextGroupRef(std::nullptr_t)
{
	THROW_TODO;
}
	
JSContextGroupRef::operator bool() const
{
	THROW_TODO;
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

bool JSObjectRef::operator!=(std::nullptr_t Null) const
{
	THROW_TODO;
}

bool JSObjectRef::operator!=(const JSObjectRef& That) const
{
	THROW_TODO;
}

JSObjectRef::operator bool() const
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

bool JSValueRef::operator!=(std::nullptr_t Null) const
{
	THROW_TODO;
}

JSValueRef::operator bool() const
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

bool JSStringRef::operator!=(std::nullptr_t Null) const
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
	THROW_TODO;
}

void				JSContextGroupRelease(JSContextGroupRef ContextGroup)
{
	THROW_TODO;
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


