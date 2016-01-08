#include "TFilterStageWriteGif.h"
#include <Build/PopCastFramework.framework/Headers/TCaster.h>
#include "TFilterStageGatherRects.h"



TFilterStage_WriteGif::TFilterStage_WriteGif(const std::string& Name,TFilter& Filter,const TJobParams& StageParams) :
	TFilterStage	( Name, Filter, StageParams )
{
	//	gr: change GetParam to throw
	if ( !StageParams.GetParamAs("Source", mSourceStage ) )
		throw Soy::AssertException("Missing param Source");
	if ( !StageParams.GetParamAs("Filename", mOutputFilename ) )
		throw Soy::AssertException("Missing param Filename");
	
	auto OpenglContext = Filter.GetOpenglContextPtr();
	
	TCasterParams Params;
	std::stringstream Filename;
	Filename << "file:" << mOutputFilename;
	Params.mName = Filename.str();
	mCastInstance = PopCast::Alloc( Params, OpenglContext );
	Soy::Assert( mCastInstance!=nullptr, "PopCast failed to allocate?");
}

TFilterStage_WriteGif::~TFilterStage_WriteGif()
{
	if ( mCastInstance )
	{
		PopCast::Free( mCastInstance->GetRef() );
		mCastInstance.reset();
	}
}
	
void TFilterStage_WriteGif::Execute(TFilterFrame& Frame,std::shared_ptr<TFilterStageRuntimeData>& Data,Opengl::TContext& ContextGl,Opencl::TContext& ContextCl)
{
	//	grab frame to write
	auto& AtlasData = Frame.GetData<TFilterStageRuntimeData_MakeRectAtlas>( mSourceStage );
	
	//	write on opengl thread
	auto WriteToGif = [this,&AtlasData,&ContextGl]
	{
		Soy::Assert( AtlasData.mTexture.IsValid(), "Expected valid texture from RectAtlas data" );
		mCastInstance->WriteFrame( AtlasData.mTexture, 0 );
	};
	Soy::TSemaphore Semaphore;
	ContextGl.PushJob( WriteToGif, Semaphore );
	Semaphore.Wait();
	
	//	todo: because gif's dont have timecodes, we need to store the frame number we're writing (caster will need to pre-empt this, or store a cache...)
	//	and then do a frame->time meta in the text stream
}



