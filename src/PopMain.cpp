#include "PopMain.h"
#include "SoyDebug.h"
/*
#include "TProtocolCli.h"
#include "TProtocolHttp.h"
#include "TProtocolWebSocket.h"
#include "TProtocolJson.h"
#include "TProtocolRtsp.h"
#include "TChannelPipe.h"
#include "TChannelSocket.h"
#include "TChannelFile.h"
#include "TChannelFork.h"
#include "UnitTest++/src/UnitTest++.h"
 */
#include "SoyApp.h"
#include <fstream>
#include <regex>

#if defined(TARGET_WINDOWS)
	#include <tchar.h>
#endif

//	include SoyEvent unit test
//	gr: need to improve this
//#include <SoyEvent.cpp>
//#include "SoyTest.cpp"

/*

#if defined(TARGET_WINDOWS)
TJobParams Private::DecodeArgs(int argc,_TCHAR* argv[])
#else
TJobParams Private::DecodeArgs(int argc,const char* argv[])
#endif
{
	std::stringstream CommandLine;
	for ( int i=0;	i<argc;	i++ )
	{
		std::string Arg = argv[i];
		
		//	if it contains spaces we need to put the arg in quotes
		if ( Arg.find(' ') != std::string::npos )
			CommandLine << '"' << Arg << '"';
		else
			CommandLine << Arg;
		CommandLine << ' ';
	}
	
	TJob Job;
	TProtocolCli::DecodeHeader( Job, CommandLine.str() );
	return Job.mParams;
}

*/

#if defined(TARGET_WINDOWS)
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,const char* argv[])
#endif
{
	//SoyThread::SetThreadName("Pop Main Thread", SoyThread::GetCurrentThreadNativeHandle() );
#if defined(TARGET_OSX_BUNDLE)
	return Soy::Platform::BundleAppMain( argc, argv );
#endif
	
	return PopMain();
}

