#pragma once

#include "TFilter.h"



class TFilterStage_ShaderBlit : public TFilterStage
{
public:
	TFilterStage_ShaderBlit(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename,const Opengl::TGeometryVertex& BlitVertexDescription,TFilter& Filter);
	
	void				Reload();
	virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data) override;
	
	bool				operator==(const std::string& Name) const	{	return mName == Name;	}
	
public:
	std::string				mVertFilename;
	std::string				mFragFilename;
	Soy::TFileWatch			mVertFileWatch;
	Soy::TFileWatch			mFragFileWatch;
	Opengl::GlProgram		mShader;
	Opengl::TGeometryVertex	mBlitVertexDescription;
};

class TFilterStageRuntimeData_ShaderBlit : public TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,TFilter& Filter) override;
	virtual Opengl::TTexture	GetTexture() override	{	return mTexture;	}

public:
	Opengl::TTexture		mTexture;
};






class TFilterStage_ReadPixels : public TFilterStage
{
public:
	TFilterStage_ReadPixels(const std::string& Name,const std::string& SourceStage,TFilter& Filter);

	virtual bool		Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data);
	
public:
	std::string			mSourceStage;
};

class TFilterStageRuntimeData_ReadPixels : public TFilterStageRuntimeData
{
public:
	virtual bool				SetUniform(const std::string& StageName,Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,TFilter& Filter) override;
	virtual Opengl::TTexture	GetTexture() override	{	return Opengl::TTexture();	}
	
public:
	SoyPixels			mPixels;
};
