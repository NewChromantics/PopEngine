#pragma once
#include "TV8Container.h"
#include <SoyOpenCl.h>


namespace ApiOpencl
{
	void	Bind(TV8Container& Container);
}

class TOpenclKernel;
class TOpenclContext;


//	bindings for an opencl context
class TOpenclContext
{
public:
	TOpenclContext(TV8Container& Container,const std::string& DeviceName);
	
	static TOpenclContext&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TOpenclContext>( Value, 0 );	}

	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);

	static v8::Local<v8::Value>				Run(const v8::CallbackInfo& Arguments);
	void									DoRun(TOpenclKernel& Kernel,BufferArray<int,3> IterationCount,v8::Persist<v8::Value> IterationCallback,v8::Persist<v8::Value> FinishedCallback,v8::Persist<v8::Promise::Resolver> Resolver);

public:
	TV8Container&						mContainer;
	v8::Persistent<v8::Object>			mHandle;
	
	//	gr: we can handle multiple, but lets do that at a high level :)
	std::shared_ptr<Opencl::TDevice>		mOpenclDevice;
	std::shared_ptr<Opencl::TContextThread>	mOpenclContext;
};



class TOpenclKernel
{
public:
	TOpenclKernel(TV8Container& Container,Opencl::TContext& Context,const std::string& Source,const std::string& KernelName);
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	
	static v8::Local<v8::Value>				SetUniform(const v8::CallbackInfo& Arguments);
	
public:
	TV8Container&						mContainer;
	v8::Persistent<v8::Object>			mHandle;
	
	//	we could have multiple kernels per program, but keeping it simple
	std::string							mKernelName;
	std::shared_ptr<Opencl::TProgram>	mProgram;
	std::shared_ptr<Opencl::TKernel>	mKernel;
};



