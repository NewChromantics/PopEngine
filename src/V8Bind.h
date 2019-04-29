#pragma once


class JSContextRef
{
public:
	JSContextRef(std::nullptr_t);
	void	operator=(std::nullptr_t Null);
	bool	operator!=(std::nullptr_t Null) const;
};
typedef JSContextRef JSGlobalContextRef;

class JSContextGroupRef
{
public:
	JSContextGroupRef(std::nullptr_t);
};
	
class JSValueRef
{
public:
	JSValueRef();
	JSValueRef(std::nullptr_t);
	
	void	operator=(std::nullptr_t Null);
	bool	operator!=(std::nullptr_t Null) const;
	operator bool() const;
};

class JSObjectRef
{
public:
	JSObjectRef(std::nullptr_t);
	void	operator=(std::nullptr_t Null);
	bool	operator!=(std::nullptr_t Null) const;
	bool	operator!=(const JSObjectRef& That) const;
};

class JSStringRef
{
public:
	void	operator=(std::nullptr_t Null);
	bool	operator!=(std::nullptr_t Null) const;
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

typedef JSObjectRef
(*JSObjectCallAsConstructorCallback) (JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
typedef JSValueRef
(*JSObjectCallAsFunctionCallback) (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
typedef void
(*JSObjectFinalizeCallback) (JSObjectRef object);


class JSClassDefinition
{
public:
	const char*			className = nullptr;	//	emulate JScore instability with raw pointers
	JSClassAttributes	attributes;
	JSObjectCallAsConstructorCallback	callAsConstructor;
	JSObjectFinalizeCallback			finalize;
};
extern const JSClassDefinition kJSClassDefinitionEmpty;

class JSClassRef
{
public:
	JSClassRef(std::nullptr_t);
};

enum JSPropertyAttributes
{
	kJSPropertyAttributeNone
};

typedef struct {
	const char* name;
	JSObjectCallAsFunctionCallback callAsFunction;
	JSPropertyAttributes attributes;
} JSStaticFunction;


void		JSObjectSetPrivate(JSObjectRef Object,void* Data);
void*		JSObjectGetPrivate(JSObjectRef Object);
JSObjectRef	JSObjectMake(JSContextRef Context,void*,void*);


JSType		JSValueGetType(JSContextRef Context,JSValueRef Value);
bool		JSValueIsObject(JSContextRef Context,JSValueRef Value);
bool		JSValueIsObject(JSContextRef Context,JSObjectRef Value);
bool		JSValueIsNumber(JSContextRef Context,JSValueRef Value);
double		JSValueToNumber(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
JSValueRef	JSValueMakeNumber(JSContextRef Context,int Value);
bool		JSObjectIsFunction(JSContextRef Context,JSObjectRef Value);
bool		JSValueToBoolean(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
JSValueRef	JSValueMakeBoolean(JSContextRef Context,bool Value);

JSValueRef	JSEvaluateScript(JSContextRef Context,JSStringRef Source,void*,void*,int,JSValueRef* Exception);
JSGlobalContextRef	JSContextGetGlobalContext(JSContextRef Context);

JSStringRef	JSStringCreateWithUTF8CString(const char* Buffer);
size_t		JSStringGetUTF8CString(JSStringRef String,char* Buffer,size_t BufferSize);
size_t		JSStringGetLength(JSStringRef String);
JSStringRef	JSValueToStringCopy(JSContextRef Context,JSValueRef Value,JSValueRef* Exception);
void		JSStringRelease(JSStringRef String);
