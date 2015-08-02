#pragma once


#include "TFilter.h"


class TFilterStage_OpenclKernel : public TFilterStage
{
public:
	TFilterStage_OpenclKernel(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter);
	
	void				Reload();
	virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data) override;
	
	bool				operator==(const std::string& Name) const	{	return mName == Name;	}
	
public:
	std::string				mKernelFilename;
	std::string				mKernelName;
	Soy::TFileWatch			mKernelFileWatch;
	std::shared_ptr<Opencl::TKernel>	mShader;
};

class TFilterStageRuntimeData_OpenclKernel : public TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,TFilter& Filter) override;
	virtual Opengl::TTexture	GetTexture() override	{	return mTexture;	}
	
public:
	//	opencl output
	Opengl::TTexture		mTexture;
};


