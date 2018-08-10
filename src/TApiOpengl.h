#pragma once
#include "TV8Container.h"
#include "SoyOpenglWindow.h"



namespace ApiOpengl
{
	void	Bind(TV8Container& Container);
}



class TRenderWindow : public TOpenglWindow
{
public:
	TRenderWindow(const std::string& Name,const TOpenglParams& Params) :
		TOpenglWindow	( Name, Soy::Rectf(0,0,100,100), Params)
	{
	}
	
	void	Clear(Opengl::TRenderTarget& RenderTarget);
	void	ClearColour(Soy::TRgb Colour);
	void	DrawQuad();
	void	DrawQuad(Opengl::TShader& Shader,std::function<void()> OnShaderBind);
	
	Opengl::TGeometry&	GetBlitQuad();
	
public:
	std::shared_ptr<Opengl::TGeometry>	mBlitQuad;
	
	std::shared_ptr<Opengl::TShader>	mDebugShader;
};


class OpenglObjects
{
public:
	//	all funcs are immediate, assuming we're on opengl thread
	//	get/alloc buffer with this id
	Opengl::TAsset	GetVao(int JavascriptName);
	Opengl::TAsset	GetBuffer(int JavascriptName);
	Opengl::TAsset	GetFrameBuffer(int JavascriptName);

private:
	Opengl::TAsset	GetObject(int JavascriptName,Array<std::pair<int,Opengl::TAsset>>& Buffers,std::function<void(GLuint,GLuint*)> Alloc,const char* AllocFuncName);

public:
	Array<std::pair<int,Opengl::TAsset>>	mVaos;
	Array<std::pair<int,Opengl::TAsset>>	mBuffers;
	Array<std::pair<int,Opengl::TAsset>>	mFrameBuffers;
};

//	v8 template to a TWindow
class TWindowWrapper
{
public:
	TWindowWrapper() :
		mContainer			( nullptr ),
		mActiveRenderTarget	(nullptr)
	{
	}
	~TWindowWrapper();
	
	//	gr: removing this!
	void		OnRender(Opengl::TRenderTarget& RenderTarget);
	
	static TWindowWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TWindowWrapper>( Value, 0 );	}

	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	
	//	these are context things
	static v8::Local<v8::Value>				DrawQuad(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				ClearColour(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				SetViewport(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Render(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				RenderChain(const v8::CallbackInfo& Arguments);

	//	run javascript on gl thread for immediate mode stuff
	static v8::Local<v8::Value>				Execute(const v8::CallbackInfo& Arguments);

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

protected:
	static void								OnFree(const v8::WeakCallbackInfo<TWindowWrapper>& data);

public:
	v8::Persistent<v8::Object>		mHandle;
	std::shared_ptr<TRenderWindow>	mWindow;
	TV8Container*					mContainer;
	
	Opengl::TRenderTarget*			mActiveRenderTarget;	//	hack until render target is it's own [temp?] object
	
	//	opengl objects allocated for immediate mode
	OpenglObjects					mImmediateObjects;
};


class TShaderWrapper
{
public:
	TShaderWrapper() :
		mContainer	( nullptr )
	{
	}
	~TShaderWrapper();
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static v8::Local<v8::Value>				SetUniform(const v8::CallbackInfo& Arguments);
	v8::Local<v8::Value>					DoSetUniform(const v8::CallbackInfo& Arguments);

	static TShaderWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TShaderWrapper>( Value, 0 );	}
	
	void									CreateShader(std::shared_ptr<Opengl::TContext>& Context,const char* VertSource,const char* FragSource);
	
public:
	Opengl::TContext*					mContext;
	v8::Persistent<v8::Object>			mHandle;
	std::shared_ptr<Opengl::TShader>	mShader;
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::function<void()>				mShaderDealloc;
	TV8Container*						mContainer;
};

