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


class TFilterStageRuntimeData_HoughLines : public TFilterStageRuntimeData
{
public:
	//	x0,y0,x1,y1,angle,distance,score,Vertical!=0
	//	gr: store as 2 arrays or one?
	//Array<cl_float8>			mHoughLines;
	Array<cl_float8>			mVertLines;
	Array<cl_float8>			mHorzLines;
};



class TFilterStage_GetTruthLines : public TFilterStage
{
public:
	TFilterStage_GetTruthLines(const std::string& Name,const std::string& VerticalLinesUniform,const std::string& HorzLinesUniform,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage				( Name, Filter, StageParams ),
		mVerticalLinesUniform		( VerticalLinesUniform ),
		mHorzLinesUniform			( HorzLinesUniform )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mVerticalLinesUniform;
	std::string			mHorzLinesUniform;
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



class TFilterStage_DrawHoughCorners : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_DrawHoughCorners(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HoughCornerDataStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mHoughCornerDataStage		( HoughCornerDataStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mHoughCornerDataStage;
};





class TFilterStage_ExtractHoughCorners : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_ExtractHoughCorners(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HoughLineStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mHoughLineStage				( HoughLineStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mHoughLineStage;
};


class TFilterStageRuntimeData_ExtractHoughCorners : public TFilterStageRuntimeData
{
public:
	
public:
	Array<cl_float4>			mCorners;	//	x,y,score,0
};





class TFilterStage_GetHoughCornerHomographys : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_GetHoughCornerHomographys(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HoughCornerStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mHoughCornerStage			( HoughCornerStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	Array<cl_float2>	mTruthCorners;
	std::string			mHoughCornerStage;
};


class TFilterStage_GetHoughLineHomographys : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_GetHoughLineHomographys(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HoughLineStage,const std::string& TruthLineStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mTruthLineStage				( TruthLineStage ),
		mHoughLineStage				( HoughLineStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mTruthLineStage;
	std::string			mHoughLineStage;
};


class TFilterStage_ScoreHoughCornerHomographys : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_ScoreHoughCornerHomographys(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HomographyDataStage,const std::string& CornerDataStage,const std::string& TruthCornerDataStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mHomographyDataStage		( HomographyDataStage ),
		mHoughCornerDataStage		( CornerDataStage ),
		mTruthCornerDataStage		( TruthCornerDataStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mHomographyDataStage;
	std::string			mHoughCornerDataStage;
	std::string			mTruthCornerDataStage;
};



class TFilterStageRuntimeData_GetHoughCornerHomographys : public TFilterStageRuntimeData
{
public:
	
public:
	Array<cl_float16>	mHomographys;	//	3x3. last 7 ignored
	Array<cl_float16>	mHomographyInvs;	//	3x3. last 7 ignored
};




class TFilterStage_DrawHomographyCorners : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_DrawHomographyCorners(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& HoughCornerDataStage,const std::string& TruthCornerDataStage,const std::string& HomographyDataStage,TFilter& Filter,const TJobParams& StageParams) :
	TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
	mHoughCornerDataStage		( HoughCornerDataStage ),
	mTruthCornerDataStage		( TruthCornerDataStage ),
	mHomographyDataStage		( HomographyDataStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mHoughCornerDataStage;
	std::string			mTruthCornerDataStage;
	std::string			mHomographyDataStage;
};



class TFilterStage_DrawMaskOnFrame : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_DrawMaskOnFrame(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& MaskStage,const std::string& HomographyDataStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mMaskStage					( MaskStage ),
		mHomographyDataStage		( HomographyDataStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string			mMaskStage;
	std::string			mHomographyDataStage;
};






