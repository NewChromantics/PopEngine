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


#define FILTER_MAX_FRAMES	10
#define FILTER_MAX_THREADS	1
#define JOB_THREAD_COUNT	1

TPopTrack::TPopTrack() :
	TJobHandler			( static_cast<TChannelManager&>(*this), JOB_THREAD_COUNT ),
	TPopJobHandler		( static_cast<TJobHandler&>(*this) ),
	mSubcriberManager	( static_cast<TChannelManager&>(*this) )
{
	TParameterTraits DecodeParameterTraits;
	DecodeParameterTraits.mAssumedKeys.PushBack("filter");
	DecodeParameterTraits.mAssumedKeys.PushBack("filename");
	AddJobHandler("decode", DecodeParameterTraits, *this, &TPopTrack::OnStartDecode );
	
	AddJobHandler("exit", TParameterTraits(), *this, &TPopTrack::OnExit );

	TParameterTraits AddStageTraits;
	AddStageTraits.mAssumedKeys.PushBack("filter");
	AddStageTraits.mAssumedKeys.PushBack("name");
	AddJobHandler("AddStage", AddStageTraits, *this, &TPopTrack::OnAddStage );
	
	//	all non-default keys are then passed to filter as params to set
	TParameterTraits SetUniformTraits;
	SetUniformTraits.mAssumedKeys.PushBack("filter");
	SetUniformTraits.mRequiredKeys.PushBack("filter");
	AddJobHandler("SetUniform", SetUniformTraits, *this, &TPopTrack::OnSetFilterUniform );
	
	TParameterTraits RunFilterTraits;
	RunFilterTraits.mAssumedKeys.PushBack("filter");
	RunFilterTraits.mRequiredKeys.PushBack("filter");
	RunFilterTraits.mAssumedKeys.PushBack("time");
	RunFilterTraits.mRequiredKeys.PushBack("time");
	AddJobHandler("Run", RunFilterTraits, *this, &TPopTrack::OnRunFilter );
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
	
	auto PixelsParam = Job.mParams.GetParam("pixels");

	std::shared_ptr<SoyPixelsImpl> PixelsImpl;
	if ( PixelsParam.IsValid() )
	{
		std::shared_ptr<SoyPixels> Pixels( new SoyPixels );
		SoyData_Stack<SoyPixels> PixelsData( *Pixels );
		
		if ( !PixelsParam.Decode(PixelsData) )
		{
			TJobReply Reply( JobAndChannel );
			Reply.mParams.AddErrorParam("Failed to get pixels");
			TChannel& Channel = JobAndChannel;
			Channel.OnJobCompleted( Reply );
			return;
		}
		PixelsImpl = Pixels;
	}
	else
	{
		//	decode filename to pixels
		auto FilenameParam = Job.mParams.GetParam("filename");
		TJobFormat CastFormat;
		CastFormat.PushFirstContainer<TYPE_Png>();
		CastFormat.PushFirstContainer<TYPE_File>();
		CastFormat.PushFirstContainer<std::string>();
		FilenameParam.Cast(CastFormat);
		std::shared_ptr<SoyPixels> Pixels( new SoyPixels );
		SoyData_Stack<SoyPixels> PixelsData( *Pixels );
		
		if ( !FilenameParam.Decode(PixelsData) )
		{
			TJobReply Reply( JobAndChannel );
			Reply.mParams.AddErrorParam("Failed to cast filename to pixels");
			TChannel& Channel = JobAndChannel;
			Channel.OnJobCompleted( Reply );
			return;
		}
		
		PixelsImpl = Pixels;
	}

	SoyTime Time( TimeStr );
	auto Filter = GetFilter( Name );

	Filter->LoadFrame( PixelsImpl, Time );

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
	
	TJobReply Reply(Job);
	try
	{
		auto Filter = GetFilter( FilterName );
		Filter->AddStage( Name, Job.mParams );
		
		Reply.mParams.AddDefaultParam("added stage");
	}
	catch(std::exception& e)
	{
		std::stringstream Error;
		Error << "Error adding stage: " << e.what();
		Reply.mParams.AddErrorParam( Error.str() );
	}

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
	bool Changed = false;
	for ( int p=0;	p<Job.mParams.mParams.GetSize();	p++ )
	{
		auto Param = Job.mParams.mParams[p];
		if ( Param.GetKey() == "filter" )
			continue;

		try
		{
			Changed |= Filter->SetUniform( Param, false );
		}
		catch( std::exception& e )
		{
			Error << "Error setting uniform " << Param.GetKey() << ": " << e.what() << std::endl;
		}
	}
	if ( Changed )
		Filter->OnStagesChanged();

	TJobReply Reply(Job);
	if ( Error.str().empty() )
		Reply.mParams.AddDefaultParam("set uniforms");
	else
		Reply.mParams.AddErrorParam( Error.str() );
	
	auto& Channel = JobAndChannel.GetChannel();
	Channel.SendJobReply( Reply );
}


void TPopTrack::OnRunFilter(TJobAndChannel &JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	auto FilterName = Job.mParams.GetParamAs<std::string>("filter");
	
	auto Filter = GetFilter( FilterName );
	auto Frame = Job.mParams.GetParamAs<SoyTime>("time");
	
	TJobReply Reply(Job);
	try
	{
		Filter->Run( Frame, Reply.mParams );
	}
	catch ( std::exception& e )
	{
		std::stringstream Error;
		Error << "Error running filter: " << e.what();
		Reply.mParams.AddErrorParam( Error.str() );
	}
	
	auto& Channel = JobAndChannel.GetChannel();
	Channel.SendJobReply( Reply );
}



void TPopTrack::OnStartDecode(TJobAndChannel& JobAndChannel)
{
	auto Job = JobAndChannel.GetJob();
	TJobReply Reply( JobAndChannel );
	
	auto FilterName = Job.mParams.GetParamAs<std::string>("filter");
	auto Filename = Job.mParams.GetParamAs<std::string>("filename");

	//	create filter
	auto Filter = GetFilter( FilterName );
	if ( !Filter )
	{
		Reply.mParams.AddErrorParam( std::string("Failed to create filter ") + FilterName );
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}

	//	allocate decoder
	try
	{
		TVideoDecoderParams Params;
		Params.mFilename = Filename;
		Params.mForceNonPlanarOutput = true;
		auto Video = std::make_shared<PopVideoDecoder>( Params );

		auto OnFrame = [=](std::pair<SoyPixelsImpl*,SoyTime>& Frame)
		{
			auto Filter = GetFilter( FilterName );
			if ( !Filter )
			{
				//	ditch decoder?
				return;
			}
			
			Filter->LoadFrame( *Frame.first, Frame.second );
		};
		Video->mOnFrame.AddListener( OnFrame );
		mMovies.PushBack( Video );
	}
	catch(std::exception& e)
	{
		std::stringstream Error;
		Error << "Failed to load movie (" << Filename << ") for filter(" << FilterName << ") " << e.what();
		Reply.mParams.AddErrorParam( Error.str() );
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	Reply.mParams.AddDefaultParam( FilterName );
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}


bool TPopTrack::OnNewFrameCallback(TEventSubscriptionManager& SubscriptionManager,TJobChannelMeta Client,TVideoDevice& Device)
{
	throw Soy::AssertException("This all needs redoing");
	
	TJob OutputJob;
	auto& Reply = OutputJob;
	
	//	grab pixels
	try
	{
		/*
		auto& LastFrame = Device.GetLastFrame();
		/*
		auto& MemFile = LastFrame.mPixels->mMemFileArray;
		TYPE_MemFile MemFileData( MemFile );
		Reply.mParams.AddDefaultParam( MemFileData );
		 */
	}
	catch(std::exception& e)
	{
		//	add error if present (last frame could be out of date)
		Reply.mParams.AddErrorParam( std::string(e.what()) );
	}
	
	//	find channel, send to Client
	//	std::Debug << "Got event callback to send to " << Client << std::endl;
	
	if ( !SubscriptionManager.SendSubscriptionJob( Reply, Client ) )
		return false;
	
	return true;
}


std::shared_ptr<TPlayerFilter> TPopTrack::GetFilter(const std::string& Name)
{
	for ( int i=0;	i<mFilters.GetSize();	i++ )
	{
		if ( mFilters[i]->mName == Name )
			return mFilters[i];
	}
	
	//	make new
	std::shared_ptr<TPlayerFilter> Filter( new TPlayerFilter(Name, FILTER_MAX_FRAMES, FILTER_MAX_THREADS ) );
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
	App.mLiteralChannel.reset( new TChan<TChannelLiteral,TProtocolCli>( SoyRef("literal") ) );

	
	App.AddChannel( CommandLineChannel );
	App.AddChannel( StdioChannel );
	App.AddChannel( HttpChannel );
	App.AddChannel( App.mLiteralChannel );

	
	
	
	
	//	bootup commands via a channel
	auto BootupFilename = "Bootup_CameraLines.txt";
//	auto BootupFilename = "Bootup_LineDetection.txt";
//	auto BootupFilename = "bootup_FullscreenMatch.txt";
//	auto BootupFilename = "bootup_Crowd.txt";
	std::shared_ptr<TChannel> BootupChannel( new TChan<TChannelFileRead,TProtocolCli>( SoyRef("Bootup"), BootupFilename, true ) );
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




