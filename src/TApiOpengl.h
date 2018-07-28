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
	static v8::Local<v8::Value>				Execute(const v8::CallbackInfo& Arguments);	//	run javascript on gl thread for immediate mode stuff
	

public:
	v8::Persistent<v8::Object>		mHandle;
	std::shared_ptr<TRenderWindow>	mWindow;
	TV8Container*					mContainer;
	
	Opengl::TRenderTarget*			mActiveRenderTarget;	//	hack until render target is it's own [temp?] object
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

	static TShaderWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TShaderWrapper>( Value, 0 );	}
	
	void									CreateShader(Opengl::TContext& Context,std::function<Opengl::TGeometry&()> GetGeo,const char* VertSource,const char* FragSource);
	
public:
	v8::Persistent<v8::Object>			mHandle;
	std::shared_ptr<Opengl::TShader>	mShader;
	TV8Container*						mContainer;
};

