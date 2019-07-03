#pragma once
#include "TBind.h"


namespace Soy
{
	class TRuntimeLibrary;
}

namespace ApiDll
{
	void	Bind(Bind::TContext& Context);

	DECLARE_BIND_TYPENAME(Library);
	
	class TFunctionBase;
}


class ApiDll::TFunctionBase
{
public:
	virtual void 	Call(Bind::TCallback& Params)=0;
};


class TDllWrapper : public Bind::TObjectWrapper<ApiDll::BindType::Library,Soy::TRuntimeLibrary>
{
public:
	TDllWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Params) override;

	static void					BindFunction(Bind::TCallback& Params);
	static void					CallFunction(Bind::TCallback& Params);

	ApiDll::TFunctionBase&		GetFunction(const std::string& FunctionName);

public:
	std::shared_ptr<Soy::TRuntimeLibrary>&	mLibrary = mObject;
	std::map<std::string,std::shared_ptr<ApiDll::TFunctionBase>>	mFunctions;
};

