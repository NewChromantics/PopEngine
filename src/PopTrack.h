#pragma once
#include <SoyApp.h>

//	re-using unity opengl device interface
#include "SoyOpenglContext.h"
#include <SoyWindow.h>

class TFilter;
class TOpenglWindow;
class TV8Container;


namespace Opengl
{
	class TContext;
};

class TPopTrack
{
public:
	TPopTrack(const std::string& WindowName);
	~TPopTrack();
	
	void				OnOpenglRender(Opengl::TRenderTarget& RenderTarget);
	std::shared_ptr<Opengl::TContext>	GetContext();
	
protected:
	void				DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect);
	
private:
	std::shared_ptr<Opengl::TGeometry>	mBlitQuad;
	std::shared_ptr<Opengl::TShader>	mBlitShader;
	std::shared_ptr<TOpenglWindow>		mWindow;
	std::shared_ptr<TV8Container>		mV8Container;
};
