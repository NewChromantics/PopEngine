#pragma once


//	re-using unity opengl device interface
#include "SoyOpenglContext.h"


class TFilter;
class TOpenglWindow;



namespace Opengl
{
	class TContext;
};

class TFilterWindow
{
public:
	TFilterWindow(std::string Name,vec2f Position,vec2f Size,TFilter& Parent);
	~TFilterWindow();
	
	bool				IsValid()			{	return mWindow!=nullptr;	}
	
	void				OnOpenglRender(Opengl::TRenderTarget& RenderTarget);
	Opengl::TContext*	GetContext();
	
protected:
	void				DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect);
	
private:
	TFilter&			mParent;
	
	Opengl::TGeometry	mBlitQuad;
	Opengl::GlProgram	mBlitShader;
	std::shared_ptr<TOpenglWindow>		mWindow;
};
