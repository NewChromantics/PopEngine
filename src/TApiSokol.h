#pragma once
#include "TBind.h"
#include "SoyWindow.h"
#include "sokol/sokol_gfx.h"

namespace ApiSokol
{
	void Bind(Bind::TContext &Context);

	class TSokolWrapper;
	class TSokolContext;

	DECLARE_BIND_TYPENAME(RenderPipeline);
}

class TApiSokol;

class ApiSokol::TSokolWrapper : public Bind::TObjectWrapper<BindType::RenderPipeline, TApiSokol>
{
public:
	TSokolWrapper(Bind::TContext &Context) : TObjectWrapper(Context)
	{
	}

	static void CreateTemplate(Bind::TTemplate &Template);
	virtual void Construct(Bind::TCallback &Params) override;

	// Initial Test
	void StartRender(Bind::TCallback &Params);

public:
	Bind::TPersistent															mWindow;
	std::shared_ptr<SoyWindow>										mSoyWindow;
	sg_pass_action				 												mPassAction;

	void																					Init(sg_desc desc);
	void																					RenderFrame();
};

class ApiSokol::TSokolContext
{
public:
	TSokolContext( std::shared_ptr<SoyWindow> mSoyWindow );

	sg_context_desc								mContextDesc;

};
