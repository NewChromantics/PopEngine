#pragma once

#include <mutex>
#include "SoyLib/src/SoyPixels.h"

class TPixelBuffer;


namespace PopCameraDevice
{
	class TDevice;
	class TStreamMeta;
	
	//	these features are currently all on/off.
	//	but some cameras have options like ISO levels, which we should allow specific numbers of
	namespace TFeature
	{
		enum Type
		{
			AutoFocus,
			AutoWhiteBalance,
		};
	}
}

//	used as desired params when creating camera devices
//	todo: merge/cleanup soy TStreamMeta which has more info (eg. transform matrix)
class PopCameraDevice::TStreamMeta
{
public:
	SoyPixelsMeta	mPixelMeta;
	size_t			mFrameRate = 0;
};



class PopCameraDevice::TDevice
{
public:
	std::shared_ptr<TPixelBuffer>	PopLastFrame(std::string& Meta);
	bool							PopLastFrame(ArrayBridge<uint8_t>& Plane0, ArrayBridge<uint8_t>& Plane1, ArrayBridge<uint8_t>& Plane2,std::string& Meta);
	SoyPixelsMeta					GetMeta() const { return mLastPixelsMeta; }

	virtual void					EnableFeature(TFeature::Type Feature,bool Enable)=0;	//	throws if unsupported
	
protected:
	virtual void					PushFrame(std::shared_ptr<TPixelBuffer> FramePixelBuffer,const SoyPixelsMeta& PixelMeta,const std::string& FrameMeta);

public:
	std::function<void()>			mOnNewFrame;
	
private:
	//	currently storing just last frame
	std::mutex						mLastPixelBufferLock;
	std::shared_ptr<TPixelBuffer>	mLastPixelBuffer;
	std::string						mLastFrameMeta;		//	maybe some key system later, but currently device outputs anything (ideally json)
	SoyPixelsMeta					mLastPixelsMeta;
};

