#pragma once
#include "TBind.h"
#include "SoyBluetooth.h"

namespace ApiBluetooth
{
	void	Bind(Bind::TContext& Context);

	static const char BluetoothDevice_TypeName[] = "BluetoothDevice";
}


namespace Bluetooth
{
	class TDeviceHandle;
}


class Bluetooth::TDeviceHandle
{
public:
	TState::Type			GetState()	{	return mGetState();	}
	void					PopData(ArrayBridge<uint8_t>&& Data);
	
public:
	std::string				mUuid;
	std::function<void()>	mOnStateChanged;
	std::function<void()>	mOnRecv;
	std::function<TState::Type()>	mGetState;
};

class TBluetoothDeviceWrapper : public Bind::TObjectWrapper<ApiBluetooth::BluetoothDevice_TypeName,Bluetooth::TDeviceHandle>
{
public:
	TBluetoothDeviceWrapper(Bind::TContext& Context) :
		TObjectWrapper			( Context )
	{
	}
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Arguments) override;

	static void					Connect(Bind::TCallback& Arguments);
	static void					ReadCharacteristic(Bind::TCallback& Arguments);

	void						OnStateChanged();
	void						OnRecvData(const std::string& Characteristic,ArrayBridge<uint8_t>&& NewData);
	
public:
	std::shared_ptr<Bluetooth::TDeviceHandle>&	mDevice = mObject;
	Bind::TPromiseQueue			mConnectPromises;
	
	//	currently supporting one
	Bind::TPromiseQueue			mReadCharacteristicPromises;
	std::mutex					mReadCharacteristicBufferLock;
	std::string					mReadCharacteristicUuid;
	Array<uint8_t>				mReadCharacteristicBuffer;
};


