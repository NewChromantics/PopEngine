#pragma once
#include <SoyApp.h>

//	re-using unity opengl device interface
#include "SoyOpenglContext.h"
#include <SoyWindow.h>

class TFilter;
class TOpenglWindow;
class TV8Container;
class TV8Instance;

namespace Opengl
{
	class TContext;
};

class TPopTrack
{
public:
	TPopTrack(const std::string& BootupJavascript);
	~TPopTrack();
	
	void				OnOpenglRender(Opengl::TRenderTarget& RenderTarget);
	std::shared_ptr<Opengl::TContext>	GetContext();
	
protected:
	void				DrawQuad(Opengl::TTexture Texture,Soy::Rectf Rect);
	
private:
	//std::shared_ptr<Opengl::TGeometry>	mBlitQuad;
	//std::shared_ptr<Opengl::TShader>	mBlitShader;
	std::shared_ptr<TV8Instance>		mV8Instance;
};



//	gr: easy tear up/down instance per script
//	should really keep an Isolate and recreate contexts
//	gr: now we're manually pumping the v8 message queue, really, we want a thread
//	that wakes up (soy worker thread)
//	which means really we should implement a v8 platform and manage subthreads and get notified of new ones
class TV8Instance : public SoyWorkerThread
{
public:
	TV8Instance(const std::string& ScriptFilename);
	~TV8Instance();
	
	std::shared_ptr<TV8Container>		mV8Container;
	std::shared_ptr<SoyWorkerThread>	mV8Thread;
	
protected:
	virtual bool	Iteration() override;

};

