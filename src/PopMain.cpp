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
#include "SoyLib/src/SoyFilesystem.h"


#if defined(TARGET_WINDOWS)
	#include <tchar.h>
#endif


namespace Pop
{
	std::string ProjectPath;
}


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

namespace Platform
{
#if defined(TARGET_WINDOWS)
	namespace Private
	{
		extern HINSTANCE InstanceHandle;
	}
#endif
}


int PopEngine(const char* ProjectPath)
{
	Pop::ProjectPath = ProjectPath;
	return Soy::Platform::BundleAppMain();
}


extern "C" int main(int argc,const char* argv[])
{
	if ( argc < 2 )
		return PopEngine(nullptr);
	
	return PopEngine( argv[1] );
}


//	define winmain AND main for gui & console subsystem builds
#if defined(TARGET_WINDOWS)
//int _tmain(int argc, _TCHAR* argv[])
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	char ExePath[MAX_PATH];
	GetModuleFileName(NULL, ExePath, MAX_PATH);
	Platform::SetDllPath(ExePath);

	Platform::Private::InstanceHandle = hInstance;
	const char* argv[2] = { ExePath, lpCmdLine };
	int argc = 2;

	return main(argc, argv);
}
#endif

