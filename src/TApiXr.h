#pragma once
#include "TBind.h"
#include "SoyVector.h"

class TImageWrapper;

namespace Xr
{
	class TDevice;
}

namespace ApiXr
{
	void	Bind(Bind::TContext& Context);
	

	//	gr: this has been renamed to device to match webxr
	//		but if we ever have a system with multiple devices
	//		we might want a session
	//	We may also want an ARKit session with space information, but no device...

	class TDeviceWrapper;
	DECLARE_BIND_TYPENAME(Device);
}

/*
class Xr::TPose
{
public:
	float3	mPosition;
	bool	mTracking = false;
	bool	mConnected = false;
};


class ApiXr::TPoseWrapper
{
public:
	void			OnNewPoses(ArrayBridge<Xr::TPose>&& Poses);
	void			WaitForPoses(Bind::TCallback& Params);

protected:
	Bind::TPromiseQueueObjects<Array<Xr::TPose>>	mPoseQueue;
};

//	spatial maps, detected planes, chaperone boundries
class ApiXr::TSpaceWrapper
{
};

//	Special headset renderers for when we're not just rendering to a window
class ApiXr::THmdWrapper
{
};
*/

class ApiXr::TDeviceWrapper : public Bind::TObjectWrapper<BindType::Device,Xr::TDevice>
{
public:
	TDeviceWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

private: 
	void			CreateDevice();

public:
	std::shared_ptr<Xr::TDevice>&	mDevice = mObject;
};

