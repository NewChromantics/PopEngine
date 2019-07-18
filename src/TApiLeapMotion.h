#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"



namespace LeapMotion
{
	class TInput;
}

namespace ApiLeapMotion
{
	void	Bind(Bind::TContext& Context);

	class TInputWrapper;
	DECLARE_BIND_TYPENAME(Input);
}



class ApiLeapMotion::TInputWrapper : public Bind::TObjectWrapper<BindType::Input,LeapMotion::TInput>
{
public:
	TInputWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Params) override;

	void						GetNextFrame(Bind::TCallback& Params);
	void						OnFramesChanged();

public:
	std::shared_ptr<LeapMotion::TInput>&	mInput = mObject;
	Bind::TPromiseQueue			mOnFramePromises;
};

