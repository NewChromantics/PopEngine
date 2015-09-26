#pragma once

#include "TFilter.h"



class TFilterStage_ShaderBlit : public TFilterStage
{
public:
	TFilterStage_ShaderBlit(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename,const Opengl::TGeometryVertex& BlitVertexDescription,TFilter& Filter,const TJobParams& StageParams);
	
	void				Reload();
	virtual void		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	
	bool				operator==(const std::string& Name) const	{	return mName == Name;	}
	
public:
	std::string				mVertFilename;
	std::string				mFragFilename;
	Soy::TFileWatch			mVertFileWatch;
	Soy::TFileWatch			mFragFileWatch;
	std::shared_ptr<Opengl::TShader>	mShader;
	Opengl::TGeometryVertex	mBlitVertexDescription;
};

class TFilterStageRuntimeData_ShaderBlit : public TFilterStageRuntimeData
{
public:
	virtual void				Shutdown(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl) override;
	virtual bool				SetUniform(const std::string& StageName,Soy::TUniformContainer& Shader,Soy::TUniform& Uniform,TFilter& Filter) override;
	virtual Opengl::TTexture	GetTexture(Opengl::TContext& ContextGl,Opencl::TContext& ContextCl,bool Blocking) override;
	virtual Opengl::TTexture	GetTexture() override	{	return mTexture;	}
	
public:
	Soy::Mutex_Profiled				mConversionLock;		//	rarely need this but
	Opengl::TTexture		mTexture;
	std::shared_ptr<Opencl::TBufferImage>	mImageBuffer;
};



