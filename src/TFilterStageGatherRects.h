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
	TFilterStage_MakeRectAtlas(const std::string& Name,const std::string& RectsStage,const std::string& ImageStage,const std::string& MaskStage,TFilter& Filter) :
		TFilterStage	( Name, Filter ),
		mRectsStage		( RectsStage ),
		mImageStage		( ImageStage ),
		mMaskStage		( MaskStage )
	{
	}
	
	virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data) override;
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
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter) override
	{
		return false;
	}
	virtual Opengl::TTexture	GetTexture() override	{	return mTexture;	}
	
public:
	Array<Soy::Rectf>		mRects;		//	rects on the texture
	Opengl::TTexture		mTexture;
};





class TWriteFileStream : public SoyWorkerThread
{
public:
	TWriteFileStream(const std::string& Filename);
	~TWriteFileStream();
	
	virtual bool	Iteration() override;
	virtual bool	CanSleep() override;

	void			PushData(const ArrayBridge<uint8>& Data);
	
public:
	std::shared_ptr<std::ofstream>	mStream;
	std::mutex		mPendingDataLock;
	Array<uint8>	mPendingData;
};

class TFilterStage_WriteRectAtlasStream : public TFilterStage
{
public:
	TFilterStage_WriteRectAtlasStream(const std::string& Name,const std::string& AtlasStage,const std::string& OutputFilename,TFilter& Filter) :
		TFilterStage	( Name, Filter ),
		mAtlasStage		( AtlasStage ),
		mOutputFilename	( OutputFilename )
	{
	}
	~TFilterStage_WriteRectAtlasStream();
	
	virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data) override;
	
	void				PushFrameData(const ArrayBridge<uint8>&& FrameData);
	
public:
	std::shared_ptr<TWriteFileStream>	mWriteThread;
	std::string							mAtlasStage;
	std::string							mOutputFilename;
};

class TFilterStageRuntimeData_WriteRectAtlasStream : public TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter) override
	{
		return false;
	}
};

