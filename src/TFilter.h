#pragma once

#include <SoyOpenglContext.h>
#include <SoyFilesystem.h>
#include <TJob.h>
#include <SoyOpencl.h>

class TFilterWindow;
class TFilter;
class TFilterFrame;
class TFilterStageRuntimeData;



class TFilterStage
{
public:
	TFilterStage(const std::string& Name,TFilter& Filter);
	
	virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data)=0;

	bool				operator==(const std::string& Name) const	{	return mName == Name;	}

public:
	SoyEvent<TFilterStage&>	mOnChanged;
	std::string				mName;
	TFilter&				mFilter;
};

class TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter)=0;
	virtual Opengl::TTexture	GetTexture()=0;
};


class TFilterFrame
{
public:
	bool		Run(TFilter& Filter);
	
	bool		SetUniform(Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter);
	std::shared_ptr<TFilterStageRuntimeData>	GetData(const std::string& StageName);

public:
	static bool	SetTextureUniform(Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,Opengl::TTexture& Texture,const std::string& TextureName);

public:
	std::shared_ptr<SoyPixelsImpl>			mFramePixels;
	Opengl::TTexture						mFrameTexture;	//	first input
	
	std::map<std::string,std::shared_ptr<TFilterStageRuntimeData>>	mStageData;
	std::mutex								mStageDataLock;
	
};



class TFilterMeta
{
public:
	TFilterMeta(const std::string& Name) :
		mName	( Name )
	{
	}

	bool					operator==(const std::string& Name) const	{	return mName == Name;	}
	
	std::string				mName;
};





class TFilter : public TFilterMeta
{
public:
	static const char* FrameSourceName;
	
public:
	TFilter(const std::string& Name);
	virtual ~TFilter()		{}
	
	bool					Run(SoyTime Frame);		//	returns true if all stages succeeded
	Opengl::TContext&		GetOpenglContext();			//	in the window
	Opencl::TContext&		GetOpenclContext();			//	in the window
	
	void					LoadFrame(std::shared_ptr<SoyPixelsImpl>& Pixels,SoyTime Time);	//	load pixels into [new] frame
	void					OnFrameChanged(SoyTime Frame)	{	Run(Frame);	}

	void					AddStage(const std::string& Name,const TJobParams& Params);
	void					OnStagesChanged();
	void					OnUniformChanged(const std::string& Name);

	void					QueueJob(std::function<bool(void)> Function);			//	queue a misc job (off main thread)
	
	//	apply uniform to shader
	virtual bool			SetUniform(Soy::TUniformContainer& Shader,Soy::TUniform& Uniform)
	{
		return false;
	}
	//	store uniform value
	virtual bool			SetUniform(TJobParam& Param,bool TriggerRerun)
	{
		throw Soy::AssertException( std::string("No known uniform ")+Param.GetKey() );
	}
	virtual TJobParam		GetUniform(const std::string& Name);

	std::shared_ptr<TFilterFrame>	GetFrame(SoyTime Frame);
	
	void					CreateBlitGeo(bool Blocking);	//	throws if failed to create (blocking only)
	
public:
	SoyEvent<const SoyTime>							mOnRunCompleted;	//	use for debugging or caching
	std::shared_ptr<TFilterWindow>					mWindow;		//	this also contains our context
	std::map<SoyTime,std::shared_ptr<TFilterFrame>>	mFrames;
	Array<std::shared_ptr<TFilterStage>>			mStages;
	std::shared_ptr<Opengl::TGeometry>				mBlitQuad;		//	commonly used
	SoyWorkerJobThread								mJobThread;		//	for misc off-main-thread jobs
	std::shared_ptr<Opengl::TContext>				mOpenglContext;
	std::shared_ptr<Opencl::TContext>				mOpenclContext;
	std::shared_ptr<Opencl::TDevice>				mOpenclDevice;
};

