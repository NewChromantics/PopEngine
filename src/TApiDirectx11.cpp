#include "TApiDirectx11.h"
#include "TApiCommon.h"


namespace ApiDirectx11
{
	const char Namespace[] = "Pop.Directx11";

	
	DEFINE_BIND_TYPENAME(Context);
}


void ApiDirectx11::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<TContextWrapper>( Namespace );
}


ApiDirectx11::TContextWrapper::~TContextWrapper()
{
	if ( mContext )
	{
		//mContext->WaitToFinish();
		mContext.reset();
	}
}

void ApiDirectx11::TContextWrapper::CreateTemplate(Bind::TTemplate& Template)
{
}

void ApiDirectx11::TContextWrapper::Construct(Bind::TCallback& Params)
{
	auto WindowName = Params.GetArgumentString(0);
	mContext.reset(new Directx::TContext());

}
