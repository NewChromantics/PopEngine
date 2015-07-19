#pragma once

#include <SoyOpenglContext.h>
#include <SoyFilesystem.h>
#include <TJob.h>


class TFilterWindow;
class TFilter;




class TFilterStage
{
public:
	TFilterStage(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename,const Opengl::TGeometryVertex& BlitVertexDescription,TFilter& Filter);
	void				Reload();
	
	bool				operator==(const std::string& Name) const	{	return mName == Name;	}

public:
	SoyEvent<TFilterStage&>	mOnChanged;
	std::string				mName;
	std::string				mVertFilename;
	std::string				mFragFilename;
	Soy::TFileWatch			mVertFileWatch;
	Soy::TFileWatch			mFragFileWatch;
	Opengl::GlProgram		mShader;
	Opengl::TGeometryVertex	mBlitVertexDescription;
	TFilter&				mFilter;
};


class TFilterFrame
{
public:
	Opengl::TTexture						mFrame;				//	first input
	std::map<std::string,Opengl::TTexture>	mShaderTextures;	//	output cache

	bool		Run(std::ostream& Error,TFilter& Filter);
	
	bool		SetUniform(Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,TFilter& Filter);
	
private:
	bool		SetTextureUniform(Opengl::TShaderState& Shader,Opengl::TUniform& Uniform,Opengl::TTexture& Texture,const std::string& TextureName);
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
	
	void					Run(SoyTime Frame);
	Opengl::TContext&		GetContext();			//	in the window
	
	void					LoadFrame(std::shared_ptr<SoyPixels>& Pixels,SoyTime Time);	//	load pixels into [new] frame
	void					OnFrameChanged(SoyTime Frame)	{	Run(Frame);	}

	void					AddStage(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename);
	void					OnStagesChanged();
	void					OnUniformChanged(const std::string& Name);

	virtual bool			SetUniform(Opengl::TShaderState& Shader,Opengl::TUniform& Uniform)
	{
		return false;
	}
	virtual bool			SetUniform(TJobParam& Param)
	{
		throw Soy::AssertException( std::string("No known uniform ")+Param.GetKey() );
	}

	std::shared_ptr<TFilterWindow>					mWindow;		//	this also contains our context
	std::map<SoyTime,std::shared_ptr<TFilterFrame>>	mFrames;
	Array<std::shared_ptr<TFilterStage>>			mStages;
	Opengl::TGeometry		mBlitQuad;
};


class TPlayerFilter : public TFilter
{
public:
	TPlayerFilter(const std::string& Name);
	
	virtual bool			SetUniform(Opengl::TShaderState& Shader,Opengl::TUniform& Uniform) override;
	virtual bool			SetUniform(TJobParam& Param) override;
	
	BufferArray<vec2f,4>	mPitchCorners;
};
