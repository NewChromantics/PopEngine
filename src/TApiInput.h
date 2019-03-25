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

	extern const char InputDevice_TypeName[];
}



class TInputDeviceWrapper : public Bind::TObjectWrapper<ApiInput::InputDevice_TypeName,Hid::TDevice>
{
public:
	TInputDeviceWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
	{
	}
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Params) override;

	//void						OnStateChanged();
	static void					GetState(Bind::TCallback& Params);

public:
	std::shared_ptr<Hid::TDevice>&			mDevice = mObject;
};

