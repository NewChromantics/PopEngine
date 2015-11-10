#include "TFilterStagePrevFrameData.h"
#include "SoyPng.h"


void TFilterStage_PrevFrameData::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	get name of stage we're fetching
	TUniformWrapper<std::string> PrevDataStageName("PrevStage", std::string() );
	if ( !Frame.SetUniform( PrevDataStageName, PrevDataStageName, mFilter, *this ) )
	{
		std::stringstream Error;
		Error << "Failed to get previous-frame-data stage name. Don't know what to fetch";
		throw Soy::AssertException( Error.str() );
	}
	
	//	grab previous frame
	auto& PrevFrame = mFilter.GetPrevFrame( Frame.mFrameTime );
	
	auto PrevFrameStageData = PrevFrame.GetDataPtr<TFilterStageRuntimeData>( PrevDataStageName.mValue );

	Soy::Assert( PrevFrameStageData!=nullptr, "Previous frame doesn't not have this stage data");
	
	Data.reset( new TFilterStageRuntimeData_PrevFrameData( PrevFrameStageData ) );
}

