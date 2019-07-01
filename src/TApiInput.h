#pragma once
#include "TBind.h"
#include "SoyOpenglWindow.h"



namespace Hid
{
	class TDevice;
}

namespace ApiInput
{
	void	Bind(Bind::TContext& Context);

	DECLARE_BIND_TYPENAME(Device);
}



class TInputDeviceWrapper : public Bind::TObjectWrapper<ApiInput::BindType::Device,Hid::TDevice>
{
public:
	TInputDeviceWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Params) override;

	//void						OnStateChanged();
	static void					GetState(Bind::TCallback& Params);
	static void					OnStateChanged(Bind::TCallback& Params);

public:
	std::shared_ptr<Hid::TDevice>&			mDevice = mObject;
	Bind::TPromiseQueue			mOnStateChangedPromises;
};

