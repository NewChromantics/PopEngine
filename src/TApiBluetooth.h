#pragma once
#include "TBind.h"
#include "SoyBluetooth.h"

namespace ApiBluetooth
{
	void	Bind(Bind::TContext& Context);
}


/*
extern const char BluetoothDevice_TypeName[];
class TBluetoothDeviceWrapper : public Bind::TObjectWrapper<Bluetooth_TypeName,Bluetooth::TDevice>
{
public:
	TBluetoothDeviceWrapper(Bind::TContext& Context,Bind::TObject& This) :
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
*/

