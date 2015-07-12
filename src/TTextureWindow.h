#pragma once


//	re-using unity opengl device interface
#include "SoyOpenglContext.h"

class TOpenglWindow;
class TPopOpengl;

namespace Opengl
{
	class TContext;
};

class TTextureWindow
{
public:
	TTextureWindow(std::string Name,vec2f Position,vec2f Size,TPopOpengl& Parent);
	~TTextureWindow();
	
	bool				IsValid()			{	return mWindow!=nullptr;	}
	
	void				OnOpenglRender(Opengl::TRenderTarget& RenderTarget);
	Opengl::TContext*	GetContext();
	
protected:
	void				DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect);
	
private:
	TPopOpengl&			mParent;
	
	Opengl::GlProgram	mBlitShader;
	Opengl::TGeometry	mBlitQuad;
	std::shared_ptr<TOpenglWindow>		mWindow;
	std::shared_ptr<Opengl::TTexture>	mTestTexture;
};
