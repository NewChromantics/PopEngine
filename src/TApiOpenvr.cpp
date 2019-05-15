#include "TApiOpenvr.h"


namespace ApiOpenvr
{
	const char Namespace[] = "Pop.Openvr";

	DEFINE_BIND_TYPENAME(Hmd);
}


void ApiOpenvr::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<THmdWrapper>( Namespace );
}


void ApiOpenvr::THmdWrapper::Construct(Bind::TCallback& Params)
{
	//auto Filename = Params.GetArgumentFilename(0);
	
	//mLibrary.reset( new Soy::TRuntimeLibrary(Filename) );
}


void ApiOpenvr::THmdWrapper::CreateTemplate(Bind::TTemplate& Template)
{
//	Template.BindFunction<ApiDll::BindFunction_FunctionName>( BindFunction );
//	Template.BindFunction<ApiDll::CallFunction_FunctionName>( CallFunction );
}


