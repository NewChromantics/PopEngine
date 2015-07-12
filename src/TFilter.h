#pragma once


#include <SoyOpenglContext.h>

class TFilterWindow;



class TFilterShader
{
public:
	TFilterShader(const std::string& Name,const std::string& VertFilename,const std::string& FragFilename);
	void				Reload(Opengl::TContext& Context);
	
public:
	std::string			mName;
	std::string			mVertFilename;
	std::string			mFragFilename;
	Opengl::GlProgram	mShader;
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
	void					Run(SoyTime Frame);

	Opengl::TContext&		GetContext();			//	in the window
	std::shared_ptr<TFilterShader>			GetShader(const std::string& Name);
	
	std::shared_ptr<TFilterWindow>					mWindow;		//	this also contains our context
	std::map<SoyTime,std::shared_ptr<TFilterFrame>>	mFrames;
	Array<std::shared_ptr<TFilterShader>>			mShaders;
	Opengl::TGeometry		mBlitQuad;
};

