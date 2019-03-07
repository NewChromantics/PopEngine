#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"
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
	
	virtual void 							Construct(Bind::TCallback& Arguments) override;
	
	
	//	run javascript on gl thread for immediate mode stuff
	static v8::Local<v8::Value>				Execute(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				ExecuteCompiledQueue(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				FlushAsync(v8::TCallback& Arguments);	//	returns promise for when flush has finished

	//	return a named array of immediate-use GL enum values
	static v8::Local<v8::Value>				GetEnums(v8::TCallback& Arguments);
	
	//	immediate calls
	static v8::Local<v8::Value>				Immediate_disable(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_enable(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_cullFace(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_bindBuffer(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_bufferData(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_bindFramebuffer(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_framebufferTexture2D(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_bindTexture(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_texImage2D(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_useProgram(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_texParameteri(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_vertexAttribPointer(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_enableVertexAttribArray(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_texSubImage2D(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_readPixels(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_viewport(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_scissor(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_activeTexture(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_drawElements(v8::TCallback& Arguments);
	static v8::Local<v8::Value>				Immediate_flush(v8::TCallback& Arguments);
	
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


	

