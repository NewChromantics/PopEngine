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
}



extern const char InputDevice_TypeName[];
class TInputDeviceWrapper : public Bind::TObjectWrapper<InputDevice_TypeName,Hid::TDevice>
{
public:
	TInputDeviceWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
	{
	}
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Arguments) override;

	//void						OnStateChanged();
	static void					GetState(Bind::TCallback& Arguments);
	

public:
	std::shared_ptr<Hid::TDevice>&			mDevice = mObject;
};

