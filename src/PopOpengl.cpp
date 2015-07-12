#include "PopOpengl.h"
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
#include "TTextureWindow.h"
#include <TChannelFile.h>


TPopOpengl::TPopOpengl() :
	TJobHandler		( static_cast<TChannelManager&>(*this) ),
	TPopJobHandler	( static_cast<TJobHandler&>(*this) )
{
	AddJobHandler("exit", TParameterTraits(), *this, &TPopOpengl::OnExit );

	TParameterTraits MakeWindowTraits;
	MakeWindowTraits.mDefaultParams.PushBack( std::make_tuple(std::string("name"),std::string("gl") ) );
	AddJobHandler("makewindow", MakeWindowTraits, *this, &TPopOpengl::OnMakeWindow );
	
	TParameterTraits MakeRenderTargetTraits;
	MakeRenderTargetTraits.mRequiredKeys.PushBack("name");
	MakeRenderTargetTraits.mRequiredKeys.PushBack("width");
	MakeRenderTargetTraits.mRequiredKeys.PushBack("height");
	AddJobHandler("makerendertarget", MakeRenderTargetTraits, *this, &TPopOpengl::OnMakeRenderTarget );

	TParameterTraits ClearRenderTargetTraits;
	AddJobHandler("clearrendertarget", ClearRenderTargetTraits, *this, &TPopOpengl::OnClearRenderTarget );
}

bool TPopOpengl::AddChannel(std::shared_ptr<TChannel> Channel)
{
	if ( !TChannelManager::AddChannel( Channel ) )
		return false;
	if ( !Channel )
		return false;
	TJobHandler::BindToChannel( *Channel );
	return true;
}


void TPopOpengl::OnExit(TJobAndChannel& JobAndChannel)
{
	mConsoleApp.Exit();
	
	//	should probably still send a reply
	TJobReply Reply( JobAndChannel );
	Reply.mParams.AddDefaultParam(std::string("exiting..."));
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}


void TPopOpengl::OnMakeWindow(TJobAndChannel &JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	auto Name = Job.mParams.GetParamAs<std::string>("name");

	vec2f Pos( 100,100 );
	vec2f Size( 400, 400 );
	std::shared_ptr<TTextureWindow> pWindow( new TTextureWindow(Name,Pos,Size,*this) );
	if ( !pWindow->IsValid() )
	{
		TJobReply Reply(Job);
		Reply.mParams.AddErrorParam("failed to create window");
		auto& Channel = JobAndChannel.GetChannel();
		Channel.SendJobReply( Reply );
		return;
	}

	mWindows.PushBack( pWindow );
	TJobReply Reply(Job);
	Reply.mParams.AddDefaultParam("OK");
//	if ( !Error.str().empty() )
//		Reply.mParams.AddErrorParam( Error.str() );
	
	auto& Channel = JobAndChannel.GetChannel();
	Channel.SendJobReply( Reply );
}

void TPopOpengl::OnMakeRenderTarget(TJobAndChannel& JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	Opengl::TFboMeta Meta;
	Meta.mName = Job.mParams.GetParamAs<std::string>("name");
	Meta.mSize.x = Job.mParams.GetParamAs<int>("width");
	Meta.mSize.y = Job.mParams.GetParamAs<int>("height");
	
	//	get context
	auto Context = GetContext( Job.mParams.GetParamAs<std::string>("context") );
	if ( !Context )
	{
		TJobReply Reply(Job);
		Reply.mParams.AddErrorParam("Could not get gl context");
		auto& Channel = JobAndChannel.GetChannel();
		Channel.SendJobReply( Reply );
		return;
	}
	
	
	std::shared_ptr<Opengl::TRenderTargetFbo> RenderTarget = GetRenderTarget( Meta.mName );
	
	std::stringstream ReplyString;
	if ( !RenderTarget )
	{
		//	gr: make this block (RAII), flush the context's commands and catch the callback event
		RenderTarget.reset( new Opengl::TRenderTargetFbo(Meta,*Context) );
		mRenderTargets.PushBack( RenderTarget );
		ReplyString << "Created new render target " << Meta.mName << std::endl;
	}
	else
	{
		ReplyString << "Render target " << Meta.mName << " already exists" << std::endl;
	}
	
	TJobReply Reply(Job);
	Reply.mParams.AddDefaultParam( ReplyString.str() );

	auto& Channel = JobAndChannel.GetChannel();
	Channel.SendJobReply( Reply );
}

void TPopOpengl::OnClearRenderTarget(TJobAndChannel& JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	auto Name = Job.mParams.GetParamAs<std::string>("name");
	auto RgbStr = Job.mParams.GetParamAs<std::string>("rgb");
	vec3f Rgb3(1,0,0);
	Soy::StringToType( Rgb3, RgbStr );
	Soy::TRgb Rgb( Rgb3.x, Rgb3.y, Rgb3.z );

	//	get context
	auto Context = GetContext( Job.mParams.GetParamAs<std::string>("context") );
	if ( !Context )
	{
		TJobReply Reply(Job);
		Reply.mParams.AddErrorParam("Could not get gl context");
		auto& Channel = JobAndChannel.GetChannel();
		Channel.SendJobReply( Reply );
		return;
	}
	
	auto RenderTarget = GetRenderTarget(Name);
	if ( !RenderTarget )
	{
		TJobReply Reply(Job);
		Reply.mParams.AddErrorParam(std::string("Could not find render target ")+Name);
		auto& Channel = JobAndChannel.GetChannel();
		Channel.SendJobReply( Reply );
		return;
	}
	
	//	queue some commands
	auto BindAndClear = [&Rgb,&RenderTarget]()
	{
		RenderTarget->Bind();
		glClearColor( Rgb.r(), Rgb.g(), Rgb.b(), 1 );
		glClear( GL_COLOR_BUFFER_BIT );
		RenderTarget->Unbind();
		
		return true;
	};
	
	Context->PushJob( BindAndClear );
	
	//	flush so lambda variables dont go out of scope
	Context->FlushJobs();

	TJobReply Reply(Job);
	Reply.mParams.AddDefaultParam("Cleared!");
	auto& Channel = JobAndChannel.GetChannel();
	Channel.SendJobReply( Reply );
}

std::shared_ptr<Opengl::TRenderTargetFbo> TPopOpengl::GetRenderTarget(const std::string& Name)
{
	for ( int i=0;	i<mRenderTargets.GetSize();	i++ )
	{
		//	todo: replace if new params
		if ( mRenderTargets[i]->mName == Name )
			return mRenderTargets[i];
	}
	
	return nullptr;
}


Opengl::TContext* TPopOpengl::GetContext(const std::string& Name)
{
	if ( mWindows.IsEmpty() )
		return nullptr;
	
	for ( int i=0;	i<mWindows.GetSize();	i++ )
	{
		//auto& Window = *mWindows[i];
		//	if name == Name
	}
	
	return mWindows[0]->GetContext();
}

//	keep alive after PopMain()
#if defined(TARGET_OSX_BUNDLE)
std::shared_ptr<TPopOpengl> gOpenglApp;
#endif


TPopAppError::Type PopMain(TJobParams& Params)
{
	std::cout << Params << std::endl;
	
	gOpenglApp.reset( new TPopOpengl );
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




