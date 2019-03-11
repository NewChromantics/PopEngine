#include "TApiOpencv.h"
#include "TApiCommon.h"


namespace ApiOpencv
{
	const char Namespace[] = "Opencv";
	
	void	FindContours(Bind::TCallback& Params);
	
	const char FindContours_FunctionName[] = "FindContours";
}


void ApiOpencv::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	
	Context.BindGlobalFunction<FindContours_FunctionName>( FindContours, Namespace );
}


void ApiOpencv::FindContours(Bind::TCallback &Params)
{
	auto& Image = Params.GetArgumentPointer<TImageWrapper>(0);
	throw Soy::AssertException("todo: find contours!");
}
