#include "TFilterStageWriteGif.h"



TFilterStage_WriteGif::TFilterStage_WriteGif(const std::string& Name,TFilter& Filter,const TJobParams& StageParams) :
	TFilterStage	( Name, Filter, StageParams )
{
	//	gr: change GetParam to throw
	if ( !StageParams.GetParamAs("Source", mSourceStage ) )
		throw Soy::AssertException("Missing param Source");
	if ( !StageParams.GetParamAs("Filename", mOutputFilename ) )
		throw Soy::AssertException("Missing param Filename");
}

TFilterStage_WriteGif::~TFilterStage_WriteGif()
{
	
}
	
void TFilterStage_WriteGif::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	grab frame to write
	
	//	write on opengl thread
}
	
void TFilterStage_WriteGif::PushFrameData(const ArrayBridge<uint8>&& FrameData)
{
	
}

