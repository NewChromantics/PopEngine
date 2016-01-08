#include "TFilterStageWriteAtlasMeta.h"
#include <Build/PopCastFramework.framework/Headers/TCaster.h>
#include "TFilterStageGatherRects.h"
#include <SoySrt.h>


TFilterStage_WriteAtlasMeta::TFilterStage_WriteAtlasMeta(const std::string& Name,TFilter& Filter,const TJobParams& StageParams) :
	TFilterStage	( Name, Filter, StageParams ),
	mFrameNumber	( 0 )
{
	//	gr: change GetParam to throw
	if ( !StageParams.GetParamAs("Source", mSourceStage ) )
		throw Soy::AssertException("Missing param Source");
	if ( !StageParams.GetParamAs("Filename", mOutputFilename ) )
		throw Soy::AssertException("Missing param Filename");
	
	mWriter.reset( new TFileStreamWriter( mOutputFilename ) );
}

TFilterStage_WriteAtlasMeta::~TFilterStage_WriteAtlasMeta()
{
	mWriter->WaitToFinish();
	mWriter.reset();
}


void SerialiseAtlasData_csv(std::stringstream& Out,const TFilterStageRuntimeData_MakeRectAtlas& Data)
{
	//	make a header string
	std::stringstream Header;
	
	auto& SourceRects = Data.mSourceRects;
	auto& DestRects = Data.mDestRects;
	
	Out << "PopTrack=" << SoyTime(true) << ";";
	Out << "Rects=";
	for ( int r=0;	r<SourceRects.GetSize();	r++ )
	{
		Out << SourceRects[r] << ">" << DestRects[r] << "#";
	}
	Out << ";";
}


void TFilterStage_WriteAtlasMeta::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	grab frame to write
	auto& AtlasData = Frame.GetData<TFilterStageRuntimeData_MakeRectAtlas>( mSourceStage );

	//	construct data
	std::stringstream AtlasSerialised;
	SerialiseAtlasData_csv( AtlasSerialised, AtlasData );
	
	auto FrameNumber = mFrameNumber++;
	auto Srt = std::make_shared<Srt::TFrame>();
	Srt->mIndex = FrameNumber;
	Srt->mStart = Frame.mFrameTime;
	Srt->mString = AtlasSerialised.str();
	
	mWriter->Push( Srt );
}



