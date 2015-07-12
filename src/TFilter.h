#pragma once


#include <SoyOpenglContext.h>

class TFilterWindow;



class TFilterStage
{
public:
	TFilterStage(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename,const Opengl::TGeometryVertex& BlitVertexDescription);
	void				Reload(Opengl::TContext& Context);
	
	bool				operator==(const std::string& Name) const	{	return mName == Name;	}

public:
	std::string			mName;
	std::string			mVertFilename;
	std::string			mFragFilename;
	Opengl::GlProgram	mShader;
	Opengl::TGeometryVertex	mBlitVertexDescription;
};


class TFilterFrame
{
public:
	Opengl::TTexture						mFrame;				//	first input
	std::map<std::string,Opengl::TTexture>	mShaderTextures;	//	output cache
};



class TFilterMeta
{
public:
	TFilterMeta(const std::string& Name) :
		mName	( Name )
	{
	}

	bool			operator==(const std::string& Name) const	{	return mName == Name;	}
	
	std::string		mName;
};




class TFilter : public TFilterMeta
{
public:
	TFilter(const std::string& Name);
	
	void					LoadFrame(std::shared_ptr<SoyPixels>& Pixels,SoyTime Time);	//	load pixels into [new] frame
	void					AddStage(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename);
	void					Run(SoyTime Frame);

	void					OnFrameChanged(SoyTime Frame)	{	Run(Frame);	}
	void					OnStagesChanged();
	Opengl::TContext&				GetContext();			//	in the window
	
	std::shared_ptr<TFilterWindow>					mWindow;		//	this also contains our context
	std::map<SoyTime,std::shared_ptr<TFilterFrame>>	mFrames;
	Array<std::shared_ptr<TFilterStage>>			mStages;
	Opengl::TGeometry		mBlitQuad;
};

