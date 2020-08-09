#pragma once
#include "TBind.h"

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;

namespace ApiVision
{
	void	Bind(Bind::TContext& Context);
	DECLARE_BIND_TYPENAME(Model);
	
	class TModelWrapper;
}

namespace PopVision
{
	class TInstance;	//	our-side model manager
	class TObject;		//	output
}


class ApiVision::TModelWrapper : public Bind::TObjectWrapper<BindType::Model,PopVision::TInstance>
{
public:
	TModelWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void 		Construct(Bind::TCallback& Arguments) override;

	void				Yolo(Bind::TCallback& Arguments);
	void				Hourglass(Bind::TCallback& Arguments);
	void				HourglassLabelMap(Bind::TCallback& Arguments);
	void				Cpm(Bind::TCallback& Arguments);
	void				CpmLabelMap(Bind::TCallback& Arguments);
	void				OpenPose(Bind::TCallback& Arguments);
	void				OpenPoseMap(Bind::TCallback& Arguments);		//	deprecated for label map
	void				OpenPoseLabelMap(Bind::TCallback& Arguments);
	void				PosenetLabelMap(Bind::TCallback& Arguments);
	void				SsdMobileNet(Bind::TCallback& Arguments);
	void				MaskRcnn(Bind::TCallback& Arguments);
	void				DeepLab(Bind::TCallback& Arguments);

	//	apple's Vision built-in face detection
	void				AppleVisionFaceDetect(Bind::TCallback& Arguments);

	void				WinSkillSkeleton(Bind::TCallback& Arguments);
	void				KinectAzureSkeleton(Bind::TCallback& Arguments);

protected:
	std::shared_ptr<PopVision::TInstance>&		mModel = mObject;
};



