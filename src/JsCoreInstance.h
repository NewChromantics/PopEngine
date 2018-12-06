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
	TInstance(const std::string& RootDirectory,const std::string& ScriptFilename);
	~TInstance();
	
	std::shared_ptr<TContext>	CreateContext();
	
private:
	JSContextGroupRef	mContextGroup;
	std::string			mRootDirectory;
	
	std::shared_ptr<TContext>	mContext;
};

class JsCore::TContext
{
public:
	TContext(JSGlobalContextRef Context,const std::string& RootDirectory);
	~TContext();
	
	void				LoadScript(const std::string& Source,const std::string& Filename);
	
private:
	void				ThrowException(JSValueRef ExceptionHandle);	//	throws if value is not undefined
	
private:
	JSGlobalContextRef	mContext;
	std::string			mRootDirectory;
};
