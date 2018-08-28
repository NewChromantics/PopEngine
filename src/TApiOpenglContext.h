#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"
#include "TV8ObjectWrapper.h"
#include "TApiOpengl.h"

class GlViewSharedContext;
class TImageWrapper;

class OpenglObjects
{
public:
	//	all funcs are immediate, assuming we're on opengl thread
	//	get/alloc buffer with this id
	Opengl::TAsset	GetVao(int JavascriptName);
	Opengl::TAsset	GetBuffer(int JavascriptName);
	Opengl::TAsset	GetFrameBuffer(int JavascriptName);
	int				GetBufferJavascriptName(Opengl::TAsset Asset);

private:
	Opengl::TAsset	GetObject(int JavascriptName,Array<std::pair<int,Opengl::TAsset>>& Buffers,std::function<void(GLuint,GLuint*)> Alloc,const char* AllocFuncName);

public:
	Array<std::pair<int,Opengl::TAsset>>	mVaos;
	Array<std::pair<int,Opengl::TAsset>>	mBuffers;
	Array<std::pair<int,Opengl::TAsset>>	mFrameBuffers;
};





//	gr: maybe this can be a base type so we can share functions
extern const char OpenglImmediateContext_TypeName[];
class TOpenglImmediateContextWrapper : public TObjectWrapper<OpenglImmediateContext_TypeName,Opengl::TContext>, public TOpenglContextWrapper
{
private:
	typedef TOpenglImmediateContextWrapper this_type;
	
public:
	TOpenglImmediateContextWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This ),
		mLastFrameBufferTexture	( nullptr ),
		mLastBoundTexture		( nullptr )
	{
	}
	~TOpenglImmediateContextWrapper();
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;
	
	
	//	run javascript on gl thread for immediate mode stuff
	static v8::Local<v8::Value>				Execute(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				ExecuteCompiledQueue(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				FlushAsync(const v8::CallbackInfo& Arguments);	//	returns promise for when flush has finished

	//	return a named array of immediate-use GL enum values
	static v8::Local<v8::Value>				GetEnums(const v8::CallbackInfo& Arguments);
	
	//	immediate calls
	static v8::Local<v8::Value>				Immediate_disable(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_enable(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_cullFace(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_bindBuffer(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_bufferData(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_bindFramebuffer(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_framebufferTexture2D(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_bindTexture(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_texImage2D(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_useProgram(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_texParameteri(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_vertexAttribPointer(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_enableVertexAttribArray(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_texSubImage2D(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_readPixels(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_viewport(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_scissor(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_activeTexture(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_drawElements(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Immediate_flush(const v8::CallbackInfo& Arguments);
	
	virtual std::shared_ptr<Opengl::TContext>		GetOpenglContext() override {	return mContext;	}
	
	TImageWrapper*							GetBoundTexture(GLenum Binding);
	TImageWrapper*							GetBoundFrameBufferTexture();
	
public:
	std::shared_ptr<Opengl::TContext>&		mContext = mObject;
	
	//	opengl objects allocated for immediate mode
	OpenglObjects							mImmediateObjects;
	
	//	hack!
	TImageWrapper*			mLastFrameBufferTexture;
	TImageWrapper*			mLastBoundTexture;
};


	

