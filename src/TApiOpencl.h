#pragma once
#include "TBind.h"
#include "SoyOpenCl.h"


namespace ApiOpencl
{
	void	Bind(Bind::TContext& Context);
}

class TOpenclKernel;
class TOpenclContext;


//	bindings for an opencl context
extern const char Opencl_Context_TypeName[];
class TOpenclContextWrapper : public Bind::TObjectWrapper<Opencl_Context_TypeName,Opencl::TContextThread>
{
public:
	TOpenclContextWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper	( Context, This )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void 		Construct(Bind::TCallback& Arguments) override;
	void 				Construct(const std::string& DeviceName);

	static void			ExecuteKernel(Bind::TCallback& Arguments);
	void				DoExecuteKernel(TOpenclKernel& Kernel,BufferArray<int,3> IterationCount,std::shared_ptr<V8Storage<v8::Function>> IterationCallback,std::shared_ptr<V8Storage<v8::Function>> FinishedCallback,std::shared_ptr<V8Storage<v8::Promise::Resolver>> Resolver);

public:
	//	gr: we can handle multiple, but lets do that at a high level :)
	std::shared_ptr<Opencl::TDevice>			mOpenclDevice;
	std::shared_ptr<Opencl::TContextThread>&	mOpenclContext = mObject;
};



class TOpenclKernel
{
public:
	TOpenclKernel(TV8Container& Container,Opencl::TContext& Context,const std::string& Source,const std::string& KernelName);
	
	Opencl::TKernel&						GetKernel()	{	return *mKernel;	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	
	static v8::Local<v8::Value>				SetUniform(const v8::TCallback& Arguments);
	
public:
	TV8Container&						mContainer;
	v8::Persistent<v8::Object>			mHandle;
	
	//	we could have multiple kernels per program, but keeping it simple
	std::string							mKernelName;
	std::shared_ptr<Opencl::TProgram>	mProgram;
	std::shared_ptr<Opencl::TKernel>	mKernel;
};



class TOpenclKernelState
{
public:
	static const std::string				GetObjectTypeName()	{	return "OpenclKernelState";	}
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);

	static v8::Local<v8::Value>				SetUniform(const v8::TCallback& Arguments);
	static v8::Local<v8::Value>				ReadUniform(const v8::TCallback& Arguments);

public:

};

