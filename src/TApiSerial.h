#pragma once
#include "TBind.h"

namespace Serial
{
	class TComPort;
}

namespace ApiSerial
{
	void	Bind(Bind::TContext& Context);

	DECLARE_BIND_TYPENAME(ComPort);
}



class TSerialComPortWrapper : public Bind::TObjectWrapper<ApiSerial::BindType::ComPort,Serial::TComPort>
{
public:
	TSerialComPortWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Params) override;

	static void					Open(Bind::TCallback& Params);
	static void					Close(Bind::TCallback& Params);
	static void					Read(Bind::TCallback& Params);

	void						OnDataReceived();
	
public:
	std::shared_ptr<Serial::TComPort>&	mComPort = mObject;
	Bind::TPromiseQueue			mReadPromises;
	bool						mDataAsString = false;
};

