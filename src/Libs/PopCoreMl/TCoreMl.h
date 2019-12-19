#pragma once

class SoyPixels;
class SoyPixelsImpl;
class TPixelBuffer;

#include "PopCoreMl.h"
#include "Array.hpp"
#include "SoyVector.h"


//	forward declaration
#if defined(__OBJC__)
@class MLMultiArray;
#endif

#if defined(TARGET_WINDOWS)
typedef void* CVPixelBufferRef;
#endif

namespace CoreMl
{
	//	deprecate objects in favour of grids + meta
	class TObject;
	
	class TModel;		//	generic interface
	
	//	CoreML models
	class TYolo;
	class THourglass;
	class TCpm;
	class TOpenPose;
	class TSsdMobileNet;
	class TMaskRcnn;
	class TDeepLab;
	class TResnet50;
	class TPosenet;
	
	//	Apple vision framework
	class TAppleVisionFace;
	
	//	WindowsMl Windows Skills
	class TWinSkillSkeleton;
	
#if defined(__OBJC__)
	void		RunPoseModel(MLMultiArray* ModelOutput,std::function<const std::string&(size_t)> GetKeypointName,std::function<void(const TObject&)>& EnumObject);
	void		RunPoseModel_GetLabelMap(MLMultiArray* ModelOutput,std::function<const std::string&(size_t)> GetKeypointName,std::function<void(vec2x<size_t>,const std::string&,ArrayBridge<float>&&)>& EnumLabelMap);
	void		ExtractFloatsFromMultiArray(MLMultiArray* MultiArray,ArrayBridge<int>&& Dimensions,ArrayBridge<float>&& Values);
	void		ExtractFloatsFromMultiArray(MLMultiArray* MultiArray,ArrayBridge<int>&& Dimensions,ArrayBridge<double>&& Values);
#endif
}


class CoreMl::TObject
{
public:
	float			mScore = 0;
	std::string		mLabel;
	Soy::Rectf		mRect = Soy::Rectf(0,0,0,0);
	vec2x<size_t>	mGridPos;
};


class __exportclass CoreMl::TModel
{
public:
	//	get all the labels this model outputs
	virtual void	GetLabels(ArrayBridge<std::string>&& Labels)=0;
	
	//	find objects in map as rects (with some max scoring... map is probbaly better)
	virtual void	GetObjects(const SoyPixelsImpl& Pixels,std::function<void(const TObject&)>& EnumObject);
	virtual void	GetObjects(CVPixelBufferRef Pixels,std::function<void(const TObject&)>& EnumObject);
	
	//	draw found labels on a map
	//	maybe callback says which component/value should go for label
	virtual void	GetLabelMap(const SoyPixelsImpl& Pixels,std::shared_ptr<SoyPixelsImpl>& MapOutput,std::function<bool(const std::string&)>& FilterLabel);
	virtual void	GetLabelMap(CVPixelBufferRef Pixels,std::shared_ptr<SoyPixelsImpl>& MapOutput,std::function<bool(const std::string&)>& FilterLabel);

	virtual void	GetLabelMap(const SoyPixelsImpl& Pixels,std::function<void(vec2x<size_t>,const std::string&,ArrayBridge<float>&&)> EnumLabelMap);
	virtual void	GetLabelMap(CVPixelBufferRef Pixels,std::function<void(vec2x<size_t>,const std::string&,ArrayBridge<float>&&)>& EnumLabelMap);
};

//	C++ factory for dll
//	todo: make this a proper CAPI for unity etc, and some nice dumb interface (instance ID etc)
__export CoreMl::TModel*	PopCoreml_AllocModel(const std::string& Name);
__export int32_t			PopCoreml_GetVersion();



