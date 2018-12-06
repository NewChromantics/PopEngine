#pragma once


#include <JavaScriptCore/JavaScriptCore.h>
#include <memory>
#include "HeapArray.hpp"


//	https://karhm.com/JavaScriptCore_C_API/
namespace JsCore
{
	class TInstance;
	class TContext;
}

//	VM to contain multiple contexts/containers
class JsCore::TInstance
{
public:
	TInstance(const std::string& RootDirectory,const std::string& BootupFilename);
	~TInstance();
	
	std::shared_ptr<TContext>	CreateContext();
	
private:
	JSContextGroupRef	mContextGroup;
	Array<std::shared_ptr<TContext>>	mContexts;
};

class JsCore::TContext
{
public:
	TContext(JSGlobalContextRef Context);
	~TContext();
	
private:
	JSGlobalContextRef	mContext;
};
