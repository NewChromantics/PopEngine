#pragma once

#include "TFilter.h"
#include <SoyStream.h>

class TFilterStage_WriteAtlasMeta : public TFilterStage
{
public:
	TFilterStage_WriteAtlasMeta(const std::string& Name,TFilter& Filter,const TJobParams& StageParams);
	~TFilterStage_WriteAtlasMeta();
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mSourceStage;
	std::string			mOutputFilename;

	std::atomic<size_t>	mFrameNumber;		//	replace this with a proper PopMovie framecounter/refernece
	std::shared_ptr<TStreamWriter>	mWriter;
};
