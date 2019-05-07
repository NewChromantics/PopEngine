#pragma once
#include "TBind.h"

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;

namespace ApiCoreMl
{
	void	Bind(Bind::TContext& Context);
}

namespace CoreMl
{
	class TInstance;
}


extern const char CoreMl_TypeName[];
class TCoreMlWrapper : public Bind::TObjectWrapper<CoreMl_TypeName,CoreMl::TInstance>
{
public:
	TCoreMlWrapper(Bind::TLocalContext& Context,Bind::TObject& This) :
		TObjectWrapper	( Context, This )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void 		Construct(Bind::TCallback& Arguments) override;

	static void			Yolo(Bind::TCallback& Arguments);
	static void			Hourglass(Bind::TCallback& Arguments);
	static void			Cpm(Bind::TCallback& Arguments);
	static void			OpenPose(Bind::TCallback& Arguments);
	static void			SsdMobileNet(Bind::TCallback& Arguments);
	static void			MaskRcnn(Bind::TCallback& Arguments);

	//	apple's Vision built-in face detection
	static void			FaceDetect(Bind::TCallback& Arguments);

protected:
	std::shared_ptr<CoreMl::TInstance>&		mCoreMl = mObject;
};

