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


#define FILTER_MAX_FRAMES	5
#define FILTER_MAX_THREADS	1
#define JOB_THREAD_COUNT	0

TPopTrack::TPopTrack() :
	TJobHandler			( static_cast<TChannelManager&>(*this), JOB_THREAD_COUNT ),
	TPopJobHandler		( static_cast<TJobHandler&>(*this) ),
	mSubcriberManager	( static_cast<TChannelManager&>(*this) )
{
	//	pop movie decoder stuff
	//	add videodecoder contaienr
	mMovies.reset( new TMovieDecoderContainer() );
	mVideoCapture.AddContainer( mMovies );
	
	TParameterTraits DecodeParameterTraits;
	DecodeParameterTraits.mAssumedKeys.PushBack("filter");
	DecodeParameterTraits.mAssumedKeys.PushBack("filename");
	AddJobHandler("decode", DecodeParameterTraits, *this, &TPopTrack::OnStartDecode );
	
	AddJobHandler("list", TParameterTraits(), *this, &TPopTrack::OnList );
	TParameterTraits GetFrameTraits;
	GetFrameTraits.mAssumedKeys.PushBack("serial");
	GetFrameTraits.mRequiredKeys.PushBack("serial");
	AddJobHandler("getframe", GetFrameTraits, *this, &TPopTrack::OnGetFrame );
	
	TParameterTraits SubscribeNewFrameTraits;
	SubscribeNewFrameTraits.mAssumedKeys.PushBack("serial");
	SubscribeNewFrameTraits.mDefaultParams.PushBack( std::make_tuple(std::string("command"),std::string("newframe")) );
	AddJobHandler("subscribenewframe", SubscribeNewFrameTraits, *this, &TPopTrack::SubscribeNewFrame );

	
	
	
	

	AddJobHandler("exit", TParameterTraits(), *this, &TPopTrack::OnExit );

	TParameterTraits LoadFrameTraits;
	LoadFrameTraits.mAssumedKeys.PushBack("filter");
	LoadFrameTraits.mAssumedKeys.PushBack("time");
	LoadFrameTraits.mAssumedKeys.PushBack("filename");
	AddJobHandler("LoadFrame", LoadFrameTraits, *this, &TPopTrack::OnLoadFrame );
	
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
	
	auto Filter = GetFilter( FilterName );
	Filter->AddStage( Name, Job.mParams );
	
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
	

	TVideoDeviceMeta Meta( FilterName, Filename );
	std::stringstream Error;
	auto Device = mMovies->AllocDevice( Meta, Error );
	
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	
	if ( Device )
	{
		Reply.mParams.AddDefaultParam( FilterName );
	
		/*
		//	subscribe to the video's new frame loading
		auto OnNewFrameRelay = [Filter,this](TVideoDevice& Video)
		{
			std::stringstream LastError;
			auto& LastFrame = Video.GetLastFrame(LastError);
			if ( LastError.str().empty() )
			{
				auto pPixels = LastFrame.GetPixelsShared();
				auto& Time = LastFrame.GetTime();

				//	allow async frame processing
				static bool PushAsJob = true;
				
				if ( PushAsJob )
				{
					TJobParams Params;
					std::shared_ptr<SoyData_Stack<SoyPixels>> PixelsData( new SoyData_Stack<SoyPixels>() );
					PixelsData->mValue.Copy( *pPixels );
					
					std::stringstream TimeStr;
					TimeStr << Time;
					
					Params.AddParam( "pixels", std::dynamic_pointer_cast<SoyData>( PixelsData ) );
					Params.AddParam( "time", TimeStr.str() );
					Params.AddParam( "filter", Filter->mName );
					
					mLiteralChannel->Execute("loadframe", Params );
				}
				else
				{
					Filter->LoadFrame( pPixels, Time );
				}
			}
			else
			{
				std::Debug << "Failed to get last frame; " << LastError.str() << std::endl;
			}
		};
		Device->mOnNewFrame.AddListener( OnNewFrameRelay );
		 */
		Filter->SetOnNewVideoEvent( Device->mOnNewFrame );
	}
	 
	Reply.mParams.AddDefaultParam( FilterName );

	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}


bool TPopTrack::OnNewFrameCallback(TEventSubscriptionManager& SubscriptionManager,TJobChannelMeta Client,TVideoDevice& Device)
{
	TJob OutputJob;
	auto& Reply = OutputJob;
	
	std::stringstream Error;
	//	grab pixels
	auto& LastFrame = Device.GetLastFrame(Error);
	if ( LastFrame.IsValid() )
	{
		auto& MemFile = LastFrame.mPixels->mMemFileArray;
		TYPE_MemFile MemFileData( MemFile );
		Reply.mParams.AddDefaultParam( MemFileData );
	}
	
	//	add error if present (last frame could be out of date)
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	
	//	find channel, send to Client
	//	std::Debug << "Got event callback to send to " << Client << std::endl;
	
	if ( !SubscriptionManager.SendSubscriptionJob( Reply, Client ) )
		return false;
	
	return true;
}


void TPopTrack::OnGetFrame(TJobAndChannel& JobAndChannel)
{
	const TJob& Job = JobAndChannel;
	TJobReply Reply( JobAndChannel );
	
	auto Serial = Job.mParams.GetParamAs<std::string>("serial");
	auto AsMemFile = Job.mParams.GetParamAsWithDefault<bool>("memfile",true);
	
	std::Debug << Job.mParams << std::endl;
	
	std::stringstream Error;
	auto Device = mVideoCapture.GetDevice( Serial, Error );
	
	if ( !Device )
	{
		std::stringstream ReplyError;
		ReplyError << "Device " << Serial << " not found " << Error.str();
		Reply.mParams.AddErrorParam( ReplyError.str() );
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	//	grab pixels
	auto& LastFrame = Device->GetLastFrame( Error );
	if ( LastFrame.IsValid() )
	{
		if ( AsMemFile )
		{
			TYPE_MemFile MemFile( LastFrame.mPixels->mMemFileArray );
			TJobFormat Format;
			Format.PushFirstContainer<SoyPixels>();
			Format.PushFirstContainer<TYPE_MemFile>();
			Reply.mParams.AddDefaultParam( MemFile, Format );
		}
		else
		{
			SoyPixels Pixels;
			Pixels.Copy( *LastFrame.mPixels );
			Reply.mParams.AddDefaultParam( Pixels );
		}
	}
	
	//	add error if present (last frame could be out of date)
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	
	//	add other stats
	auto FrameRate = Device->GetFps();
	auto FrameMs = Device->GetFrameMs();
	Reply.mParams.AddParam("fps", FrameRate);
	Reply.mParams.AddParam("framems", FrameMs );
	Reply.mParams.AddParam("serial", Device->GetMeta().mSerial );
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
	
}

//	copied from TPopCapture::OnListDevices
void TPopTrack::OnList(TJobAndChannel& JobAndChannel)
{
	TJobReply Reply( JobAndChannel );
	
	Array<TVideoDeviceMeta> Metas;
	mVideoCapture.GetDevices( GetArrayBridge(Metas) );
	
	std::stringstream MetasString;
	for ( int i=0;	i<Metas.GetSize();	i++ )
	{
		auto& Meta = Metas[i];
		if ( i > 0 )
			MetasString << ",";
		
		MetasString << Meta;
	}
	
	if ( !MetasString.str().empty() )
		Reply.mParams.AddDefaultParam( MetasString.str() );
	else
		Reply.mParams.AddErrorParam("No devices found");
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}


void TPopTrack::SubscribeNewFrame(TJobAndChannel& JobAndChannel)
{
	const TJob& Job = JobAndChannel;
	TJobReply Reply( JobAndChannel );
	
	std::stringstream Error;
	
	//	get device
	auto Serial = Job.mParams.GetParamAs<std::string>("serial");
	auto Device = mVideoCapture.GetDevice( Serial, Error );
	if ( !Device )
	{
		std::stringstream ReplyError;
		ReplyError << "Device " << Serial << " not found " << Error.str();
		Reply.mParams.AddErrorParam( ReplyError.str() );
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	//	create new subscription for it
	//	gr: determine if this already exists!
	auto EventName = Job.mParams.GetParamAs<std::string>("command");
	auto Event = mSubcriberManager.AddEvent( Device->mOnNewFrame, EventName, Error );
	if ( !Event )
	{
		std::stringstream ReplyError;
		ReplyError << "Failed to create new event " << EventName << ". " << Error.str();
		Reply.mParams.AddErrorParam( ReplyError.str() );
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	//	make a lambda to recieve the event
	auto Client = Job.mChannelMeta;
	TEventSubscriptionCallback<TVideoDevice> ListenerCallback = [this,Client](TEventSubscriptionManager& SubscriptionManager,TVideoDevice& Value)
	{
		return this->OnNewFrameCallback( SubscriptionManager, Client, Value );
	};
	
	//	subscribe this caller
	if ( !Event->AddSubscriber( Job.mChannelMeta, ListenerCallback, Error ) )
	{
		std::stringstream ReplyError;
		ReplyError << "Failed to add subscriber to event " << EventName << ". " << Error.str();
		Reply.mParams.AddErrorParam( ReplyError.str() );
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	
	std::stringstream ReplyString;
	ReplyString << "OK subscribed to " << EventName;
	Reply.mParams.AddDefaultParam( ReplyString.str() );
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	Reply.mParams.AddParam("eventcommand", EventName);
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
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
	std::shared_ptr<TChannel> BootupChannel( new TChan<TChannelFileRead,TProtocolCli>( SoyRef("Bootup"), "bootup.txt", true ) );
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




