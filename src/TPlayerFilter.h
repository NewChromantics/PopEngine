#pragma once

#include "TFilter.h"



class TExtractedPlayer
{
public:
	Soy::Rectf	mRect;
	vec3f		mRgb;	//	convert this to actual data based on other uniforms
};
std::ostream& operator<<(std::ostream &out,const TExtractedPlayer& in);

class TExtractedFrame
{
public:
	Array<TExtractedPlayer>	mPlayers;
	SoyTime					mTime;
};


class TWorkThread
{
public:
	TWorkThread(std::shared_ptr<PopWorker::TJob>& Job);
	~TWorkThread();
	
	void				Run();
	bool				IsRunning();
	
private:
	std::shared_ptr<PopWorker::TJob>	mJob;
	std::thread							mThread;
};



class TThreadPool
{
public:
	TThreadPool(size_t MaxThreads);

	//void			Run(std::function<void()> Function);	//	blocks until there's a free thread, then runs and frees again
	void			Run(std::shared_ptr<PopWorker::TJob>& Function);

public:
	Array<std::shared_ptr<TWorkThread>>	mRunThreads;
	size_t					mMaxRunThreads;
	std::mutex				mArrayLock;
};


class TVideoDevice;

class TPlayerFilter : public TFilter, public SoyWorkerThread
{
public:
	TPlayerFilter(const std::string& Name,size_t MaxFrames,size_t MaxRunThreads);
	
	void					Run(SoyTime FrameTime,TJobParams& ResultParams);

	virtual bool			SetUniform(Soy::TUniformContainer& Shader,Soy::TUniform& Uniform) override;
	virtual bool			SetUniform(TJobParam& Param,bool TriggerRerun) override;
	virtual TJobParam		GetUniform(const std::string& Name) override;
	
	virtual bool			Iteration() override;
	void					DeleteOldFrames();
	
	void					SetOnNewVideoEvent(SoyEvent<TVideoDevice>& Event);
	
public:
	TThreadPool				mRunThreads;
	size_t					mMaxFrames;			//	delete frames if more than this
	
	//	uniforms
	int						mCylinderPixelWidth;
	int						mCylinderPixelHeight;
	BufferArray<vec2f,4>	mPitchCorners;
	BufferArray<float,5>	mDistortionParams;
	vec2f					mLensOffset;
	float					mRectMergeMax;
	vec2x<int>				mAtlasSize;
};
