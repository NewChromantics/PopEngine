#pragma once
#include "TBind.h"
#include "SoyPixels.h"
#include "SoyVector.h"



//	engine debug things
namespace ApiEngine
{
	extern const char	Namespace[];
	void	Bind(Bind::TContext& Context);
	
	//	general debug stats in a self-updating window (so JS land won't drag it down)
	//	consider renaming this to BindContextStats, but something better for the user
	class TStatsWindow;
	class TStatsWindowWrapper;
	DECLARE_BIND_TYPENAME(StatsWindow);
}


class ApiEngine::TStatsWindowWrapper : public Bind::TObjectWrapper<BindType::StatsWindow, TStatsWindow>
{
public:
	TStatsWindowWrapper(Bind::TContext& Context) :
		TObjectWrapper(Context)
	{
	}

	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

public:
	std::shared_ptr<TStatsWindow>&	mWindow = mObject;
};

