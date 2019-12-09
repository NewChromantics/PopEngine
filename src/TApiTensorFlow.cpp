#include "TApiTensorFlow.h"


namespace ApiTensorFlow
{
	const char Namespace[] = "Pop.TensorFlow";

	DEFINE_BIND_TYPENAME(Model);

	DEFINE_BIND_FUNCTIONNAME(GetVersion);

	void	GetVersion(Bind::TCallback& Params);
}

namespace TensorFlow
{
}


//	make a job queue
class TensorFlow::TModel
{
public:
	TModel();
	~TModel();
};


void ApiTensorFlow::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<BindFunction::GetVersion>( GetVersion, Namespace);

	Context.BindObjectType<TModelWrapper>(Namespace);
}

void ApiTensorFlow::GetVersion(Bind::TCallback& Params)
{
	std::string Version(TF_Version());
	Params.Return(Version);
}

void ApiTensorFlow::TModelWrapper::Construct(Bind::TCallback& Params)
{
	mModel.reset(new TensorFlow::TModel);
}


void ApiTensorFlow::TModelWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	//Template.BindFunction<BindFunction::WaitForPoses>(&THmdWrapper::WaitForPoses);
}

TensorFlow::TModel::TModel()
{

}

TensorFlow::TModel::~TModel()
{

}
