#pragma once


#include "TFilterStageOpencl.h"
#include <Build/PopCastFramework.framework/Headers/PopCast.h>


class TFilterStage_WriteGif : public TFilterStage
{
public:
	TFilterStage_WriteGif(const std::string& Name,TFilter& Filter,const TJobParams& StageParams);
	~TFilterStage_WriteGif();
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mSourceStage;
	std::string			mOutputFilename;

	std::shared_ptr<PopCast::TInstance>	mCastInstance;
};

class TFilterStageRuntimeData_WriteGif : public TFilterStageRuntimeData
{
public:
};

