#pragma once
#include "TBind.h"
#include "SoyVector.h"

class TImageWrapper;

namespace Xr
{
	class TPose;
	class TSession;
}

namespace ApiXr
{
	void	Bind(Bind::TContext& Context);


	class TPoseWrapper;
	class TSpatialWrapper;
	class THmdWrapper;

	//	might be better as device?
	class TSessionWrapper;
	DECLARE_BIND_TYPENAME(Session);
}


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


class ApiXr::TSessionWrapper : public Bind::TObjectWrapper<BindType::Session,Xr::TSession>, public TPoseWrapper, public TSpaceWrapper, public THmdWrapper
{
public:
	TSessionWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void		CreateTemplate(Bind::TTemplate& Template);
	virtual void 	Construct(Bind::TCallback& Params) override;

public:
	std::shared_ptr<Xr::TSession>&	mSession = mObject;
	TPoseWrapper	mPoseWrapper;
};

