#pragma once


#include "TFilter.h"





class TFilterStage_WritePng : public TFilterStage
{
public:
	TFilterStage_WritePng(const std::string& Name,const std::string& OutputFilename,const std::string& ImageStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage	( Name, Filter, StageParams ),
		mFilename		( OutputFilename ),
		mImageStage		( ImageStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mFilename;
	std::string			mImageStage;
};


class TFilterStage_ReadPng : public TFilterStage
{
public:
	TFilterStage_ReadPng(const std::string& Name,const std::string& Filename,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage	( Name, Filter, StageParams ),
		mFilename		( Filename )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mFilename;
};

