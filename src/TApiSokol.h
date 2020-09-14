#pragma once
#include "TBind.h"
#include "SoyWindow.h"
#include "sokol/sokol_gfx.h"

namespace ApiSokol
{
	void Bind(Bind::TContext &Context);

	class TSokolContextWrapper;

	DECLARE_BIND_TYPENAME(Context);
}

//	non-js-api sokol
namespace Sokol
{
	class TContext;
	class TContextParams;	//	or ViewParams?
	
	std::shared_ptr<TContext>	Platform_CreateContext(std::shared_ptr<SoyWindow> Window,TContextParams Params);
}

class Sokol::TContextParams
{
public:
	std::function<void(sg_context,vec2x<size_t>)>	mOnPaint;		//	render callback
	std::string							mViewName;		//	try to attach to existing views
	size_t								mFramesPerSecond = 60;
};

class ApiSokol::TSokolContextWrapper : public Bind::TObjectWrapper<BindType::Context,Sokol::TContext>
{
public:
	TSokolContextWrapper(Bind::TContext &Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate &Template);
	virtual void	Construct(Bind::TCallback &Params) override;

	//	gr: this could be more like SubmitNextFrame with a list of render commands
	//		that C++ will excute next time the context wants to execute
	//void			StartRender(Bind::TCallback &Params);
	
	//	gr: this could be async, submit render commands then return promise. Resolve promise when those commands are rendered
	//		this is a bit backwards to normal, we normally want to be signalled when its time to render...
	//void			WaitForFrameRendered(Bind::TCallback &Params);

private:
	//	gr: sg_context isnt REQUIRED, but hints to implementations that they should be creating it
	void			OnPaint(sg_context Context,vec2x<size_t> ViewRect);
	
public:
	std::shared_ptr<Sokol::TContext>&		mSokolContext = mObject;
	//Bind::TPersistent						mWindow;
	//std::shared_ptr<SoyWindow>				mSoyWindow;
};



class Sokol::TContext
{
public:
	//std::function<void(sg_context,vec2x<size_t>)>	mOnPaint;
};


