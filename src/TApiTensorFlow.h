#pragma once
#include "TBind.h"
#include "tensorflow/c/c_api.h"


namespace TensorFlow
{
	class TModel;
}

namespace ApiTensorFlow
{
	void	Bind(Bind::TContext& Context);

	class TModelWrapper;
	DECLARE_BIND_TYPENAME(Model);
}


class ApiTensorFlow::TModelWrapper : public Bind::TObjectWrapper<BindType::Model, TensorFlow::TModel>
{
public:
	TModelWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

public:
	std::shared_ptr<TensorFlow::TModel>&	mModel = mObject;
};



