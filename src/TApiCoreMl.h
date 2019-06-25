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
	class TObject;
}


extern const char CoreMl_TypeName[];
class TCoreMlWrapper : public Bind::TObjectWrapper<CoreMl_TypeName,CoreMl::TInstance>
{
public:
	TCoreMlWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void 		Construct(Bind::TCallback& Arguments) override;

	void				Yolo(Bind::TCallback& Arguments);
	void				Hourglass(Bind::TCallback& Arguments);
	void				Cpm(Bind::TCallback& Arguments);
	void				OpenPose(Bind::TCallback& Arguments);
	void				SsdMobileNet(Bind::TCallback& Arguments);
	void				MaskRcnn(Bind::TCallback& Arguments);
	void				DeepLab(Bind::TCallback& Arguments);

	//	apple's Vision built-in face detection
	void				FaceDetect(Bind::TCallback& Arguments);

protected:
	std::shared_ptr<CoreMl::TInstance>&		mCoreMl = mObject;
};


class CoreMl::TObject
{
public:
	float			mScore = 0;
	std::string		mLabel;
	Soy::Rectf		mRect = Soy::Rectf(0,0,0,0);
	vec2x<size_t>	mGridPos;
};

