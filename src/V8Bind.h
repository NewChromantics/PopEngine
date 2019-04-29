#pragma once

#include <cstddef>

class JSContextRef
{
public:
	JSContextRef(std::nullptr_t);
	
	void	operator=(std::nullptr_t Null);
	bool	operator!=(std::nullptr_t Null) const;
	operator bool() const;
};
typedef JSContextRef JSGlobalContextRef;

class JSContextGroupRef
{
public:
	JSContextGroupRef(std::nullptr_t);

	operator bool() const;
};

class JSObjectRef
{
public:
	JSObjectRef(std::nullptr_t);
	
	void	operator=(std::nullptr_t Null);
	void	operator=(JSObjectRef That);
	bool	operator!=(std::nullptr_t Null) const;
	bool	operator!=(const JSObjectRef& That) const;
	operator bool() const;
};

class JSValueRef
{
public:
	JSValueRef();
	JSValueRef(JSObjectRef Object);
	JSValueRef(std::nullptr_t);
	
	void	operator=(JSObjectRef That);
	void	operator=(std::nullptr_t Null);
	bool	operator!=(std::nullptr_t Null) const;
	operator bool() const;
};

class JSStringRef
{
public:
	JSStringRef(std::nullptr_t);
	
	void	operator=(std::nullptr_t Null);
	bool	operator!=(std::nullptr_t Null) const;
};


class JSClassRef
{
public:
	JSClassRef(std::nullptr_t);
};


enum JSType
{
	kJSTypeString,
	kJSTypeBoolean,
	kJSTypeUndefined,
	kJSTypeNull,
	kJSTypeObject,
};

enum JSClassAttributes
{
	kJSClassAttributeNone
};

enum JSTypedArrayType
{
	kJSTypedArrayTypeNone,
	kJSTypedArrayTypeInt8Array,
	kJSTypedArrayTypeInt16Array,
	kJSTypedArrayTypeInt32Array,
	kJSTypedArrayTypeUint8Array,
	kJSTypedArrayTypeUint8ClampedArray,
	kJSTypedArrayTypeUint16Array,
	kJSTypedArrayTypeUint32Array,
	kJSTypedArrayTypeFloat32Array,
};

typedef void(*JSTypedArrayBytesDeallocator)(void* bytes, void* deallocatorContext);
typedef JSObjectRef(*JSObjectCallAsConstructorCallback) (JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
typedef JSValueRef(*JSObjectCallAsFunctionCallback) (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
typedef void(*JSObjectFinalizeCallback) (JSObjectRef object);


enum JSPropertyAttributes
{
	kJSPropertyAttributeNone
};


typedef struct {
	const char* name;
	JSObjectCallAsFunctionCallback callAsFunction;
	JSPropertyAttributes attributes;
} JSStaticFunction;


class JSClassDefinition
{
public:
	const char*			className = nullptr;	//	emulate JScore instability with raw pointers
	JSClassAttributes	attributes;
	JSObjectCallAsConstructorCallback	callAsConstructor = nullptr;
	JSObjectFinalizeCallback			finalize = nullptr;
	JSStaticFunction*		staticFunctions = nullptr;
};
extern const JSClassDefinition kJSClassDefinitionEmpty;



class JSPropertyNameArrayRef
{
	
};





void		JSObjectSetPrivate(JSObjectRef Object,void* Data);
void*		JSObjectGetPrivate(JSObjectRef Object);
JSObjectRef	JSObjectMake(JSContextRef Context,JSClassRef Class,void*);
JSValueRef	JSObjectGetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef* Exception);
void		JSObjectSetProperty(JSContextRef Context,JSObjectRef This,JSStringRef Name,JSValueRef Value,JSPropertyAttributes Attribs,JSValueRef* Exception );
void		JSObjectSetPropertyAtIndex(JSContextRef Context,JSObjectRef This,size_t Index,JSValueRef Value,JSValueRef* Exception);

JSType		JSValueGetType(JSContextRef Context,JSValueRef Value);
bool		JSValueIsObject(JSContextRef Context,JSValueRef Value);
bool		JSValueIsObject(JSContextRef Context,JSObjectRef Value);
JSObjectRef JSValueToObject(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
void		JSValueProtect(JSContextRef Context,JSValueRef Value);
void		JSValueUnprotect(JSContextRef Context,JSValueRef Value);

JSPropertyNameArrayRef	JSObjectCopyPropertyNames(JSContextRef Context,JSObjectRef This);
size_t		JSPropertyNameArrayGetCount(JSPropertyNameArrayRef Keys);
JSStringRef	JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef Keys,size_t Index);

bool		JSValueIsNumber(JSContextRef Context,JSValueRef Value);
double		JSValueToNumber(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
JSValueRef	JSValueMakeNumber(JSContextRef Context,int Value);

bool		JSObjectIsFunction(JSContextRef Context,JSObjectRef Value);
JSValueRef	JSObjectCallAsFunction(JSContextRef Context,JSObjectRef Object,JSObjectRef This,size_t ArgumentCount,JSValueRef* Arguments,JSValueRef* Exception);
JSValueRef	JSObjectMakeFunctionWithCallback(JSContextRef Context,JSStringRef Name,JSObjectCallAsFunctionCallback FunctionPtr);

bool		JSValueToBoolean(JSContextRef Context,JSValueRef Value);
JSValueRef	JSValueMakeBoolean(JSContextRef Context,bool Value);

JSValueRef	JSValueMakeUndefined(JSContextRef Context);
bool		JSValueIsUndefined(JSContextRef Context,JSValueRef Value);

JSValueRef	JSValueMakeNull(JSContextRef Context);
bool		JSValueIsNull(JSContextRef Context,JSValueRef Value);

JSObjectRef	JSObjectMakeArray(JSContextRef Context,size_t ElementCount,const JSValueRef* Elements,JSValueRef* Exception);
bool		JSValueIsArray(JSContextRef Context,JSValueRef Value);
JSTypedArrayType	JSValueGetTypedArrayType(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
JSObjectRef	JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef Context,JSTypedArrayType ArrayType,void* Buffer,size_t BufferSize,JSTypedArrayBytesDeallocator Dealloc,void* DeallocContext,JSValueRef* Exception);
void*		JSObjectGetTypedArrayBytesPtr(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception);
size_t		JSObjectGetTypedArrayByteOffset(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception);
size_t		JSObjectGetTypedArrayLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception);
size_t		JSObjectGetTypedArrayByteLength(JSContextRef Context,JSObjectRef Array,JSValueRef* Exception);

JSValueRef			JSEvaluateScript(JSContextRef Context,JSStringRef Source,JSObjectRef This,JSStringRef Filename,int LineNumber,JSValueRef* Exception);
JSGlobalContextRef	JSContextGetGlobalContext(JSContextRef Context);
JSObjectRef			JSContextGetGlobalObject(JSContextRef Context);
JSContextGroupRef	JSContextGroupCreate();
void				JSContextGroupRelease(JSContextGroupRef ContextGroup);
JSContextRef		JSGlobalContextCreateInGroup(JSContextGroupRef ContextGroup,JSClassRef GlobalClass);
void				JSGlobalContextSetName(JSContextRef Context,JSStringRef Name);
void				JSGlobalContextRelease(JSContextRef Context);
void				JSGarbageCollect(JSContextRef Context);

JSStringRef	JSStringCreateWithUTF8CString(const char* Buffer);
size_t		JSStringGetUTF8CString(JSStringRef String,char* Buffer,size_t BufferSize);
size_t		JSStringGetLength(JSStringRef String);
JSStringRef	JSValueToStringCopy(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
JSValueRef	JSValueMakeString(JSContextRef Context,JSStringRef String);
void		JSStringRelease(JSStringRef String);

JSClassRef	JSClassCreate(JSClassDefinition* Definition);
void		JSClassRetain(JSClassRef Class);

