#pragma once


#include "TFilter.h"



//	store the stage data from a previous frame so we can fetch uniforms from it via the runtime data

class TFilterStage_PrevFrameData : public TFilterStage
{
public:
	TFilterStage_PrevFrameData(const std::string& Name,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage	( Name, Filter, StageParams )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
};



class TFilterStageRuntimeData_PrevFrameData : public TFilterStageRuntimeData
{
public:
	TFilterStageRuntimeData_PrevFrameData(std::shared_ptr<TFilterStageRuntimeData> PrevFrameData) :
		mPrevFrameData	( PrevFrameData )
	{
	}
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,TFilter& Filter,const TJobParams& StageUniforms,Opengl::TContext& ContextGl)
	{
		if ( mPrevFrameData )
			if ( mPrevFrameData->SetUniform( StageName, Shader, Uniform, Filter, StageUniforms, ContextGl ) )
				return true;
		
		return TFilterStageRuntimeData::SetUniform( StageName, Shader, Uniform, Filter, StageUniforms, ContextGl );
	}
	
	virtual Opengl::TTexture	GetTexture(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl,bool Blocking)
	{
		if ( mPrevFrameData )
			return mPrevFrameData->GetTexture( ContextGl, ContextCl, Blocking );

		return TFilterStageRuntimeData::GetTexture( ContextGl, ContextCl, Blocking );
	}
	
	virtual Opengl::TTexture				GetTexture()
	{
		if ( mPrevFrameData )
			return mPrevFrameData->GetTexture();
		return TFilterStageRuntimeData::GetTexture();
	}
	
public:
	std::shared_ptr<TFilterStageRuntimeData>	mPrevFrameData;
};

