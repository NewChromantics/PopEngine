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
	
	std::shared_ptr<TContext>	Platform_CreateContext(std::shared_ptr<SoyWindow> Window,const std::string& ViewName,int SampleCount);
}



class ApiSokol::TSokolContextWrapper : public Bind::TObjectWrapper<BindType::Context,Sokol::TContext>
{
public:
	TSokolContextWrapper(Bind::TContext &Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate &Template);
	virtual void	Construct(Bind::TCallback &Params) override;

	// Initial Test
	void			StartRender(Bind::TCallback &Params);

public:
	std::shared_ptr<Sokol::TContext>&		mSokolContext = mObject;
	Bind::TPersistent						mWindow;
	std::shared_ptr<SoyWindow>				mSoyWindow;
	std::string								mViewName;
	sg_pass_action				 			mPassAction;
	

	void									Init(sg_desc desc);
	void									RenderFrame();
};



class Sokol::TContext
{
public:
	virtual sg_context_desc		GetSokolContext()=0;	//	gr: pure virtual to stop the base class allocating which doesn't do anything
};
