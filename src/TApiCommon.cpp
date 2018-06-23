#include "TApiCommon.h"
#include <SoyDebug.h>

const char Log_FunctionName[] = "log";

static v8::Local<v8::Value> OnLog(v8::CallbackInfo& Params);


void ApiCommon::Bind(TV8Container& Container)
{
	//  load api's before script & executions
	Container.BindGlobalFunction<Log_FunctionName>(OnLog);
	

}

static v8::Local<v8::Value> OnLog(v8::CallbackInfo& Params)
{
	using namespace v8;

	auto& args = Params.mParams;
	
	if (args.Length() < 1)
	{
		throw Soy::AssertException("log() with no args");
	}
	
	HandleScope scope(Params.mIsolate);
	for ( auto i=0;	i<args.Length();	i++ )
	{
		auto arg = args[i];
		String::Utf8Value value(arg);
		std::Debug << *value << std::endl;
	}
	
	return v8::Undefined(Params.mIsolate);
}
