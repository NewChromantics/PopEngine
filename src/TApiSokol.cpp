#include "TApiSokol.h"
 #include "sokol/sokol_app.h"
 #include "sokol/sokol_gfx.h"
 #include "sokol/sokol_glue.h"

static struct {
    sg_pass_action pass_action;
} state;

namespace ApiSokol
{
    const char Namespace[] = "Pop.Sokol";

    DEFINE_BIND_TYPENAME(Sokol);
    DEFINE_BIND_FUNCTIONNAME(Test);
}

void ApiSokol::Bind(Bind::TContext& Context)
{
    Context.CreateGlobalObjectInstance("", Namespace);
    Context.BindObjectType<TSokolWrapper>(Namespace);
}

class SoySokol
{
	
};

void ApiSokol::TSokolWrapper::CreateTemplate(Bind::TTemplate& Template)
{
    Template.BindFunction<BindFunction::Test>( &TSokolWrapper::Test );
}

void ApiSokol::TSokolWrapper::Construct(Bind::TCallback& Params)
{

}

#include "TApiSokol.hpp"
