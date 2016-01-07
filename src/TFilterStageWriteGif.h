#pragma once


#include "TFilterStageOpencl.h"


class TFilterStage_WriteGif : public TFilterStage
{
public:
	TFilterStage_WriteGif(const std::string& Name,TFilter& Filter,const TJobParams& StageParams);
	~TFilterStage_WriteGif();
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
	void				PushFrameData(const ArrayBridge<uint8>&& FrameData);
	
public:
	std::string							mSourceStage;
	std::string							mOutputFilename;
};

class TFilterStageRuntimeData_WriteGif : public TFilterStageRuntimeData
{
public:
};

