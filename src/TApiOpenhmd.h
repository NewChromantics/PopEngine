#pragma once
#include "TBind.h"


namespace Openhmd
{
	class THmd;
	class THmdFrame;
}

namespace ApiOpenhmd
{
	void	Bind(Bind::TContext& Context);

	class THmdWrapper;
	DECLARE_BIND_TYPENAME(Hmd);
}


class ApiOpenhmd::THmdWrapper : public Bind::TObjectWrapper<Hmd_TypeName,Openhmd::THmd>
{
public:
	THmdWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

	void			GetEyeMatrix(Bind::TCallback& Params);

public:
	std::shared_ptr<Openhmd::THmd>&	mHmd = mObject;
};

