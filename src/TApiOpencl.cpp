#include "TApiOpencl.h"
#include "TApiCommon.h"

using namespace v8;

const char OpenclEnumDevices_FunctionName[] = "OpenclEnumDevices";

static v8::Local<v8::Value> OpenclEnumDevices(v8::CallbackInfo& Params);


void ApiOpencl::Bind(TV8Container& Container)
{
	Container.BindObjectType("OpenclContext", TOpenclContext::CreateTemplate );
	Container.BindGlobalFunction<OpenclEnumDevices_FunctionName>( OpenclEnumDevices );
}



class TOpenclRunner : public PopWorker::TJob
{
public:
	TOpenclRunner(Opencl::TContext& Context,Opencl::TKernel& Kernel) :
	mContext	( Context ),
	mKernel		( Kernel )
	{
	}
	
	virtual void		Run() override;
	
protected:
	//	get iterations and can setup first set of kernel args
	//	number of elements in the array dictates dimensions
	virtual void		Init(Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>&& Iterations)=0;
	
	//	set any iteration-specific args
	virtual void		RunIteration(Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& WorkGroups,bool& Block)=0;
	
	//	after last iteration - read back data etc
	virtual void		OnFinished(Opencl::TKernelState& Kernel)=0;
	
public:
	Opencl::TKernel&	mKernel;
	Opencl::TContext&	mContext;
};


class TOpenclRunnerLambda : public TOpenclRunner
{
public:
	TOpenclRunnerLambda(Opencl::TContext& Context,Opencl::TKernel& Kernel,std::function<void(Opencl::TKernelState&,ArrayBridge<vec2x<size_t>>&)> InitLambda,std::function<void(Opencl::TKernelState&,const Opencl::TKernelIteration&,bool&)> IterationLambda,std::function<void(Opencl::TKernelState&)> FinishedLambda) :
	TOpenclRunner		( Context, Kernel ),
	mIterationLambda	( IterationLambda ),
	mInitLambda			( InitLambda ),
	mFinishedLambda		( FinishedLambda )
	{
	}
	
	virtual void		Init(Opencl::TKernelState& Kernel,ArrayBridge<vec2x<size_t>>&& Iterations) override
	{
		mInitLambda( Kernel, Iterations );
	}
	
	virtual void		RunIteration(Opencl::TKernelState& Kernel,const Opencl::TKernelIteration& WorkGroups,bool& Block) override
	{
		mIterationLambda( Kernel, WorkGroups, Block );
	}
	
	//	after last iteration - read back data etc
	virtual void		OnFinished(Opencl::TKernelState& Kernel) override
	{
		mFinishedLambda( Kernel );
	}
	
public:
	std::function<void(Opencl::TKernelState&,ArrayBridge<vec2x<size_t>>&)>	mInitLambda;
	std::function<void(Opencl::TKernelState&,const Opencl::TKernelIteration&,bool&)>	mIterationLambda;
	std::function<void(Opencl::TKernelState&)>						mFinishedLambda;
};


void TOpenclRunner::Run()
{
	//ofScopeTimerWarning Timer( (std::string("Opencl ") + this->mKernel.mKernelName).c_str(), 0 );
	auto Kernel = mKernel.Lock(mContext);
	
	//	get iterations we want
	Array<vec2x<size_t>> Iterations;
	Init( Kernel, GetArrayBridge( Iterations ) );
	
	//	divide up the iterations
	Array<Opencl::TKernelIteration> IterationSplits;
	Kernel.GetIterations( GetArrayBridge(IterationSplits), GetArrayBridge(Iterations) );
	
	//	for now, because buffers get realeased etc when the kernelstate is destructed,
	//	lets just block on the last execution to make sure nothing is in use. Optimise later.
	Opencl::TSync LastSemaphore;
	static bool BlockLast = true;
	
	for ( int i=0;	i<IterationSplits.GetSize();	i++ )
	{
		auto& Iteration = IterationSplits[i];
		
		//	setup the iteration
		bool Block = false;
		RunIteration( Kernel, Iteration, Block );
		
		//	execute it
		Opencl::TSync ItSemaphore;
		auto* Semaphore = Block ? &ItSemaphore : nullptr;
		if ( BlockLast && i == IterationSplits.GetSize()-1 )
			Semaphore = &LastSemaphore;
		
		if ( Semaphore )
		{
			Kernel.QueueIteration( Iteration, *Semaphore );
			Semaphore->Wait();
		}
		else
		{
			Kernel.QueueIteration( Iteration );
		}
	}
	
	LastSemaphore.Wait();
	
	OnFinished( Kernel );
}



static Local<Value> OpenclEnumDevices(CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;

	//	get devices
	Array<Opencl::TDeviceMeta> Devices;
	auto Filter = OpenclDevice::Type::GPU;
	Opencl::GetDevices( GetArrayBridge(Devices), Filter );
	
	//	make an array to return
	auto DevicesArray = v8::Array::New( &Params.GetIsolate() );
	for ( auto i=0;	i<Devices.GetSize();	i++ )
	{
		auto& Device = Devices[i];
		auto DeviceName = v8::GetString( Params.GetIsolate(), Device.mName );
		DevicesArray->Set( i, DeviceName );
	}
	
	return DevicesArray;
}



TOpenclContext::TOpenclContext(TV8Container& Container,const std::string& DeviceName) :
	mContainer	( Container )
{
	//	find the device
	Opengl::TContext* OpenglContext = nullptr;
	
	Array<Opencl::TDeviceMeta> DeviceMetas;
	auto EnumDevice = [&](const Opencl::TDeviceMeta& Device)
	{
		auto MatchDeviceName = Device.mName;
		if ( !Soy::StringContains( MatchDeviceName, DeviceName, false ) )
			return;
		DeviceMetas.PushBack( Device );
	};
	Opencl::EnumDevices( EnumDevice );
	
	mOpenclDevice.reset( new Opencl::TDevice( GetArrayBridge(DeviceMetas) ) );
	mOpenclContext = mOpenclDevice->CreateContextThread( DeviceName );
	
}


Local<FunctionTemplate> TOpenclContext::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	//	https://github.com/v8/v8/wiki/Embedder's-Guide
	//	1 field to 1 c++ object
	//	gr: we can just use the template that's made automatically and modify that!
	//	gr: prototypetemplate and instancetemplate are basically the same
	//		but for inheritance we may want to use prototype
	//		https://groups.google.com/forum/#!topic/v8-users/_i-3mgG5z-c
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);
	
	return ConstructorFunc;
}


void TOpenclContext::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	if ( Arguments.Length() != 1 )
	{
		auto Exception = Isolate->ThrowException( v8::GetString( *Arguments.GetIsolate(), "Expected 1 argument, OpenclContext(DeviceName)"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
	
	//	get the device
	auto DeviceName = v8::GetString( Arguments[0] );
	
	//	alloc window
	//	gr: this should be OWNED by the context (so we can destroy all c++ objects with the context)
	//		but it also needs to know of the V8container to run stuff
	//		cyclic hell!
	auto* NewContext = new TOpenclContext( Container, DeviceName );
	
	NewContext->mHandle.Reset( Isolate, Arguments.This() );
	
	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewContext ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}









TOpenclKernel::TOpenclKernel(TV8Container& Container,Opencl::TContext& Context,const std::string& Source,const std::string& KernelName) :
	mContainer	( Container )
{
	mProgram.reset( new Opencl::TProgram( Source, Context ) );
	auto& Program = *mProgram;
	mKernel.reset( new Opencl::TKernel( KernelName, Program ) );
}


Local<FunctionTemplate> TOpenclKernel::CreateTemplate(TV8Container& Container)
{
	auto* Isolate = Container.mIsolate;
	
	//	pass the container around
	auto ContainerHandle = External::New( Isolate, &Container );
	auto ConstructorFunc = FunctionTemplate::New( Isolate, Constructor, ContainerHandle );
	
	auto InstanceTemplate = ConstructorFunc->InstanceTemplate();
	//	[0] object
	//	[1] container
	InstanceTemplate->SetInternalFieldCount(2);
	
	return ConstructorFunc;
}


void TOpenclKernel::Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments)
{
	auto* Isolate = Arguments.GetIsolate();
	
	if ( !Arguments.IsConstructCall() )
	{
		auto Exception = Isolate->ThrowException(String::NewFromUtf8( Isolate, "Expecting to be used as constructor. new Window(Name);"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	if ( Arguments.Length() != 3 )
	{
		auto Exception = Isolate->ThrowException( v8::GetString( *Arguments.GetIsolate(), "Expected 2 arguments, OpenclKernel(Context,Source,KernelName)"));
		Arguments.GetReturnValue().Set(Exception);
		return;
	}
	
	auto This = Arguments.This();
	auto& Container = v8::GetObject<TV8Container>( Arguments.Data() );
	auto& Context = v8::GetObject<TOpenclContext>( Arguments[0] );
	auto KernelSource = v8::GetString( Arguments[1] );
	auto KernelName = v8::GetString( Arguments[2] );

	//	alloc
	auto& ContextContext = *Context.mOpenclContext;
	auto* NewKernel = new TOpenclKernel( Container, ContextContext, KernelSource, KernelName );
	
	NewKernel->mHandle.Reset( Isolate, This );
	
	//	set fields
	This->SetInternalField( 0, External::New( Arguments.GetIsolate(), NewKernel ) );
	
	// return the new object back to the javascript caller
	Arguments.GetReturnValue().Set( This );
}


v8::Local<v8::Value> TOpenclContext::Run(const v8::CallbackInfo& Params)
{
	auto& Arguments = Params.mParams;
	auto& This = v8::GetObject<TOpenclContext>( Arguments.This() );
	auto* Isolate = Params.mIsolate;
	auto* Container = &Params.mContainer;

	//	make a promise resolver (persistent to copy to thread)
	auto Resolver = v8::Promise::Resolver::New( Isolate );
/*	auto ResolverPersistent = v8::GetPersistent( *Isolate, Resolver );
	
	//auto KernelPersistent = v8::GetPersistent( *Isolate, Arguments[0] );
	auto& Kernel = v8::GetObject<TOpenclKernel>(Arguments[0]);
	BufferArray<int,3> IterationCount;
	v8::EnumArray( Arguments[1], GetArrayBridge(IterationCount) );
	auto IterationCallbackPersistent = v8::GetPersistent( *Isolate, Arguments[2] );
	auto FinishedCallbackPersistent = v8::GetPersistent( *Isolate, Arguments[3] );
	
	This.DoRun( Kernel, IterationCount, IterationCallbackPersistent, FinishedCallbackPersistent, ResolverPersistent );
	*/
	
	//	return the promise
	auto Promise = Resolver->GetPromise();
	return Promise;
}


void TOpenclContext::DoRun(TOpenclKernel& Kernel,BufferArray<int,3> IterationCount,Persist<Value> IterationCallback,Persist<Value> FinishedCallback,Persist<Promise::Resolver> Resolver)
{
	auto* Isolate = &this->mContainer.GetIsolate();
	
	auto KernelInit = [](Opencl::TKernelState&,ArrayBridge<vec2x<size_t>>&)
	{
	};
	auto KernelIteration = [](Opencl::TKernelState&,const Opencl::TKernelIteration&,bool&)
	{
	};
	auto KernelFinished = [](Opencl::TKernelState&)
	{
	};
	/*
	 auto ExecuteRenderCallback = [=](Local<v8::Context> Context)
	 {
	 auto* Isolate = Container->mIsolate;
	 BufferArray<v8::Local<v8::Value>,2> CallbackParams;
	 auto WindowLocal = v8::GetLocal( *Isolate, WindowPersistent );
	 auto TargetLocal = v8::GetLocal( *Isolate, TargetPersistent );
	 CallbackParams.PushBack( WindowLocal );
	 CallbackParams.PushBack( TargetLocal );
	 auto CallbackFunctionLocal = v8::GetLocal( *Isolate, RenderCallbackPersistent );
	 auto CallbackFunctionLocalFunc = v8::Local<Function>::Cast( CallbackFunctionLocal );
	 auto FunctionThis = Context->Global();
	 Container->ExecuteFunc( Context, CallbackFunctionLocalFunc, FunctionThis, GetArrayBridge(CallbackParams) );
	 };
	 */
	auto OnCompleted = [=](Local<Context> Context)
	{
		auto ResolverLocal = v8::GetLocal( *Isolate, Resolver );
		//ResolverLocal->Resolve( v8::Undefined(&Isolate) );
	};
	
	auto OpenclRun = [=]
	{
		/*
		 try
		 {
		 //	get the texture from the image
		 std::string GenerateTextureError;
		 auto OnError = [&](const std::string& Error)
		 {
		 throw Soy::AssertException(Error);
		 };
		 TargetImage->GetTexture( []{}, OnError );
		 
		 //	setup render target
		 auto& TargetTexture = TargetImage->GetTexture();
		 Opengl::TRenderTargetFbo RenderTarget( TargetTexture );
		 RenderTarget.mGenerateMipMaps = false;
		 RenderTarget.Bind();
		 RenderTarget.SetViewportNormalised( Soy::Rectf(0,0,1,1) );
		 try
		 {
		 //	immediately call the javascript callback
		 Container->RunScoped( ExecuteRenderCallback );
		 RenderTarget.Unbind();
		 }
		 catch(std::exception& e)
		 {
		 RenderTarget.Unbind();
		 throw;
		 }
		 
		 //	queue the completion, doesn't need to be done instantly
		 Container->QueueScoped( OnCompleted );
		 }
		 catch(std::exception& e)
		 {
		 //	queue the error callback
		 std::string ExceptionString(e.what());
		 auto OnError = [=](Local<Context> Context)
		 {
		 auto ResolverLocal = v8::GetLocal( *Isolate, ResolverPersistent );
		 //	gr: does this need to be an exception? string?
		 auto Error = String::NewFromUtf8( Isolate, ExceptionString.c_str() );
		 //auto Exception = v8::GetException( *Context->GetIsolate(), ExceptionString)
		 //ResolverLocal->Reject( Exception );
		 ResolverLocal->Reject( Error );
		 };
		 Container->QueueScoped( OnError );
		 }
		 */
	};
	
	auto& OpenclContext = *mOpenclContext;
	//auto* JobRunner = new TOpenclRunnerLambda( OpenclContext, Kernel, KernelInit, KernelIteration, KernelFinished );
	//std::shared_ptr<PopWorker::TJob> Job(JobRunner);
	//OpenclContext.PushJob( Job );
}
