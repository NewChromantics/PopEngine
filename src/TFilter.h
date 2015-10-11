#pragma once

#include <SoyOpenglContext.h>
#include <SoyFilesystem.h>
#include <TJob.h>
#include <SoyOpencl.h>

class TFilterWindow;
class TFilter;
class TFilterFrame;
class TFilterStageRuntimeData;


static size_t ImageCropLeft = 0;
static size_t ImageCropRight = 0;
static size_t ImageCropTop = 0;
static size_t ImageCropBottom = 0;
/*
static size_t ImageCropLeft = 0;
static size_t ImageCropRight = 100;
static size_t ImageCropTop = 1170;
static size_t ImageCropBottom = 400;
*/


namespace Opengl
{
	class TContext;
}
namespace Opencl
{
	class TContext;
}


class TFilterStage
{
public:
	TFilterStage(const std::string& Name,TFilter& Filter,const TJobParams& StageParams);
	
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)=0;
	virtual bool		SetUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform);
	virtual bool		AllowRender() const
	{
		//	skip rendering frames with no visual output
		if ( mUniforms.HasParam("SkipRender") )
			return false;
		return true;
	}

	bool				operator==(const std::string& Name) const	{	return mName == Name;	}

public:
	SoyEvent<TFilterStage&>	mOnChanged;
	std::string				mName;
	TFilter&				mFilter;
	TJobParams				mUniforms;	//	params specified in config per-stage, which we can relay onto uniforms so we can specify kernel params in the config!
};

class TFilterStageRuntimeData
{
public:
	virtual void				Shutdown(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
	{
	}
	
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,TFilter& Filter,const TJobParams& StageUniforms)
	{
		return false;
	}

	//	gr: rework all of this to extract the system-based image we want (opengl, opencl, cpu) and move all conversions out of stages
	virtual Opengl::TTexture	GetTexture(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl,bool Blocking)
	{
		//	fallback is simple version
		return GetTexture();
	}
	
	virtual Opengl::TTexture				GetTexture()
	{
		return Opengl::TTexture();
	}
	
	virtual std::shared_ptr<SoyPixelsImpl>	GetPixels(Opengl::TContext& ContextGl);		//	default grabs opengl texture and reads pixels
};


class TFilterFrame
{
public:
	TFilterFrame(SoyTime Time) :
		mFrameTime	( Time )
	{
	}
	~TFilterFrame();
	
	bool		Run(TFilter& Filter,const std::string& Description,std::shared_ptr<Opengl::TContext>& ContextGl,std::shared_ptr<Opencl::TContext>& ContextCl);	//	gr: description to avoid passing meta data, like frame timestamp
	
	bool		SetUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,TFilter& Filter,TFilterStage& Stage);

	template<class RUNTIMEDATATYPE>
	RUNTIMEDATATYPE&	GetData(const std::string& StageName)
	{
		auto pData = GetData(StageName);
		if ( !pData )
		{
			std::stringstream Error;
			Error << "Stage data " << StageName << " not found";
			throw Soy::AssertException( Error.str() );
		}
		return dynamic_cast<RUNTIMEDATATYPE&>( *pData );
	}
	
	template<class RUNTIMEDATATYPE>
	std::shared_ptr<RUNTIMEDATATYPE>	AllocData(const std::string& StageName)
	{
		mStageDataLock.lock(std::string("AllocStageData for ") + StageName );
		auto it = mStageData.find( StageName );
		if ( it == mStageData.end() )
		{
			mStageData[StageName].reset( new RUNTIMEDATATYPE() );
			it = mStageData.find( StageName );
		}
		std::shared_ptr<RUNTIMEDATATYPE> Data = std::dynamic_pointer_cast<RUNTIMEDATATYPE>( it->second );
		mStageDataLock.unlock();
		return Data;
	}

private:
	std::shared_ptr<TFilterStageRuntimeData>	GetData(const std::string& StageName);
	std::shared_ptr<Opengl::TContext>		mContextGl;
	std::shared_ptr<Opencl::TContext>		mContextCl;
	
public:
	static bool	SetTextureUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,Opengl::TTexture& Texture,const std::string& TextureName,TFilter& Filter,const TJobParams& StageUniforms);
	static bool	SetTextureUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,const SoyPixelsImpl& Texture,const std::string& TextureName,TFilter& Filter,const TJobParams& StageUniforms);
	static bool	SetTextureUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,const SoyPixelsMeta& Meta,const std::string& TextureName,TFilter& Filter,const TJobParams& StageUniforms);

	//	deprecate the use of these
	Opengl::TTexture				GetFrameTexture(TFilter& Filter,bool Blocking=true);
	std::shared_ptr<SoyPixelsImpl>	GetFramePixels(TFilter& Filter,bool Blocking=true);
	
public:
	Soy::Mutex_Profiled				mRunLock;		//	lock whilst running to avoid being deleted until it's finished
	SoyTime							mFrameTime;
	
	std::map<std::string,std::shared_ptr<TFilterStageRuntimeData>>	mStageData;
	Soy::Mutex_Profiled				mStageDataLock;
	
};

class TFilterStageRuntimeData_Frame : public TFilterStageRuntimeData
{
public:
	virtual void					Shutdown(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	virtual bool					SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform,TFilter& Filter,const TJobParams& StageUniforms) override;
	virtual Opengl::TTexture		GetTexture() override	{	return mTexture ? *mTexture : Opengl::TTexture();	}
	virtual std::shared_ptr<SoyPixelsImpl>	GetPixels(Opengl::TContext& ContextGl) override
	{
		if ( mPixels )
			return mPixels;
		return TFilterStageRuntimeData::GetPixels( ContextGl );
	}
	
	std::shared_ptr<SoyPixelsImpl>	GetPixels(Opengl::TContext& Context,bool Blocking);
	Opengl::TTexture				GetTexture(Opengl::TContext& Context,bool Blocking);
	
public:
	std::shared_ptr<SoyPixelsImpl>		mPixels;
	std::shared_ptr<Opengl::TTexture>	mTexture;
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
	Opengl::TContext&		GetOpenglContext();		//	in the window
	//Opencl::TContext&		GetOpenclContext();
	void					GetOpenclContexts(ArrayBridge<std::shared_ptr<Opencl::TContext>>&& Contexts);
	void					CreateOpenclContexts();
	std::shared_ptr<Opencl::TContext>	PickNextOpenclContext();
	
	void					LoadFrame(std::shared_ptr<SoyPixelsImpl>& Pixels,SoyTime Time);	//	load pixels into [new] frame
	void					OnFrameChanged(SoyTime Frame)	{	Run(Frame);	}

	void					AddStage(const std::string& Name,const TJobParams& Params);
	void					OnStagesChanged();
	void					OnUniformChanged(const std::string& Name);

	void					QueueJob(std::function<void(void)> Function);			//	queue a misc job (off main thread)
	void					PushDevSnapshot(std::shared_ptr<TFilterStageRuntimeData> StageData,const TFilterStage& Stage);

	//	apply uniform to shader
	virtual bool			SetUniform(Soy::TUniformContainer& Shader,const Soy::TUniform& Uniform);
	//	store uniform value
	virtual bool			SetUniform(TJobParam& Param,bool TriggerRerun);
	virtual TJobParam		GetUniform(const std::string& Name);

	std::shared_ptr<TFilterFrame>	GetFrame(SoyTime Frame);
	bool					DeleteFrame(SoyTime Frame);		//	returns false if it was still in use. throws if it doesnt exist
	
	void					CreateBlitGeo(bool Blocking);	//	throws if failed to create (blocking only)
	
public:
	SoyEvent<const SoyTime>							mOnFrameAdded;
	SoyEvent<const SoyTime>							mOnRunCompleted;	//	use for debugging or caching
	std::shared_ptr<TFilterWindow>					mWindow;		//	this also contains our context
	std::map<SoyTime,std::shared_ptr<TFilterFrame>>	mFrames;
	Soy::Mutex_Profiled								mFramesLock;
	Array<std::shared_ptr<TFilterStage>>			mStages;
	std::shared_ptr<Opengl::TGeometry>				mBlitQuad;		//	commonly used
	SoyWorkerJobThread								mOddJobThread;		//	for misc off-main-thread jobs
	std::shared_ptr<Opengl::TContext>				mOpenglContext;
	Array<std::shared_ptr<Opencl::TContext>>		mOpenclContexts;
	Array<std::shared_ptr<Opencl::TDevice>>			mOpenclDevices;
	std::mutex										mOpenclContextLock;
	size_t											mCurrentOpenclContext;
	TJobParams										mUniforms;
	
	SoyWorkerJobThread								mDevSnapshotThread;		//	snapshot writing thread as to not hold up normal processing
	std::string										mDevSnapshotsDir;
};


