#pragma once


#include "TFilterStageOpencl.h"





class TFilterStage_GatherHoughTransforms : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_GatherHoughTransforms(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
};

class TFilterStageRuntimeData_GatherHoughTransforms : public TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter) override
	{
		return false;
	}
	virtual Opengl::TTexture	GetTexture() override	{	return Opengl::TTexture();	}
	
public:
	Array<float>				mAngles;
	Array<float>				mDistances;
	Array<cl_int>				mAngleXDistances;	//	[Angle][Distance]=Count
};



