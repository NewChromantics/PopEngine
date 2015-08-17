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

class TPlayerFilter : public TFilter, public SoyWorkerThread
{
public:
	TPlayerFilter(const std::string& Name);
	
	void					Run(SoyTime FrameTime,TJobParams& ResultParams);

	virtual bool			SetUniform(Soy::TUniformContainer& Shader,Soy::TUniform& Uniform) override;
	virtual bool			SetUniform(TJobParam& Param,bool TriggerRerun) override;
	virtual TJobParam		GetUniform(const std::string& Name) override;
	
	void					ExtractPlayers(SoyTime FrameTime,TFilterFrame& FilterFrame,TExtractedFrame& ExtractedFrame);
	
	virtual bool			Iteration() override;
	void					DeleteOldFrames();
	
public:
	//	uniforms
	int						mCylinderPixelWidth;
	int						mCylinderPixelHeight;
	BufferArray<vec2f,4>	mPitchCorners;
	BufferArray<float,5>	mDistortionParams;
	vec2f					mLensOffset;
};
