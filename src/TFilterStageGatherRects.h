#pragma once


#include "TFilterStageOpencl.h"





class TFilterStage_GatherRects : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_GatherRects(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
};

class TFilterStageRuntimeData_GatherRects : public TFilterStageRuntimeData
{
public:
	Array<cl_float4>		mRects;
};



class TFilterStage_DistortRects : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_DistortRects(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,const std::string& MinMaxDataStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams ),
		mMinMaxDataStage			( MinMaxDataStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string		mMinMaxDataStage;
};

class TFilterStageRuntimeData_DistortRects : public TFilterStageRuntimeData_GatherRects
{
};


class TFilterStage_DrawMinMax : public TFilterStage_OpenclKernel
{
public:
	TFilterStage_DrawMinMax(const std::string& Name,const std::string& KernelFilename,const std::string& KernelName,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage_OpenclKernel	( Name, KernelFilename, KernelName, Filter, StageParams )
	{
		StageParams.GetParamAs("MinMaxSource",mMinMaxDataStage);
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
public:
	std::string		mMinMaxDataStage;
};



class TFilterStage_MakeRectAtlas : public TFilterStage
{
public:
	TFilterStage_MakeRectAtlas(const std::string& Name,const std::string& RectsStage,const std::string& ImageStage,const std::string& MaskStage,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage	( Name, Filter, StageParams ),
		mRectsStage		( RectsStage ),
		mImageStage		( ImageStage ),
		mMaskStage		( MaskStage )
	{
	}
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	void				CreateBlitResources();
	
public:
	std::string	mRectsStage;
	std::string	mImageStage;
	std::string	mMaskStage;

	std::mutex							mBlitResourcesLock;
	std::shared_ptr<Opengl::TShader>	mBlitShader;
	std::shared_ptr<Opengl::TGeometry>	mBlitGeo;
};

class TFilterStageRuntimeData_MakeRectAtlas : public TFilterStageRuntimeData
{
public:
	virtual void				Shutdown(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
	virtual Opengl::TTexture	GetTexture() override	{	return mTexture;	}
	
public:
	Array<Soy::Rectf>		mSourceRects;
	Array<Soy::Rectf>		mDestRects;
	Opengl::TTexture		mTexture;
};



//#define USE_STREAM

class TWriteFileStream : public SoyWorkerThread
{
public:
	TWriteFileStream(const std::string& Filename);
	~TWriteFileStream();
	
	virtual bool	Iteration() override;
	virtual bool	CanSleep() override;

	void			PushData(const ArrayBridge<uint8>& Data);
	
public:
#if defined(USE_STREAM)
	std::shared_ptr<std::ofstream>	mStream;
#else
	FILE*			mFile;
#endif
	Soy::Mutex_Profiled		mPendingDataLock;
	Array<uint8>	mPendingData;
	Array<uint8>	mStaticWriteBuffer;
};

class TFilterStage_WriteRectAtlasStream : public TFilterStage
{
public:
	TFilterStage_WriteRectAtlasStream(const std::string& Name,const std::string& AtlasStage,const std::string& OutputFilename,TFilter& Filter,const TJobParams& StageParams) :
		TFilterStage	( Name, Filter, StageParams ),
		mAtlasStage		( AtlasStage ),
		mOutputFilename	( OutputFilename )
	{
	}
	~TFilterStage_WriteRectAtlasStream();
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
	void				PushFrameData(const ArrayBridge<uint8>&& FrameData);
	
public:
	std::mutex							mWriteThreadLock;
	std::shared_ptr<TWriteFileStream>	mWriteThread;
	std::string							mAtlasStage;
	std::string							mOutputFilename;
};

class TFilterStageRuntimeData_WriteRectAtlasStream : public TFilterStageRuntimeData
{
public:
};

