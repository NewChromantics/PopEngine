#pragma once
#include "TBind.h"
#include "SoyBluetooth.h"

namespace ApiBluetooth
{
	void	Bind(Bind::TContext& Context);
}



extern const char BluetoothDevice_TypeName[];
class TBluetoothDeviceWrapper : public Bind::TObjectWrapper<BluetoothDevice_TypeName,Bluetooth::TDevice>
{
public:
	TBluetoothDeviceWrapper(Bind::TContext& Context,Bind::TObject& This) :
		TObjectWrapper			( Context, This )
	{
	}
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Arguments) override;

	static void					Connect(Bind::TCallback& Arguments);
	static void					ReadCharacteristic(Bind::TCallback& Arguments);

	void						OnStateChanged();
	void						OnRecvData(const std::string& Characteristic,ArrayBridge<uint8_t>&& NewData);
	
public:
	std::shared_ptr<Bluetooth::TDevice>&	mDevice = mObject;
	Bind::TPromiseQueue			mConnectPromises;
	
	//	currently supporting one
	Bind::TPromiseQueue			mReadCharacteristicPromises;
	std::mutex					mReadCharacteristicBufferLock;
	std::string					mReadCharacteristicUuid;
	Array<uint8_t>				mReadCharacteristicBuffer;
};


