#pragma once


#include "TFilterStageOpencl.h"





class TFilterStage_GatherRects : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_GatherRects(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter )
	{
	}
	
	virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data) override;
	
public:
};

class TFilterStageRuntimeData_GatherRects : public TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter) override
	{
		return false;
	}
	virtual Opengl::TTexture	GetTexture() override	{	return Opengl::TTexture();	}
	
public:
	Array<cl_float4>		mRects;
};





class TFilterStage_MakeRectAtlas : public TFilterStage
{
public:
	TFilterStage_MakeRectAtlas(const std::string& Name,const std::string& RectsStage,const std::string& ImageStage,TFilter& Filter) :
		TFilterStage	( Name, Filter ),
		mRectsStage		( RectsStage ),
		mImageStage		( ImageStage )
	{
	}
	
	virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data) override;
	void				CreateBlitResources();
	
public:
	std::string	mRectsStage;
	std::string	mImageStage;

	std::mutex							mBlitResourcesLock;
	std::shared_ptr<Opengl::TShader>	mBlitShader;
	std::shared_ptr<Opengl::TGeometry>	mBlitGeo;
};

class TFilterStageRuntimeData_MakeRectAtlas : public TFilterStageRuntimeData
{
public:
	virtual void				Shutdown(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter) override
	{
		return false;
	}
	virtual Opengl::TTexture	GetTexture() override	{	return mTexture;	}
	
public:
	Opengl::TTexture		mTexture;
};
