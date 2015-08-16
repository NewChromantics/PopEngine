#pragma once


//	re-using unity opengl device interface
#include "SoyOpenglContext.h"
#include <SoyWindow.h>

class TFilter;
class TOpenglWindow;



namespace Opengl
{
	class TContext;
};

class TFilterWindow
{
public:
	TFilterWindow(std::string Name,TFilter& Parent);
	~TFilterWindow();
	
	bool				IsValid()			{	return mWindow!=nullptr;	}
	
	void				OnOpenglRender(Opengl::TRenderTarget& RenderTarget);
	Opengl::TContext*	GetContext();
	
	void				OnMouseDown(const TMousePos& Pos);
	void				OnMouseMove(const TMousePos& Pos);
	void				OnMouseUp(const TMousePos& Pos);
	
protected:
	void				DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect);
	
private:
	TFilter&			mParent;
	
	std::shared_ptr<Opengl::TGeometry>	mBlitQuad;
	std::shared_ptr<Opengl::TShader>	mBlitShader;
	std::shared_ptr<TOpenglWindow>		mWindow;
	
	std::function<void()>	mZoomFunc;
	float				mZoom;
	vec2f				mZoomPosPx;
};
