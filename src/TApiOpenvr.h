#pragma once
#include "TBind.h"


namespace Openvr
{
	class THmd;
}

namespace ApiOpenvr
{
	void	Bind(Bind::TContext& Context);

	class THmdWrapper;
	DECLARE_BIND_TYPENAME(Hmd);
}


class ApiOpenvr::THmdWrapper : public Bind::TObjectWrapper<ApiOpenvr::Hmd_TypeName,Openvr::THmd>
{
public:
	THmdWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

public:
	std::shared_ptr<Openvr::THmd>&	mHmd = mObject;
};

