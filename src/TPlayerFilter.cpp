#include "TPlayerFilter.h"
#include "TFilterStageOpengl.h"


TPlayerFilter::TPlayerFilter(const std::string& Name) :
	SoyWorkerThread	( std::string("Player Filter ")+Name, SoyWorkerWaitMode::Wake ),
	TFilter			( Name )
{
	mCylinderPixelWidth = 10;
	mCylinderPixelHeight = 19;
	mPitchCorners.PushBack( vec2f(0.0f,0.0f) );
	mPitchCorners.PushBack( vec2f(0.5f,0.0f) );
	mPitchCorners.PushBack( vec2f(0.5f,0.8f) );
	mPitchCorners.PushBack( vec2f(0.0f,0.8f) );
	mDistortionParams.PushBack(0);
	mDistortionParams.PushBack(0);
	mDistortionParams.PushBack(0);
	mDistortionParams.PushBack(0);
	mDistortionParams.PushBack(0);
	mLensOffset = vec2f(0,0);
	mRectMergeMax = 1;
	mAtlasSize = vec2x<int>( 512, 512 );
	
	
	WakeOnEvent( mOnRunCompleted );
	WakeOnEvent( mOnFrameAdded );
	Start();
}

TJobParam TPlayerFilter::GetUniform(const std::string& Name)
{
	if ( Name == "CylinderPixelWidth" )
		return TJobParam( Name, mCylinderPixelWidth );
	
	if ( Name == "CylinderPixelHeight" )
		return TJobParam( Name, mCylinderPixelHeight );
	
	if ( Name == "RectMergeMax" )
		return TJobParam( Name, mRectMergeMax );
	
	if ( Name == "AtlasWidth" )
		return TJobParam( Name, mAtlasSize.x );
	
	if ( Name == "AtlasHeight" )
		return TJobParam( Name, mAtlasSize.y );
	
	return TFilter::GetUniform(Name);
}

bool TPlayerFilter::SetUniform(Soy::TUniformContainer& Shader,Soy::TUniform& Uniform)
{
	if ( Uniform.mName == "MaskTopLeft" )
	{
		Shader.SetUniform( Uniform, mPitchCorners[0] );
		return true;
	}
	
	if ( Uniform.mName == "MaskTopRight" )
	{
		Shader.SetUniform_s( Uniform.mName, mPitchCorners[1] );
		return true;
	}
	
	if ( Uniform.mName == "MaskBottomRight" )
	{
		Shader.SetUniform_s( Uniform.mName, mPitchCorners[2] );
		return true;
	}
	
	if ( Uniform.mName == "MaskBottomLeft" )
	{
		Shader.SetUniform_s( Uniform.mName, mPitchCorners[3] );
		return true;
	}
	
	if ( Uniform.mName == "RadialDistortionX" )
	{
		Shader.SetUniform_s( Uniform.mName, mDistortionParams[0] );
		return true;
	}
	if ( Uniform.mName == "RadialDistortionY" )
	{
		Shader.SetUniform_s( Uniform.mName, mDistortionParams[1] );
		return true;
	}
	if ( Uniform.mName == "TangentialDistortionX" )
	{
		Shader.SetUniform( Uniform.mName, mDistortionParams[2] );
		return true;
	}
	if ( Uniform.mName == "TangentialDistortionY" )
	{
		Shader.SetUniform( Uniform.mName, mDistortionParams[3] );
		return true;
	}
	if ( Uniform.mName == "K5Distortion" )
	{
		Shader.SetUniform( Uniform.mName, mDistortionParams[4] );
		return true;
	}
	if ( Uniform.mName == "LensOffsetX" )
	{
		Shader.SetUniform( Uniform.mName, mLensOffset.x );
		return true;
	}
	if ( Uniform.mName == "LensOffsetY" )
	{
		Shader.SetUniform( Uniform.mName, mLensOffset.y );
		return true;
	}
	if ( Uniform.mName == "RectMergeMax" )
	{
		Shader.SetUniform( Uniform.mName, mRectMergeMax );
		return true;
	}
	if ( Uniform.mName == "AtlasWidth" )
	{
		Shader.SetUniform( Uniform.mName, mAtlasSize.x );
		return true;
	}
	if ( Uniform.mName == "AtlasHeight" )
	{
		Shader.SetUniform( Uniform.mName, mAtlasSize.y );
		return true;
	}
	return false;
}


bool TPlayerFilter::SetUniform(TJobParam& Param,bool TriggerRerun)
{
	if ( Param.GetKey() == "MaskTopLeft" )
	{
		auto& Var = mPitchCorners[0];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	
	if ( Param.GetKey() == "MaskTopRight" )
	{
		auto& Var = mPitchCorners[1];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	
	if ( Param.GetKey() == "MaskBottomRight" )
	{
		auto& Var = mPitchCorners[2];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	
	if ( Param.GetKey() == "MaskBottomLeft" )
	{
		auto& Var = mPitchCorners[3];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}

	if ( Param.GetKey() == "RadialDistortionX" )
	{
		auto& Var = mDistortionParams[0];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "RadialDistortionY" )
	{
		auto& Var = mDistortionParams[1];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "TangentialDistortionX" )
	{
		auto& Var = mDistortionParams[2];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "TangentialDistortionY" )
	{
		auto& Var = mDistortionParams[3];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "K5Distortion" )
	{
		auto& Var = mDistortionParams[4];
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "LensOffsetX" )
	{
		auto& Var = mLensOffset.x;
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "LensOffsetY" )
	{
		auto& Var = mLensOffset.y;
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "RectMergeMax" )
	{
		auto& Var = mRectMergeMax;
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "AtlasWidth" )
	{
		auto& Var = mAtlasSize.x;
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	if ( Param.GetKey() == "AtlasHeight" )
	{
		auto& Var = mAtlasSize.y;
		Soy::Assert( Param.Decode( Var ), "Failed to decode" );
		if ( TriggerRerun )
			OnUniformChanged( Param.GetKey() );
		return true;
	}
	
	return TFilter::SetUniform( Param, TriggerRerun );
}


void TPlayerFilter::Run(SoyTime FrameTime,TJobParams& ResultParams)
{
	bool AllCompleted = TFilter::Run( FrameTime );
	if ( !Soy::Assert( AllCompleted, "Filter run failed") )
		throw Soy::AssertException("Filter run failed");

	auto FilterFrame = GetFrame(FrameTime);
	if ( !Soy::Assert( FilterFrame!=nullptr, "Missing filter frame") )
		throw Soy::AssertException("Missing filter frame");

	throw Soy::AssertException("todo; extract players from appropriate stage");
}


std::ostream& operator<<(std::ostream &out,const TExtractedPlayer& in)
{
	out << in.mRect.x << 'x' << in.mRect.y << 'x' << in.mRect.w << 'x' << in.mRect.h;
	return out;
}

bool TPlayerFilter::Iteration()
{
	//	check to see if we should delete some frames
	static int MaxFrames = 5;
	while ( mFrames.size() > MaxFrames )
	{
		//	todo: make sure we get oldest
		auto FirstFrame = mFrames.begin();
		if ( FirstFrame == mFrames.end() )
			break;
		
		auto FrameTime = FirstFrame->first;
		//std::Debug << "Deleting frame " << FrameTime << std::endl;
		DeleteFrame( FrameTime );
	}
	
	return true;
}
