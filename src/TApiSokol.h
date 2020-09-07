#pragma once
#include "TBind.h"

namespace ApiSokol
{
	void Bind(Bind::TContext &Context);

	class TSokolWrapper;
	DECLARE_BIND_TYPENAME(RenderPipeline);
}

class SoySokol;

class ApiSokol::TSokolWrapper : public Bind::TObjectWrapper<BindType::RenderPipeline, SoySokol>
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
	Bind::TPersistent								mWindow;
};
