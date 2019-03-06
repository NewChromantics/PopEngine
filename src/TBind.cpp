#include "TBind.h"
#include "SoyLib/src/SoyString.h"


Bind::TObject Bind::TContext::GetGlobalObject(const std::string& ObjectName)
{
	auto Global = GetRootGlobalObject();
	if ( ObjectName.length() == 0 )
		return Global;
	auto Child = Global.GetObject( ObjectName );
	return Child;
}


void Bind::TContext::CreateGlobalObjectInstance(TString ObjectType,TString Name)
{
	auto NewObject = CreateObjectInstance( ObjectType );
	auto ParentName = Name;
	auto ObjectName = Soy::StringPopRight( ParentName, '.' );
	auto ParentObject = GetGlobalObject( ParentName );
	ParentObject.Set( ObjectName, NewObject );
}


