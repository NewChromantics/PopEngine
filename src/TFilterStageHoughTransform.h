#pragma once


#include "TFilterStageOpencl.h"





class TFilterStage_GatherHoughTransforms : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_GatherHoughTransforms(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
};

class TFilterStageRuntimeData_GatherHoughTransforms : public TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,TFilter& Filter) override
	{
		return false;
	}
	virtual Opengl::TTexture	GetTexture() override	{	return Opengl::TTexture();	}
	
public:
	Array<float>				mAngles;
	Array<float>				mDistances;
	Array<cl_int>				mAngleXDistances;	//	[Angle][Distance]=Count
};




class TFilterStage_ExtractHoughLines : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_ExtractHoughLines(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HoughDataStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mHoughDataStage				( HoughDataStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mHoughDataStage;
};


class TFilterStageRuntimeData_ExtractHoughLines : public TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,TFilter& Filter) override
	{
		return false;
	}
	virtual Opengl::TTexture	GetTexture() override	{	return Opengl::TTexture();	}
	
public:
	Array<cl_float8>			mHoughLines;	//	x0,y0,x1,y1,angle,distance,score,0
};




class TFilterStage_DrawHoughLinesDynamic : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_DrawHoughLinesDynamic(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HoughDataStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mHoughDataStage				( HoughDataStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mHoughDataStage;
};





class TFilterStage_DrawHoughLines : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_DrawHoughLines(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HoughLineDataStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mHoughLineDataStage			( HoughLineDataStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mHoughLineDataStage;
};



