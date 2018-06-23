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
	void	DrawQuad(Soy::Rectf Rect);
	
	public:
	std::shared_ptr<Opengl::TGeometry>	mBlitQuad;
	std::shared_ptr<Opengl::TShader>	mBlitShader;
};


//	v8 template to a TWindow
class TWindowWrapper
{
public:
	TWindowWrapper() :
	mContainer	( nullptr )
	{
	}
	~TWindowWrapper();
	
	void		OnRender(Opengl::TRenderTarget& RenderTarget);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static void								DrawQuad(const v8::CallbackInfo& Arguments);
	static void								ClearColour(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
public:
	v8::Persistent<v8::Object>				mHandle;
	std::shared_ptr<TRenderWindow>	mWindow;
	TV8Container*					mContainer;
};


