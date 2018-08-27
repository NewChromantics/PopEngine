#pragma once

#include "v8-inspector.h"
#include "TApiWebsocket.h"
#include "TApiHttp.h"



class TV8Inspector //: public v8_inspector::V8Inspector::Channel
{
public:
	TV8Inspector(v8::Isolate& Isolate,v8::Local<v8::Context> Context);
	
protected:
	void	OnMessage(const std::string& Message);
	void	OnMessage(const Array<uint8_t>& Message);
	
	void 	OnDiscoveryRequest(const std::string& Url,Http::TResponseProtocol& Request);
	
	/*
	TV8Inspector(TaskRunner* task_runner, int context_group_id,
						v8::Isolate* isolate, v8::Local<v8::Function> function)
	: task_runner_(task_runner),
	context_group_id_(context_group_id),
	function_(isolate, function) {}
	virtual ~FrontendChannelImpl() = default;
	
	void set_session_id(int session_id) { session_id_ = session_id; }
	
private:
	void sendResponse(
					  int callId,
					  std::unique_ptr<v8_inspector::StringBuffer> message) override {
		task_runner_->Append(
							 new SendMessageTask(this, ToVector(message->string())));
	}
	void sendNotification(
						  std::unique_ptr<v8_inspector::StringBuffer> message) override {
		task_runner_->Append(
							 new SendMessageTask(this, ToVector(message->string())));
	}
	void flushProtocolNotifications() override {}
	
	class SendMessageTask : public TaskRunner::Task {
	public:
		SendMessageTask(FrontendChannelImpl* channel,
						const std::vector<uint16_t>& message)
		: channel_(channel), message_(message) {}
		virtual ~SendMessageTask() {}
		bool is_priority_task() final { return false; }
		
	private:
		void Run(IsolateData* data) override {
			v8::MicrotasksScope microtasks_scope(data->isolate(),
												 v8::MicrotasksScope::kRunMicrotasks);
			v8::HandleScope handle_scope(data->isolate());
			v8::Local<v8::Context> context =
			data->GetContext(channel_->context_group_id_);
			v8::Context::Scope context_scope(context);
			v8::Local<v8::Value> message = ToV8String(data->isolate(), message_);
			v8::MaybeLocal<v8::Value> result;
			result = channel_->function_.Get(data->isolate())
			->Call(context, context->Global(), 1, &message);
		}
		FrontendChannelImpl* channel_;
		std::vector<uint16_t> message_;
	};
	
	TaskRunner* task_runner_;
	int context_group_id_;
	v8::Global<v8::Function> function_;
	int session_id_;
	DISALLOW_COPY_AND_ASSIGN(FrontendChannelImpl);
	 */
private:
	v8::Isolate&	mIsolate;
	std::shared_ptr<THttpServer>		mDiscoveryServer;	//	chrome connects to this to find inspectors
	std::shared_ptr<TWebsocketServer>	mWebsocketServer;
	std::string		mUuid;
};


