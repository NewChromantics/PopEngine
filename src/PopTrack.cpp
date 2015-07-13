#include "PopTrack.h"
#include <TParameters.h>
#include <SoyDebug.h>
#include <TProtocolCli.h>
#include <TProtocolHttp.h>
#include <SoyApp.h>
#include <PopMain.h>
#include <TJobRelay.h>
#include <SoyPixels.h>
#include <SoyString.h>
#include <TFeatureBinRing.h>
#include <SortArray.h>
#include <TChannelLiteral.h>
#include <TChannelFile.h>


TPopTrack::TPopTrack() :
	TJobHandler		( static_cast<TChannelManager&>(*this) ),
	TPopJobHandler	( static_cast<TJobHandler&>(*this) )
{
	AddJobHandler("exit", TParameterTraits(), *this, &TPopTrack::OnExit );

	TParameterTraits LoadFrameTraits;
	LoadFrameTraits.mAssumedKeys.PushBack("filter");
	LoadFrameTraits.mAssumedKeys.PushBack("time");
	LoadFrameTraits.mAssumedKeys.PushBack("filename");
	AddJobHandler("LoadFrame", LoadFrameTraits, *this, &TPopTrack::OnLoadFrame );
	
	TParameterTraits AddStageTraits;
	AddStageTraits.mAssumedKeys.PushBack("filter");
	AddStageTraits.mAssumedKeys.PushBack("name");
	AddStageTraits.mAssumedKeys.PushBack("vertfilename");
	AddStageTraits.mAssumedKeys.PushBack("fragfilename");
	AddJobHandler("AddStage", AddStageTraits, *this, &TPopTrack::OnAddStage );
	
	//	all non-default keys are then passed to filter as params to set
	TParameterTraits SetUniformTraits;
	SetUniformTraits.mAssumedKeys.PushBack("filter");
	SetUniformTraits.mRequiredKeys.PushBack("filter");
	AddJobHandler("SetUniform", SetUniformTraits, *this, &TPopTrack::OnSetFilterUniform );
}

bool TPopTrack::AddChannel(std::shared_ptr<TChannel> Channel)
{
	if ( !TChannelManager::AddChannel( Channel ) )
		return false;
	if ( !Channel )
		return false;
	TJobHandler::BindToChannel( *Channel );
	return true;
}


void TPopTrack::OnExit(TJobAndChannel& JobAndChannel)
{
	mConsoleApp.Exit();
	
	//	should probably still send a reply
	TJobReply Reply( JobAndChannel );
	Reply.mParams.AddDefaultParam(std::string("exiting..."));
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}


void TPopTrack::OnLoadFrame(TJobAndChannel &JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	auto Name = Job.mParams.GetParamAs<std::string>("filter");
	auto TimeStr = Job.mParams.GetParamAs<std::string>("time");
	
	//	decode filename to pixels
	auto FilenameParam = Job.mParams.GetParam("filename");
	TJobFormat CastFormat;
	CastFormat.PushFirstContainer<TYPE_Png>();
	CastFormat.PushFirstContainer<TYPE_File>();
	CastFormat.PushFirstContainer<std::string>();
	FilenameParam.Cast(CastFormat);
	std::shared_ptr<SoyPixels> pPixels( new SoyPixels() );
	SoyData_Stack<SoyPixels> Pixels( *pPixels );
	
	if ( !FilenameParam.Decode(Pixels) )
	{
		TJobReply Reply( JobAndChannel );
		Reply.mParams.AddErrorParam("Failed to cast filename to pixels");
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	SoyTime Time( TimeStr );

	auto Filter = GetFilter( Name );
	Filter->LoadFrame( pPixels, Time );

	TJobReply Reply(Job);
	Reply.mParams.AddDefaultParam("loaded frame");
	
	auto& Channel = JobAndChannel.GetChannel();
	Channel.SendJobReply( Reply );
}


void TPopTrack::OnAddStage(TJobAndChannel &JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	auto FilterName = Job.mParams.GetParamAs<std::string>("filter");
	auto Name = Job.mParams.GetParamAs<std::string>("name");
	auto VertFilename = Job.mParams.GetParamAs<std::string>("vertfilename");
	auto FragFilename = Job.mParams.GetParamAs<std::string>("fragfilename");
	
	auto Filter = GetFilter( FilterName );
	Filter->AddStage( Name, VertFilename, FragFilename );
	
	TJobReply Reply(Job);
	Reply.mParams.AddDefaultParam("added stage");
	
	auto& Channel = JobAndChannel.GetChannel();
	Channel.SendJobReply( Reply );
}

void TPopTrack::OnSetFilterUniform(TJobAndChannel &JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	auto FilterName = Job.mParams.GetParamAs<std::string>("filter");

	auto Filter = GetFilter( FilterName );
	
	//	set uniform from all other params
	std::stringstream Error;
	for ( int p=0;	p<Job.mParams.mParams.GetSize();	p++ )
	{
		auto Param = Job.mParams.mParams[p];
		if ( Param.GetKey() == "filter" )
			continue;

		try
		{
			Filter->SetUniform( Param );
		}
		catch( std::exception& e )
		{
			Error << "Error setting uniform " << Param.GetKey() << ": " << e.what() << std::endl;
		}
	}

	TJobReply Reply(Job);
	if ( Error.str().empty() )
		Reply.mParams.AddDefaultParam("set uniforms");
	else
		Reply.mParams.AddErrorParam( Error.str() );
	
	auto& Channel = JobAndChannel.GetChannel();
	Channel.SendJobReply( Reply );
}



std::shared_ptr<TPlayerFilter> TPopTrack::GetFilter(const std::string& Name)
{
	for ( int i=0;	i<mFilters.GetSize();	i++ )
	{
		if ( mFilters[i]->mName == Name )
			return mFilters[i];
	}
	
	//	make new
	std::shared_ptr<TPlayerFilter> Filter( new TPlayerFilter(Name) );
	mFilters.PushBack( Filter );
	
	return Filter;
}

//	keep alive after PopMain()
#if defined(TARGET_OSX_BUNDLE)
std::shared_ptr<TPopTrack> gOpenglApp;
#endif


TPopAppError::Type PopMain(TJobParams& Params)
{
	std::cout << Params << std::endl;
	
	gOpenglApp.reset( new TPopTrack );
	auto& App = *gOpenglApp;

	auto CommandLineChannel = std::shared_ptr<TChan<TChannelLiteral,TProtocolCli>>( new TChan<TChannelLiteral,TProtocolCli>( SoyRef("cmdline") ) );
	
	//	create stdio channel for commandline output
	auto StdioChannel = CreateChannelFromInputString("std:", SoyRef("stdio") );
	auto HttpChannel = CreateChannelFromInputString("http:8080-8090", SoyRef("http") );
	
	
	App.AddChannel( CommandLineChannel );
	App.AddChannel( StdioChannel );
	App.AddChannel( HttpChannel );

	
	
	
	
	//	bootup commands via a channel
	std::shared_ptr<TChannel> BootupChannel( new TChan<TChannelFileRead,TProtocolCli>( SoyRef("Bootup"), "bootup.txt" ) );
	/*
	//	display reply to stdout
	//	when the commandline SENDs a command (a reply), send it to stdout
	auto RelayFunc = [](TJobAndChannel& JobAndChannel)
	{
		std::Debug << JobAndChannel.GetJob().mParams << std::endl;
	};
	//BootupChannel->mOnJobRecieved.AddListener( RelayFunc );
	BootupChannel->mOnJobSent.AddListener( RelayFunc );
	BootupChannel->mOnJobLost.AddListener( RelayFunc );
	*/
	App.AddChannel( BootupChannel );
	


#if !defined(TARGET_OSX_BUNDLE)
	//	run
	App.mConsoleApp.WaitForExit();
#endif

	return TPopAppError::Success;
}




